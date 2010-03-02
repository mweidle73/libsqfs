#include "bulkdata.h"
#include "internal.h"

#include <assert.h>

static inline void
libsqfs_chunk_workq_init(libsqfs_chunk_workq * workq)
{
	workq->first = workq->last = 0;
}

static inline void
libsqfs_chunk_workq_push_back(libsqfs_chunk_workq * workq, libsqfs_chunk * chunk)
{
	chunk->workq_prev = workq->last;
	chunk->workq_next = 0;
	if (workq->last) workq->last->workq_next = chunk;
	else workq->first = chunk;
	workq->last = chunk;
}

static inline void
libsqfs_chunk_workq_push_front(libsqfs_chunk_workq * workq, libsqfs_chunk * chunk)
{
	chunk->workq_prev = 0;
	chunk->workq_next = workq->first;
	if (workq->first) workq->first->workq_prev = chunk;
	else workq->last = chunk;
	workq->first = chunk;
}

static inline void
libsqfs_chunk_workq_remove(libsqfs_chunk_workq * workq, libsqfs_chunk * chunk)
{
	if (chunk->workq_prev) chunk->workq_prev->workq_next = chunk->workq_next;
	else workq->first = chunk->workq_next;
	if (chunk->workq_next) chunk->workq_next->workq_prev = chunk->workq_prev;
	else workq->last = chunk->workq_prev;
}

static inline libsqfs_chunk *
libsqfs_chunk_workq_pop(libsqfs_chunk_workq * workq)
{
	libsqfs_chunk * chunk = workq->first;
	if (chunk) {
		if (chunk->workq_next) chunk->workq_next->workq_prev = 0;
		else workq->last = 0;
		workq->first = chunk->workq_next;
	}
	return chunk;
}

/* chunk base class */

struct _libsqfs_chunk_vmt {
	/* steps to be performed for chunks -- note that some may be skipped  */
	bool (*read)(libsqfs_chunk * chunk, libsqfs_image_t image);
	bool (*deduplicate)(libsqfs_chunk * chunk);
	bool (*assign)(libsqfs_chunk * chunk, libsqfs_image_t image);
	bool (*compress)(libsqfs_chunk * chunk, libsqfs_compressor_instance *ci, libsqfs_image_t image);
	bool (*write)(libsqfs_chunk * chunk, libsqfs_image_t image);
	void (*destroy)(libsqfs_chunk * chunk);
};

static inline void
libsqfs_chunk_init(libsqfs_chunk * chunk, const libsqfs_chunk_vmt * vmt, libsqfs_bulkdata * bulkdata, libsqfs_data_piece data)
{
	chunk->vmt = vmt;
	chunk->bulkdata = bulkdata;
	chunk->prev = chunk->next = 0;
	chunk->state = libsqfs_chunk_none;
	chunk->claimed_by_thread = false;
	chunk->data = data;
	chunk->cached_data = 0;
}

static inline void
libsqfs_chunk_fini(libsqfs_chunk * chunk)
{
	if (chunk->cached_data) free(chunk->cached_data);
}

static inline void
libsqfs_chunk_destroy(libsqfs_chunk * chunk)
{
	chunk->vmt->destroy(chunk);
}

static bool
libsqfs_chunk_do_read(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_none);
	pthread_mutex_unlock(&chunk->bulkdata->lock);
	
	assert(chunk->data.size);
	chunk->cached_data = malloc(chunk->data.size);
	if (!chunk->cached_data) {
		libsqfs_image_out_of_memory(image);
		pthread_mutex_lock(&chunk->bulkdata->lock);
		return false;
	}
	
	ssize_t count = libsqfs_data_pread(chunk->data.data, chunk->cached_data, chunk->data.size, chunk->data.offset);
	
	if (count != chunk->data.size) {
		/* FIXME: this error could be more descriptive */
		libsqfs_image_flag_error(image, "Read failed");
		pthread_mutex_lock(&chunk->bulkdata->lock);
		return false;
	}
	
	pthread_mutex_lock(&chunk->bulkdata->lock);
	chunk->state = libsqfs_chunk_read;
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->dedupq, chunk);
	return true;
}

static bool
libsqfs_chunk_do_deduplicate(libsqfs_chunk * chunk)
{
	assert(chunk->state == libsqfs_chunk_read);
	chunk->state = libsqfs_chunk_deduplicated;
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->assignq, chunk);
	return true;
}

static bool
libsqfs_chunk_do_assign(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_deduplicated);
	chunk->state = libsqfs_chunk_assigned;
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->compressq, chunk);
	return true;
}

