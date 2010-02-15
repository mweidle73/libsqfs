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
	
	idtable->offset = libsqfs_write_metatable(image, ids, sizeof(ids), false, true);
	
	return idtable->offset != -1;
}

