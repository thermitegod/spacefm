/*
 *      mime-cache.h
 *
 *      Copyright 2007 PCMan <pcman.tw@gmail.com>
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

#include <glib.h>

typedef struct MimeCache
{
    char* file_path;
    bool has_reverse_suffix : 1; /* since mime.cache v1.1, shared mime info v0.4 */
    bool has_str_weight : 1;     /* since mime.cache v1.1, shared mime info v0.4 */
    const char* buffer;
    unsigned int size;

    uint32_t n_alias;
    const char* alias;

    uint32_t n_parents;
    const char* parents;

    uint32_t n_literals;
    const char* literals;

    uint32_t n_globs;
    const char* globs;

    uint32_t n_suffix_roots;
    const char* suffix_roots;

    uint32_t n_magics;
    uint32_t magic_max_extent;
    const char* magics;
} MimeCache;

MimeCache* mime_cache_new(const char* file_path);
bool mime_cache_load(MimeCache* cache, const char* file_path);
bool mime_cache_reload(MimeCache* cache);
void mime_cache_free(MimeCache* cache);

const char* mime_cache_lookup_literal(MimeCache* cache, const char* filename);
const char* mime_cache_lookup_glob(MimeCache* cache, const char* filename, int* glob_len);
const char* mime_cache_lookup_suffix(MimeCache* cache, const char* filename,
                                     const char** suffix_pos);
const char* mime_cache_lookup_magic(MimeCache* cache, const char* data, int len);
const char** mime_cache_lookup_parents(MimeCache* cache, const char* mime_type);
const char* mime_cache_lookup_alias(MimeCache* cache, const char* mime_type);