static void
libsqfs_chunk_try_writeout(libsqfs_chunk * chunk)
{
	if (chunk->state != libsqfs_chunk_writable) return;
	if (chunk->prev && chunk->prev->state != libsqfs_chunk_finished) return;
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->writeq, chunk);
}

static void
libsqfs_chunk_mark_finished(libsqfs_chunk * chunk)
{
	assert(!chunk->prev || chunk->prev->state == libsqfs_chunk_finished);
	
	chunk->state = libsqfs_chunk_finished;
	
	if (chunk->next && chunk->next->state == libsqfs_chunk_writable)
		libsqfs_chunk_workq_push_front(&chunk->bulkdata->writeq, chunk->next);
	
	chunk->bulkdata->chunks.finished ++;
	if (chunk->bulkdata->chunks.finished == chunk->bulkdata->chunks.count)
		pthread_cond_broadcast(&chunk->bulkdata->cond);
}


/* bulk data blocks as stored in the target image */

static inline void
libsqfs_image_block_init(libsqfs_image_block * dst)
{
	dst->offset = -1;
	dst->size = 0;
	dst->data = 0;
	dst->compressed = false;
}

static inline void
libsqfs_image_block_fini(libsqfs_image_block * dst)
{
	if (dst->data) free(dst->data);
}

static bool
libsqfs_image_block_compress(libsqfs_image_block * dst, const void * src, size_t size, libsqfs_compressor_instance * ci)
{
	dst->data = malloc(size);
	if (!size) return false;
	
	ssize_t compressed_size = 
	libsqfs_compressor_instance_compress(ci, dst->data, size, src, size);
	
	if (compressed_size != -1) {
		dst->compressed = true;
		dst->size = compressed_size;
	} else {
		dst->compressed = false;
		dst->size = size;
		memcpy(dst->data, src, size);
	}
	return true;
}

static bool
libsqfs_image_block_write(libsqfs_chunk * chunk, libsqfs_image_block * dst, libsqfs_image_t image)
{
	dst->offset = libsqfs_image_reserve(image, dst->size);
	pthread_mutex_unlock(&chunk->bulkdata->lock);
	
	ssize_t count = libsqfs_pwrite(image->dst, dst->data, dst->size, dst->offset);
	if (count != dst->size) {
		/* FIXME: this error could be more descriptive */
		libsqfs_image_flag_error(image, "Write failed");
		pthread_mutex_lock(&chunk->bulkdata->lock);
		return false;
	}
	
	free(dst->data);
	dst->data = 0;
	
	pthread_mutex_lock(&chunk->bulkdata->lock);
	
	return true;
}

/* fragment blocks, aggregating multiple fragment pieces */

static inline void
libsqfs_fragment_block_destroy(libsqfs_fragment_block * frag_block)
{
	libsqfs_image_block_fini(&frag_block->dst);
	free(frag_block);
}

static libsqfs_fragment_block *
libsqfs_fragment_block_create(libsqfs_bulkdata * bd)
{
	libsqfs_fragment_block * frag_block = malloc(sizeof(*frag_block));
	if (!frag_block) return 0;
	
	frag_block->pieces.first = frag_block->pieces.last = 0;
	frag_block->size = 0;
	
	frag_block->index = bd->fragment_blocks.count ++;
	frag_block->prev = bd->fragment_blocks.last;
	frag_block->next = 0;
	if (bd->fragment_blocks.last) bd->fragment_blocks.last->next = frag_block;
	else bd->fragment_blocks.first = frag_block;
	bd->fragment_blocks.last = frag_block;
	
	libsqfs_image_block_init(&frag_block->dst);
	
	return frag_block;
}

static void
libsqfs_fragment_block_add_piece(libsqfs_fragment_block * frag_block, libsqfs_fragment_piece * piece)
{
	piece->fragment_block = frag_block;
	piece->offset = frag_block->size;
	piece->prev_in_block = frag_block->pieces.last;
	piece->next_in_block = 0;
	if (frag_block->pieces.last) frag_block->pieces.last->next_in_block = piece;
	else frag_block->pieces.first = piece;
	frag_block->pieces.last = piece;
	frag_block->size += piece->data.size;
	
}

/* fragment pieces */

