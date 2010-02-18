#include "internal.h"

#include "squashfs_fs.h"

static void
libsqfs_fragment_piece_destroy(libsqfs_fragment_piece * piece)
{
	free(piece);
}

static libsqfs_fragment_block *
libsqfs_fragment_block_create(libsqfs_fragment_table * frag_table)
{
	libsqfs_fragment_block * fragment = malloc(sizeof(*fragment));
	if (!fragment) return 0;
	
	fragment->pieces.first = fragment->pieces.last = 0;
	fragment->pieces.count = 0;
	fragment->index = frag_table->fragments.count;
	fragment->size = 0;
	fragment->chunk = 0;
	
	return fragment;
}

static libsqfs_fragment_piece *
libsqfs_fragment_block_add_piece(libsqfs_fragment_block * fragment,
	libsqfs_data_t data, libsqfs_off_t offset, size_t size)
{
	libsqfs_fragment_piece * piece = malloc(sizeof(*piece));
	if (!piece) return 0;
	
	piece->src.data = data;
	piece->src.offset = offset;
	piece->src.size = size;
	piece->fragment = fragment;
	piece->offset = fragment->size;
	fragment->size += size;
	fragment->pieces.count ++;
	piece->prev = fragment->pieces.last;
	piece->next = 0;
	if (fragment->pieces.last) fragment->pieces.last->next = piece;
	else fragment->pieces.first = piece;
	fragment->pieces.last = piece;
	
	return piece;
}

static void
libsqfs_fragment_block_destroy(libsqfs_fragment_block * fragment)
{
	libsqfs_fragment_piece * piece = fragment->pieces.first;
	while(piece) {
		libsqfs_fragment_piece * next = piece->next;
		libsqfs_fragment_piece_destroy(piece);
		piece = next;
	}
	free(fragment);
}

void
libsqfs_fragment_table_init(libsqfs_fragment_table * frag_table)
{
	frag_table->offset = 0;
	frag_table->fragments.first = frag_table->fragments.last = 0;
	frag_table->fragments.count = 0;
	frag_table->open_fragment = 0;
}

void
libsqfs_fragment_table_destroy(libsqfs_fragment_table * frag_table)
{
	libsqfs_fragment_block * fragment = frag_table->fragments.first;
	
	while(fragment) {
		libsqfs_fragment_block * next = fragment->next;
		libsqfs_fragment_block_destroy(fragment);
		fragment = next;
	}
	if (frag_table->open_fragment)
		libsqfs_fragment_block_destroy(frag_table->open_fragment);
}

bool
libsqfs_fragment_table_write(libsqfs_image_t image, libsqfs_fragment_table * frag_table)
{
	if (!frag_table->fragments.count) {
		frag_table->offset = libsqfs_image_reserve(image, 0);
		return true;
	}
	
	struct squashfs_fragment_entry entries[frag_table->fragments.count];
	
	size_t n = 0;
	libsqfs_fragment_block * fragment = frag_table->fragments.first;
	while(fragment) {
		entries[n].start_block = cpu_to_le64(fragment->chunk->dst.offset);
		uint32_t value = fragment->chunk->dst.size;
		if (!fragment->chunk->dst.compressed)
			value |= SQUASHFS_COMPRESSED_BIT_BLOCK;
		entries[n].size = cpu_to_le32(value);
		entries[n].unused = 0;
		
		fragment = fragment->next;
		n ++;
	}
	
	frag_table->offset = libsqfs_write_metatable(image, entries, sizeof(entries),
		false, true);
	
	return frag_table->offset != -1;
}

libsqfs_fragment_piece *
libsqfs_image_submit_fragment_piece(libsqfs_image_t image, libsqfs_data_t data, size_t size, libsqfs_off_t offset)
{
	libsqfs_fragment_table * frag_table = &image->frag_table;
	if (frag_table->open_fragment && frag_table->open_fragment->size + size > image->block_size) {
		bool success = libsqfs_image_flush_fragments(image);
		if (!success) return 0;
	}
	
	if (!frag_table->open_fragment)
		frag_table->open_fragment = libsqfs_fragment_block_create(frag_table);
	
	libsqfs_fragment_block * fragment = frag_table->open_fragment;
	if (!fragment) return 0;
	
	return libsqfs_fragment_block_add_piece(fragment, data, offset, size);
}

bool
libsqfs_image_flush_fragments(libsqfs_image_t image)
{
	libsqfs_fragment_table * frag_table = &image->frag_table;
	libsqfs_fragment_block * fragment = frag_table->open_fragment;
	if (!fragment) return true;
	
	fragment->prev = frag_table->fragments.last;
	fragment->next = 0;
	if (frag_table->fragments.last) frag_table->fragments.last->next = fragment;
	else frag_table->fragments.first = fragment;
	frag_table->fragments.last = fragment;
	frag_table->fragments.count ++;
	
	/* create data element that linearizes the individual data
	pieces comprising the fragment */
	libsqfs_data_piece pieces[fragment->pieces.count];
	libsqfs_fragment_piece * current = fragment->pieces.first;
	size_t n = 0;
	while(current) {
		pieces[n] = current->src;
		n++;
		current = current->next;
	}
	libsqfs_data_t data = libsqfs_data_create_compound(image, n, pieces);
	if (!data) return false;
	
	/* submit this as a chunk, and track the chunk so we can later
	find out where it was written into the image */
	libsqfs_chunk * chunk = libsqfs_image_submit_chunk_for_data(image, data,
		0, fragment->size, image->options.fragment_compression);
	if (!chunk) return false;
	fragment->chunk = chunk;
	
	return true;
}

