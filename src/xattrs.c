#include "xattrs.h"
#include "internal.h"

#include <string.h>

typedef struct {
	char prefix[16];
	size_t prefix_len;
	int code;
} libsqfs_xattr_domain;

static const libsqfs_xattr_domain xattr_domains[] = {
	{ "user.", 5, SQUASHFS_XATTR_USER },
	{ "trusted.", 8, SQUASHFS_XATTR_TRUSTED },
	{ "security.", 9, SQUASHFS_XATTR_SECURITY },
};

static const libsqfs_xattr_domain *
classify_xattr_domain(const char * name)
{
	size_t n;
	for(n=0; n<sizeof(xattr_domains)/sizeof(xattr_domains[0]); n++) {
		const libsqfs_xattr_domain * dom = &xattr_domains[n];
		if (strncasecmp(name, dom->prefix, dom->prefix_len) == 0)
			return dom;
	}
	return 0;
}

libsqfs_xattr_t
libsqfs_xattr_create(libsqfs_image_t image, const char * name, size_t value_size, const void * value)
{
	/* test whether the attribute name has a valid prefix */
	const libsqfs_xattr_domain * dom = classify_xattr_domain(name);
	if (!dom) return false;
	
	libsqfs_xattr * xattr = malloc(sizeof(*xattr));
	if (!xattr) return 0;
	
	xattr->name = strdup(name);
	if (!xattr->name) {
		free(xattr);
		return 0;
	}
	xattr->suffix_length = strlen(name) - dom->prefix_len;
	xattr->suffix = xattr->name + dom->prefix_len;
	xattr->domain = dom->code;
	
	xattr->value = malloc(value_size);
	if (!xattr->value) {
		free(xattr->name);
		free(xattr);
		return 0;
	}
	
	memcpy(xattr->value, value, value_size);
	xattr->value_length = value_size;
	
	libsqfs_xattr_table * tab = &image->xattr_table;
	
	xattr->prev = tab->xattrs.last;
	xattr->next = 0;
	if (tab->xattrs.last) tab->xattrs.last->next = xattr;
	else tab->xattrs.first = xattr;
	tab->xattrs.last = xattr;
	
	return xattr;
}

static void
libsqfs_xattr_destroy(libsqfs_xattr_t xattr)
{
	free(xattr->name);
	free(xattr->value);
	free(xattr);
}

/* serialize all attributes corresponding to this set into the
corresponding xattr data table */
static bool
libsqfs_xattrset_serialize_data(libsqfs_image_t image, libsqfs_xattrset * xattrset)
{
	libsqfs_xattr_table * tab = &image->xattr_table;
	bool first = true;
	
	xattrset->stored_size = 0;
	
	size_t n;
	for(n=0; n<xattrset->nattrs; n++) {
		libsqfs_xattr * xattr = xattrset->attrs[n];
		
		struct squashfs_xattr_entry entry;
		entry.type = cpu_to_le32(xattr->domain);
		entry.size = cpu_to_le32(xattr->suffix_length);
		libsqfs_metatable_entry pos;
		if (!libsqfs_metatable_append(&tab->xattr_data, &entry, sizeof(entry), &pos)) return false;
		if (first) xattrset->stored_pos = pos;
		
		if (!libsqfs_metatable_append(&tab->xattr_data, xattr->suffix, xattr->suffix_length, &pos)) return false;
		
		struct squashfs_xattr_val val;
		val.vsize = cpu_to_le32(xattr->value_length);
		if (!libsqfs_metatable_append(&tab->xattr_data, &val, sizeof(val), &pos)) return false;
		
		if (!libsqfs_metatable_append(&tab->xattr_data, xattr->value, xattr->value_length, &pos)) return false;
		
		xattrset->stored_size += sizeof(entry) + xattr->suffix_length + sizeof(val) + xattr->value_length;
		
		first = false;
	}
	
	struct squashfs_xattr_id descriptor;
	descriptor.xattr = cpu_to_le64((xattrset->stored_pos.block << 16ULL) | xattrset->stored_pos.offset);
	descriptor.count = cpu_to_le32(xattrset->nattrs);
	descriptor.size = cpu_to_le32(xattrset->stored_size);
	if (!libsqfs_metatable_append(&tab->xattr_descriptors, &descriptor, sizeof(descriptor), 0)) return false;
	
	return true;
}