static bool
libsqfs_fragment_piece_deduplicate(libsqfs_chunk * chunk)
{
	assert(chunk->state == libsqfs_chunk_read);
	/* FIXME: really perform deduplication here */
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	
	piece->state = libsqfs_chunk_deduplicated;
	
	/* if previous piece has not been assigned to any block,
	abort here as we cannot make an assignment decision yet */
	if (piece->prev_piece && piece->prev_piece->state < libsqfs_chunk_assigned)
		return true;
	
	/* otherwise, move to assignment queue */
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->assignq, chunk);
	return true;
}

static bool
libsqfs_fragment_piece_assign(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_deduplicated);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	
	libsqfs_fragment_block * frag_block = 0;
	
	if (piece->prev_piece) {
		frag_block = piece->prev_piece->fragment_block;
		if (frag_block->size + piece->data.size > image->options.block_size) {
			/* adding this piece would overflow the previous fragment block;
			schedule compression and write-out for the previous block, and
			create a new block starting with this element */
			piece->flush_previous_block = true;
			frag_block = 0;
		}
	}
	
	if (!frag_block) {
		frag_block = libsqfs_fragment_block_create(piece->bulkdata);
		if (!frag_block) {
			pthread_mutex_unlock(&piece->bulkdata->lock);
			libsqfs_image_out_of_memory(image);
			pthread_mutex_lock(&piece->bulkdata->lock);
			return false;
		}
	}
	
	libsqfs_fragment_block_add_piece(frag_block, piece);
	if (piece->flush_previous_block) {
		piece->state = libsqfs_chunk_assigned;	
		libsqfs_chunk_workq_push_front(&piece->bulkdata->compressq, chunk);
	} else {
		piece->state = libsqfs_chunk_writable;
		libsqfs_chunk_try_writeout(chunk);
	}
	
	if (piece->next_piece && piece->next_piece->state == libsqfs_chunk_deduplicated) {
		libsqfs_chunk_workq_push_front(&piece->bulkdata->assignq, (libsqfs_chunk *)piece->next_piece);
	}
	
	return true;
}

static bool
libsqfs_fragment_piece_compress(libsqfs_chunk * chunk, libsqfs_compressor_instance *ci, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_assigned);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	assert(piece->flush_previous_block);
	libsqfs_fragment_block * frag_block = piece->prev_piece->fragment_block;
	
	pthread_mutex_unlock(&piece->bulkdata->lock);
	
	char buffer[frag_block->size];
	size_t pos = 0;
	libsqfs_fragment_piece * p = frag_block->pieces.first;
	while(p) {
		memcpy(buffer+pos, p->cached_data, p->data.size);
		pos += p->data.size;
		
		/* FIXME: maybe keep for deduplication */
		free(p->cached_data);
		p->cached_data = 0;
		
		p = p->next_in_block;
	}
	
	if (!libsqfs_image_block_compress(&frag_block->dst, buffer, frag_block->size, ci)) {
		libsqfs_image_out_of_memory(image);
		pthread_mutex_lock(&piece->bulkdata->lock);
		return false;
	}
	
	pthread_mutex_lock(&piece->bulkdata->lock);
	piece->state = libsqfs_chunk_writable;
	libsqfs_chunk_try_writeout(chunk);
	return true;
}

static bool
libsqfs_fragment_piece_write(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_writable);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	if (piece->flush_previous_block) {
		libsqfs_fragment_block * frag_block = piece->prev_piece->fragment_block;
		if (!libsqfs_image_block_write(chunk, &frag_block->dst, image)) return false;
	}
	libsqfs_chunk_mark_finished(chunk);
	
	return true;
}

static void
libsqfs_fragment_piece_destroy(libsqfs_chunk * chunk)
{
	libsqfs_chunk_fini(chunk);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	free(piece);
}

static const libsqfs_chunk_vmt libsqfs_fragment_piece_vmt = {
	.destroy = &libsqfs_fragment_piece_destroy,
	.read = &libsqfs_chunk_do_read,
	.deduplicate = &libsqfs_fragment_piece_deduplicate,
	.assign = &libsqfs_fragment_piece_assign,
	.compress = &libsqfs_fragment_piece_compress,
	.write = &libsqfs_fragment_piece_write
};

static libsqfs_fragment_piece *
libsqfs_fragment_piece_create(libsqfs_bulkdata * bd, libsqfs_data_piece data)
{
	libsqfs_fragment_piece * piece = malloc(sizeof(*piece));
	if (!piece) return 0;
	
	libsqfs_chunk_init((libsqfs_chunk *) piece, &libsqfs_fragment_piece_vmt, bd, data);
	piece->prev_piece = piece->next_piece = 0;
	piece->prev_in_block = piece->next_in_block = 0;
	piece->offset = -1;
	piece->fragment_block = 0;
	piece->flush_previous_block = false;
	
	return piece;
}

