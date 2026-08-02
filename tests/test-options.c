/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2011
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Refer to the file "COPYING" for details.
 */

#include <libsqfs.h>

int main()
{
	libsqfs_image_options_t o1 = libsqfs_image_options_create();
	libsqfs_image_options_t o2 = libsqfs_image_options_copy(o1);
	libsqfs_image_options_destroy(o2);
	libsqfs_image_options_destroy(o1);
	
	return 0;
}
