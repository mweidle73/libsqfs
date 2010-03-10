#include "internal.h"

void
libsqfs_idtable_init(libsqfs_idtable * idtable)
{
	idtable->ids = 0;
	idtable->nids = 0;
	idtable->offset = -1;
}

/* returns 16-bit mapped id, or -1 on mapping failure */
int
libsqfs_idtable_map(libsqfs_idtable * idtable, uint32_t id)
{
	size_t n;
	for(n=0; n<idtable->nids; n++)
		if (idtable->ids[n] == id) return n;
	
	uint32_t * tmp = realloc(idtable->ids, sizeof(uint32_t) * (idtable->nids+1));
	if (!tmp) return -1;
	idtable->ids = tmp;
	
	n = idtable->nids;
	idtable->ids[idtable->nids++] = id;
	return n;
}

bool
libsqfs_idtable_write(libsqfs_image_t image, libsqfs_idtable * idtable)
{
	size_t n;
	uint32_t ids[idtable->nids];
	for(n=0; n<idtable->nids; n++)
		ids[n] = cpu_to_le32(idtable->ids[n]);
	
	libsqfs_metatable tab;
	libsqfs_metatable_init(&tab, 0);
	
	bool success = libsqfs_metatable_append(&tab, ids, sizeof(ids), 0);
	success = success && libsqfs_metatable_write_with_index(&tab, image);
	
	idtable->offset = tab.offset;
	
	libsqfs_metatable_fini(&tab);
	
	return success;
}

void
libsqfs_idtable_destroy(libsqfs_idtable * idtable)
{
	free(idtable->ids);
}