static bool
libsqfs_closing_piece_read(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	chunk->state = libsqfs_chunk_read;
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->dedupq, chunk);
	return true;
}

static bool
libsqfs_closing_piece_deduplicate(libsqfs_chunk * chunk)
{
	assert(chunk->state == libsqfs_chunk_read);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	
	piece->state = libsqfs_chunk_deduplicated;
	
	/* if previous piece has not been assigned to any block,
	abort here as we cannot make an assignment decision yet */
	if (piece->prev_piece && piece->prev_piece->state < libsqfs_chunk_assigned)
		return true;
	
	/* otherwise, move to assignment queue */
	libsqfs_chunk_workq_push_front(&chunk->bulkdata->assignq, chunk);
	return true;
}

static bool
libsqfs_closing_piece_assign(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_deduplicated);
	libsqfs_fragment_piece * piece = (libsqfs_fragment_piece *) chunk;
	
	if (piece->prev_piece) {
		piece->flush_previous_block = true;
	}
	
	if (piece->flush_previous_block) {
		piece->state = libsqfs_chunk_assigned;
		libsqfs_chunk_workq_push_front(&piece->bulkdata->compressq, chunk);
	} else {
		piece->state = libsqfs_chunk_writable;
		libsqfs_chunk_mark_finished(chunk);
	}
	return true;
}

static const libsqfs_chunk_vmt libsqfs_closing_piece_vmt = {
	.destroy = &libsqfs_fragment_piece_destroy,
	.read = &libsqfs_closing_piece_read,
	.deduplicate = &libsqfs_closing_piece_deduplicate,
	.assign = &libsqfs_closing_piece_assign,
	.compress = &libsqfs_fragment_piece_compress,
	.write = &libsqfs_fragment_piece_write
};

static libsqfs_fragment_piece *
libsqfs_closing_piece_create(libsqfs_bulkdata * bd)
{
	libsqfs_fragment_piece * piece = malloc(sizeof(*piece));
	if (!piece) return 0;
	
	libsqfs_data_piece data = {};
	libsqfs_chunk_init((libsqfs_chunk *) piece, &libsqfs_closing_piece_vmt, bd, data);
	piece->prev_piece = piece->next_piece = 0;
	piece->prev_in_block = piece->next_in_block = 0;
	piece->offset = -1;
	piece->fragment_block = 0;
	piece->flush_previous_block = false;
	
	return piece;
}

/* full blocks */

static bool
libsqfs_full_block_compress(libsqfs_chunk * chunk, libsqfs_compressor_instance *ci, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_assigned);
	libsqfs_full_block * block = (libsqfs_full_block *) chunk;
	
	pthread_mutex_unlock(&block->bulkdata->lock);
	
	if (!libsqfs_image_block_compress(&block->dst, block->cached_data, block->data.size, ci)) {
		libsqfs_image_out_of_memory(image);
		pthread_mutex_lock(&block->bulkdata->lock);
		return false;
	}
	
	/* FIXME: possibly preserve for deduplication */
	free(block->cached_data);
	block->cached_data = 0;
	
	pthread_mutex_lock(&block->bulkdata->lock);
	block->state = libsqfs_chunk_writable;
	libsqfs_chunk_try_writeout(chunk);
	return true;
}

static bool
libsqfs_full_block_write(libsqfs_chunk * chunk, libsqfs_image_t image)
{
	assert(chunk->state == libsqfs_chunk_writable);
	libsqfs_full_block * block = (libsqfs_full_block *) chunk;
	if (!libsqfs_image_block_write(chunk, &block->dst, image)) return false;
	
	libsqfs_chunk_mark_finished(chunk);
	return true;
}

static void
libsqfs_full_block_destroy(libsqfs_chunk * chunk)
{
	libsqfs_chunk_fini(chunk);
	
	libsqfs_full_block * block = (libsqfs_full_block *) chunk;
	libsqfs_image_block_fini(&block->dst);
	free(block);
}

static const libsqfs_chunk_vmt libsqfs_full_block_vmt = {
	.destroy = &libsqfs_full_block_destroy,
	.read = &libsqfs_chunk_do_read,
	.deduplicate = &libsqfs_chunk_do_deduplicate,
	.assign = &libsqfs_chunk_do_assign,
	.compress = &libsqfs_full_block_compress,
	.write = &libsqfs_full_block_write
};