libsqfs_xattrset_t
libsqfs_xattrset_create(libsqfs_image_t image, size_t nattrs, libsqfs_xattr_t attrs[const])
{
	/* TODO: sort attributes by name, ensure names are unique */
	/* TODO: search for identical xattr sets, return existing xattr if possible */
	
	libsqfs_xattrset * xattrset;
	
	xattrset = malloc(sizeof(*xattrset));
	if (!xattrset) return 0;
	
	xattrset->attrs = malloc(nattrs * sizeof(attrs[0]));
	if (!xattrset->attrs) {
		free(xattrset);
		return 0;
	}
	
	size_t n;
	for(n=0; n<nattrs; n++) xattrset->attrs[n] = attrs[n];
	xattrset->nattrs = nattrs;
	
	libsqfs_xattr_table * tab = &image->xattr_table;
	xattrset->id = tab->nids++;
	
	xattrset->prev = tab->xattrsets.last;
	xattrset->next = 0;
	if (tab->xattrsets.last) tab->xattrsets.last->next = xattrset;
	else tab->xattrsets.first = xattrset;
	tab->xattrsets.last = xattrset;
	
	if (!libsqfs_xattrset_serialize_data(image, xattrset)) return 0;
	
	return xattrset;
}

static void
libsqfs_xattrset_destroy(libsqfs_xattrset * xattrset)
{
	free(xattrset->attrs);
	free(xattrset);
}










void
libsqfs_xattr_table_init(libsqfs_xattr_table * tab, libsqfs_compressor_instance * compressor)
{
	tab->xattrs.first = tab->xattrs.last = 0;
	tab->xattrsets.first = tab->xattrsets.last = 0;
	tab->nids = 0;
	libsqfs_metatable_init(&tab->xattr_data, 0);
	libsqfs_metatable_init(&tab->xattr_descriptors, 0);
}

void
libsqfs_xattr_table_fini(libsqfs_xattr_table * tab)
{
	libsqfs_xattrset_t xattrset = tab->xattrsets.first;
	while(xattrset) {
		libsqfs_xattrset_t next = xattrset->next;
		libsqfs_xattrset_destroy(xattrset);
		xattrset = next;
	}
	
	libsqfs_xattr_t xattr = tab->xattrs.first;
	while(xattr) {
		libsqfs_xattr_t next = xattr->next;
		libsqfs_xattr_destroy(xattr);
		xattr = next;
	}
	
	libsqfs_metatable_fini(&tab->xattr_data);
	libsqfs_metatable_fini(&tab->xattr_descriptors);
}

bool
libsqfs_xattr_table_write(libsqfs_xattr_table * tab, libsqfs_image_t image)
{
	if (tab->nids == 0) {
		tab->offset = (libsqfs_off_t)-1;
		return true;
	}
	/* layout of the xattr area is as follows:

	+-------------------+
	| xattr data        |
	+-------------------+
	| xattr descriptors |
	+-------------------+
	| header            |
	+-------------------+
	| index table       |
	+-------------------+

	The header provides the number of xattr descriptors as well as the start
	position of the xattr data table. The following index table contains the
	locations of the compressed blocks building the xattr descriptor table
	(size of the index is implicitly determined through the number of descriptors).
	The descriptors themselves reference the xattr data blocks.
	*/
	ssize_t written;
	size_t descriptor_table_size = tab->nids * sizeof(struct squashfs_xattr_id);
	size_t index_entries = (descriptor_table_size + SQUASHFS_METADATA_SIZE - 1) / SQUASHFS_METADATA_SIZE;

	/* xattr data */
	if (!libsqfs_metatable_write(&tab->xattr_data, image)) return false;

	/* xattr descriptors */
	if (!libsqfs_metatable_write(&tab->xattr_descriptors, image)) return false;

	/* header */
	struct squashfs_xattr_table header;
	libsqfs_off_t xattr_header_pos = libsqfs_image_reserve(image, sizeof(header));

	header.xattr_table_start = cpu_to_le64(tab->xattr_data.offset);
	header.xattr_ids = cpu_to_le32(tab->nids);
	header.unused = 0;

	written = libsqfs_pwrite(image->dst, &header, sizeof(header), xattr_header_pos);
	if (written != sizeof(header)) return false;

	/* index table */ 
	/* reserve space for index table, will revisit later */
	libsqfs_off_t index_pos = libsqfs_image_reserve(image, index_entries * sizeof(uint64_t));

	uint64_t index_table[index_entries];
	size_t n;
	libsqfs_metablock * block = tab->xattr_descriptors.first;
	for(n=0; n<index_entries; n++) {
		index_table[n] = cpu_to_le64(tab->xattr_descriptors.offset + block->offset);
		block = block->next;
	}

	written = libsqfs_pwrite(image->dst, index_table, sizeof(index_table), index_pos);
	if (written != sizeof(index_table)) return false;

	tab->offset = xattr_header_pos;

	return true;
}
