/*
 *      vfs-thumbnail-loader.h
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#pragma once

#include "vfs-dir.hxx"
#include "vfs-file-info.hxx"

struct VFSThumbnailLoader;

void vfs_thumbnail_loader_free(VFSThumbnailLoader* loader);

void vfs_thumbnail_loader_request(VFSDir* dir, VFSFileInfo* file, bool is_big);
void vfs_thumbnail_loader_cancel_all_requests(VFSDir* dir, bool is_big);

/* Load thumbnail for the specified file
 *  If the caller knows mtime of the file, it should pass mtime to this function to
 *  prevent unnecessary disk I/O and this can speed up the loading.
 *  Otherwise, it should pass 0 for mtime, and the function will do stat() on the file
 *  to get mtime.
 */
GdkPixbuf* vfs_thumbnail_load_for_uri(const char* uri, int size, time_t mtime);
GdkPixbuf* vfs_thumbnail_load_for_file(const char* file, int size, time_t mtime);

void vfs_thumbnail_init();