static libsqfs_full_block * 
libsqfs_full_block_create(libsqfs_bulkdata * bd, libsqfs_data_piece data)
{
	libsqfs_full_block * block = malloc(sizeof(*block));
	if (!block) return 0;
	
	libsqfs_chunk_init((libsqfs_chunk *) block, &libsqfs_full_block_vmt, bd, data);
	libsqfs_image_block_init(&block->dst);
	
	return block;
}

void
libsqfs_bulkdata_init(libsqfs_bulkdata * bd)
{
	bd->chunks.first = bd->chunks.last = 0;
	bd->chunks.count = bd->chunks.finished = 0;
	bd->fragment_blocks.first = bd->fragment_blocks.last = 0;
	bd->fragment_blocks.count = 0;
	bd->fragment_pieces.first = bd->fragment_pieces.last = 0;
	
	libsqfs_chunk_workq_init(&bd->readq);
	libsqfs_chunk_workq_init(&bd->dedupq);
	libsqfs_chunk_workq_init(&bd->assignq);
	libsqfs_chunk_workq_init(&bd->compressq);
	libsqfs_chunk_workq_init(&bd->writeq);
	
	bd->may_add_chunks = true;
	
	pthread_mutex_init(&bd->lock, 0);
	pthread_cond_init(&bd->cond, 0);
	
	bd->frag_table_loc = -1;
}

void
libsqfs_bulkdata_fini(libsqfs_bulkdata * bd)
{
	libsqfs_chunk * chunk = bd->chunks.first;
	while(chunk) {
		libsqfs_chunk * next = chunk->next;
		libsqfs_chunk_destroy(chunk);
		chunk = next;
	}
	
	libsqfs_fragment_block * frag = bd->fragment_blocks.first;
	while(frag) {
		libsqfs_fragment_block * next = frag->next;
		libsqfs_fragment_block_destroy(frag);
		frag = next;
	}
}

#if 0
static void dump_queue(const char * name, libsqfs_chunk_workq * workq)
{
	printf("%s:", name);
	libsqfs_chunk * chunk= workq->first;
	while(chunk) {
		printf(" %p", chunk);
		assert(chunk != chunk->workq_next);
		chunk = chunk->workq_next;
	}
	printf("\n");
}
#endif

/* pick the first chunk not yet claimed by any other thread, and work on it
as far as possible */
static bool
libsqfs_bulkdata_process_single_locked(libsqfs_bulkdata * bd, libsqfs_compressor_instance * ci, libsqfs_image_t image)
{
	libsqfs_chunk * chunk;
	
#if 0
	printf("\n");
	chunk = bd->chunks.first;
	while(chunk) {
		printf("%p(%s:%d) ", chunk, 
			(chunk->vmt == &libsqfs_fragment_piece_vmt) ? "frag" : "blck",
			chunk->state);
		chunk = chunk->next;
	}
	
	printf("\n");
	dump_queue("writeq", &bd->writeq);
	dump_queue("compressq", &bd->compressq);
	dump_queue("assignq", &bd->assignq);
	dump_queue("dedupq", &bd->dedupq);
	dump_queue("readq", &bd->readq);
	fflush(stdout);
#endif
	
	chunk = libsqfs_chunk_workq_pop(&bd->writeq);
	if (chunk) return chunk->vmt->write(chunk, image);
	
	chunk = libsqfs_chunk_workq_pop(&bd->compressq);
	if (chunk) return chunk->vmt->compress(chunk, ci, image);
	
	chunk = libsqfs_chunk_workq_pop(&bd->assignq);
	if (chunk) return chunk->vmt->assign(chunk, image);
	
	chunk = libsqfs_chunk_workq_pop(&bd->dedupq);
	if (chunk) return chunk->vmt->deduplicate(chunk);
	
	chunk = libsqfs_chunk_workq_pop(&bd->readq);
	if (chunk) return chunk->vmt->read(chunk, image);
	
	return false;
}

void
libsqfs_bulkdata_process(libsqfs_bulkdata * bd, libsqfs_compressor_instance * ci, libsqfs_image_t image)
{
	pthread_mutex_lock(&bd->lock);
	while(true) {
		if (libsqfs_bulkdata_process_single_locked(bd, ci, image)) continue;
		if (bd->chunks.finished == bd->chunks.count && !bd->may_add_chunks) break;
		pthread_cond_wait(&bd->cond, &bd->lock);
	}
	pthread_mutex_unlock(&bd->lock);
}

static void
libsqfs_bulkdata_enqueue_chunk_locked(libsqfs_bulkdata * bd, libsqfs_chunk * chunk)
{
	chunk->prev = bd->chunks.last;
	chunk->next = 0;
	if (bd->chunks.last) bd->chunks.last->next = chunk;
	else bd->chunks.first = chunk;
	bd->chunks.last = chunk;
	
	chunk->index = bd->chunks.count ++ ;
	
	libsqfs_chunk_workq_push_back(&bd->readq, chunk);
	pthread_cond_signal(&bd->cond);
}

static void
libsqfs_bulkdata_enqueue_chunk(libsqfs_bulkdata * bd, libsqfs_chunk * chunk)
{
	pthread_mutex_lock(&bd->lock);
	libsqfs_bulkdata_enqueue_chunk_locked(bd, chunk);
	pthread_mutex_unlock(&bd->lock);
}

static void
libsqfs_bulkdata_enqueue_piece_locked(libsqfs_bulkdata * bd, libsqfs_fragment_piece * piece)
{
	libsqfs_bulkdata_enqueue_chunk_locked(bd, (libsqfs_chunk *) piece);
	piece->prev_piece = bd->fragment_pieces.last;
	piece->next_piece = 0;
	if (bd->fragment_pieces.last) bd->fragment_pieces.last->next_piece = piece;
	else bd->fragment_pieces.first = piece;
	bd->fragment_pieces.last = piece;
}

static void
libsqfs_bulkdata_enqueue_piece(libsqfs_bulkdata * bd, libsqfs_fragment_piece * piece)
{
	pthread_mutex_lock(&bd->lock);
	libsqfs_bulkdata_enqueue_piece_locked(bd, piece);
	pthread_mutex_unlock(&bd->lock);
}

bool
libsqfs_bulkdata_seal(libsqfs_bulkdata * bd)
{
	pthread_mutex_lock(&bd->lock);
	if (bd->may_add_chunks) {
		pthread_mutex_unlock(&bd->lock);
		
		libsqfs_fragment_piece * piece = libsqfs_closing_piece_create(bd);
		if (!piece) return false;
		libsqfs_bulkdata_enqueue_piece(bd, piece);
		
		pthread_mutex_lock(&bd->lock);
		bd->may_add_chunks = false;
		pthread_cond_signal(&bd->cond);
	}
	pthread_mutex_unlock(&bd->lock);
	
	return true;
}

bool
libsqfs_bulkdata_finish(libsqfs_bulkdata * bd, libsqfs_image_t image)
{
	libsqfs_bulkdata_seal(bd);
	libsqfs_bulkdata_process(bd, image->compressor, image);
	
	return true;
}

/* create and write fragment table */
bool
libsqfs_bulkdata_write_fragment_table(libsqfs_bulkdata * bd, libsqfs_image_t image)
{
	if (!bd->fragment_blocks.count) {
		bd->frag_table_loc = libsqfs_image_reserve(image, 0);
		return true;
	}
	
	struct squashfs_fragment_entry entries[bd->fragment_blocks.count];
	
	size_t n = 0;
	libsqfs_fragment_block * fragment = bd->fragment_blocks.first;
	while(fragment) {
		entries[n].start_block = cpu_to_le64(fragment->dst.offset);
		uint32_t value = fragment->dst.size;
		if (!fragment->dst.compressed)
			value |= SQUASHFS_COMPRESSED_BIT_BLOCK;
		entries[n].size = cpu_to_le32(value);
		entries[n].unused = 0;
		
		fragment = fragment->next;
		n ++;
	}
	
	bd->frag_table_loc = libsqfs_write_metatable(image, entries, sizeof(entries),
		false, true);
	
	return bd->frag_table_loc != -1;
}

libsqfs_full_block *
libsqfs_bulkdata_sumbit(libsqfs_bulkdata * bd, libsqfs_data_piece data)
{
	libsqfs_full_block * block = libsqfs_full_block_create(bd, data);
	if (!block) return 0;
	
	libsqfs_bulkdata_enqueue_chunk(bd, (libsqfs_chunk *) block);
	
	return block;
}

libsqfs_fragment_piece *
libsqfs_bulkdata_submit_fragment(libsqfs_bulkdata * bd, libsqfs_data_piece src)
{
	libsqfs_fragment_piece * piece = libsqfs_fragment_piece_create(bd, src);
	if (!piece) return 0;
	
	libsqfs_bulkdata_enqueue_piece(bd, piece);
	
	return piece;
}
