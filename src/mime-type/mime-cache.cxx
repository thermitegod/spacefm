/*
 *      mime-cache.c
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

#include <cstdint>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "mime-cache.hxx"

#define LIB_MAJOR_VERSION 1
/* FIXME: since mime-cache 1.2, weight is splitted into three parts
 * only lower 8 bit contains weight, and higher bits are flags and case-sensitivity.
 * anyway, since we don't support weight at all, it'll be fixed later.
 * We claimed that we support 1.2 to cheat pcmanfm as a temporary quick dirty fix
 * for the broken file manager, but this should be correctly done in the future.
 * Weight and case-sensitivity are not handled now. */
#define LIB_MAX_MINOR_VERSION 2
#define LIB_MIN_MINOR_VERSION 0

/* handle byte order here */
#define VAL16(buffrer, idx) GUINT16_FROM_BE(*(uint16_t*)(buffer + idx))
#define VAL32(buffer, idx)  GUINT32_FROM_BE(*(uint32_t*)(buffer + idx))

/* cache header */
#define MAJOR_VERSION  0
#define MINOR_VERSION  2
#define ALIAS_LIST     4
#define PARENT_LIST    8
#define LITERAL_LIST   12
#define SUFFIX_TREE    16
#define GLOB_LIST      20
#define MAGIC_LIST     24
#define NAMESPACE_LIST 28

MimeCache*
mime_cache_new(const char* file_path)
{
    MimeCache* cache = g_slice_new0(MimeCache);
    if (G_LIKELY(file_path))
        mime_cache_load(cache, file_path);
    return cache;
}

static void
mime_cache_unload(MimeCache* cache, bool clear)
{
    if (G_LIKELY(cache->buffer))
    {
#ifdef HAVE_MMAP
        munmap((char*)cache->buffer, cache->size);
#else
        g_free(cache->buffer);
#endif
    }
    g_free(cache->file_path);
    if (clear)
        memset(cache, 0, sizeof(MimeCache));
}

void
mime_cache_free(MimeCache* cache)
{
    mime_cache_unload(cache, false);
    g_slice_free(MimeCache, cache);
}

bool
mime_cache_load(MimeCache* cache, const char* file_path)
{
    /* Unload old cache first if needed */
    if (file_path == cache->file_path)
        cache->file_path =
            nullptr; /* steal the string to prevent it from being freed during unload */
    mime_cache_unload(cache, true);

    /* Store the file path */
    cache->file_path = g_strdup(file_path);

    /* Open the file and map it into memory */
    int fd = open(file_path, O_RDONLY, 0);

    if (fd < 0)
        return false;

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0)
    {
        close(fd);
        return false;
    }

    char* buffer = nullptr;
#ifdef HAVE_MMAP
    buffer = (char*)mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
#else
    buffer = g_malloc(statbuf.st_size);
    if (buffer)
        read(fd, buffer, statbuf.st_size);
    else
        buffer = (void*)-1;
#endif
    close(fd);

    if (buffer == (void*)-1)
        return false;

    unsigned int majv = VAL16(buffer, MAJOR_VERSION);
    unsigned int minv = VAL16(buffer, MINOR_VERSION);

    /* Check version */
    if (majv > LIB_MAJOR_VERSION || minv > LIB_MAX_MINOR_VERSION || minv < LIB_MIN_MINOR_VERSION)
    {
#ifdef HAVE_MMAP
        munmap(buffer, statbuf.st_size);
#else
        g_free(buffer);
#endif
        return false;
    }

    /* Since mime.cache v1.1, shared mime info v0.4
     * suffix tree is replaced with reverse suffix tree,
     * and glob and literal strings are sorted by weight. */
    if (minv >= 1)
    {
        cache->has_reverse_suffix = true;
        cache->has_str_weight = true;
    }

    uint32_t offset;

    cache->buffer = buffer;
    cache->size = statbuf.st_size;

    offset = VAL32(buffer, ALIAS_LIST);
    cache->alias = buffer + offset + 4;
    cache->n_alias = VAL32(buffer, offset);

    offset = VAL32(buffer, PARENT_LIST);
    cache->parents = buffer + offset + 4;
    cache->n_parents = VAL32(buffer, offset);

    offset = VAL32(buffer, LITERAL_LIST);
    cache->literals = buffer + offset + 4;
    cache->n_literals = VAL32(buffer, offset);

    offset = VAL32(buffer, GLOB_LIST);
    cache->globs = buffer + offset + 4;
    cache->n_globs = VAL32(buffer, offset);

    offset = VAL32(buffer, SUFFIX_TREE);
    cache->suffix_roots = buffer + VAL32(buffer + offset, 4);
    cache->n_suffix_roots = VAL32(buffer, offset);

    offset = VAL32(buffer, MAGIC_LIST);
    cache->n_magics = VAL32(buffer, offset);
    cache->magic_max_extent = VAL32(buffer + offset, 4);
    cache->magics = buffer + VAL32(buffer + offset, 8);

    return true;
}

static bool
magic_rule_match(const char* buf, const char* rule, const char* data, int len)
{
    uint32_t offset = VAL32(rule, 0);
    uint32_t range = VAL32(rule, 4);

    uint32_t max_offset = offset + range;
    uint32_t val_len = VAL32(rule, 12);

    for (; offset < max_offset && (offset + val_len) <= static_cast<uint32_t>(len); ++offset)
    {
        bool match = false;
        uint32_t val_off = VAL32(rule, 16);
        uint32_t mask_off = VAL32(rule, 20);
        const char* value = buf + val_off;
        /* FIXME: word_size and byte order are not supported! */

        if (G_UNLIKELY(mask_off > 0)) /* compare with mask applied */
        {
            unsigned int i = 0;
            const char* mask = buf + mask_off;

            for (; i < val_len; ++i)
            {
                if ((data[offset + i] & mask[i]) != value[i])
                    break;
            }
            if (i >= val_len)
                match = true;
        }
        else /* direct comparison */
        {
            if (memcmp(value, data + offset, val_len) == 0)
                match = true;
        }

        if (match)
        {
            uint32_t n_children = VAL32(rule, 24);
            if (n_children > 0)
            {
                uint32_t first_child_off = VAL32(rule, 28);
                unsigned int i;
                rule = buf + first_child_off;
                for (i = 0; i < n_children; ++i, rule += 32)
                {
                    if (magic_rule_match(buf, rule, data, len))
                        return true;
                }
            }
            else
                return true;
        }
    }
    return false;
}

static bool
magic_match(const char* buf, const char* magic, const char* data, int len)
{
    uint32_t n_rules = VAL32(magic, 8);
    uint32_t rules_off = VAL32(magic, 12);
    const char* rule = buf + rules_off;

    for (unsigned int i = 0; i < n_rules; ++i, rule += 32)
        if (magic_rule_match(buf, rule, data, len))
            return true;
    return false;
}

const char*
mime_cache_lookup_magic(MimeCache* cache, const char* data, int len)
{
    const char* magic = cache->magics;

    if (G_UNLIKELY(!data || (len == 0) || !magic))
        return nullptr;

    for (unsigned int i = 0; i < cache->n_magics; ++i, magic += 16)
    {
        if (magic_match(cache->buffer, magic, data, len))
        {
            return cache->buffer + VAL32(magic, 4);
        }
    }
    return nullptr;
}

static const char*
lookup_suffix_nodes(const char* buf, const char* nodes, uint32_t n, const char* name)
{
    uint32_t uchar = g_unichar_tolower(g_utf8_get_char(name));

    /* binary search */
    int upper = n;
    int lower = 0;
    int middle = n / 2;

    while (upper >= lower)
    {
        const char* node = nodes + middle * 16;
        uint32_t ch = VAL32(node, 0);

        if (uchar < ch)
            upper = middle - 1;
        else if (uchar > ch)
            lower = middle + 1;
        else /* uchar == ch */
        {
            uint32_t n_children = VAL32(node, 8);
            name = g_utf8_next_char(name);

            if (n_children > 0)
            {
                uint32_t first_child_off;
                if (uchar == 0)
                    return nullptr;

                if (!name || name[0] == 0)
                {
                    uint32_t offset = VAL32(node, 4);
                    return offset ? buf + offset : nullptr;
                }
                first_child_off = VAL32(node, 12);
                return lookup_suffix_nodes(buf, (buf + first_child_off), n_children, name);
            }
            else
            {
                if (!name || name[0] == 0)
                {
                    uint32_t offset = VAL32(node, 4);
                    return offset ? buf + offset : nullptr;
                }
                return nullptr;
            }
        }
        middle = (upper + lower) / 2;
    }
    return nullptr;
}

/* Reverse suffix tree is used since mime.cache 1.1 (shared mime info 0.4)
 * Returns the address of the found "node", not mime-type.
 * FIXME: 1. Should be optimized with binary search
 *        2. Should consider weight of suffix nodes
 */
static const char*
lookup_reverse_suffix_nodes(const char* buf, const char* nodes, uint32_t n, const char* name,
                            const char* suffix, const char** suffix_pos)
{
    const char* ret = nullptr;
    const char* cur_suffix_pos = (const char*)suffix + 1;

    uint32_t uchar = suffix ? g_unichar_tolower(g_utf8_get_char(suffix)) : 0;
    // LOG_DEBUG("{}: suffix= '{}'", name, suffix);

    for (unsigned int i = 0; i < n; ++i)
    {
        const char* node = nodes + i * 12;
        uint32_t ch = VAL32(node, 0);
        const char* _suffix_pos = suffix;
        if (G_LIKELY(ch))
        {
            if (ch == uchar)
            {
                uint32_t n_children = VAL32(node, 4);
                uint32_t first_child_off = VAL32(node, 8);
                const char* leaf_node =
                    lookup_reverse_suffix_nodes(buf,
                                                buf + first_child_off,
                                                n_children,
                                                name,
                                                g_utf8_find_prev_char(name, suffix),
                                                &_suffix_pos);
                if (leaf_node && _suffix_pos < cur_suffix_pos)
                {
                    ret = leaf_node;
                    cur_suffix_pos = _suffix_pos;
                }
            }
        }
        else /* ch == 0 */
        {
            /* uint32_t weight = VAL32(node, 8); */
            /* suffix is found in the tree! */

            if (suffix < cur_suffix_pos)
            {
                ret = node;
                cur_suffix_pos = suffix;
            }
        }
    }
    *suffix_pos = cur_suffix_pos;
    return ret;
}

const char*
mime_cache_lookup_suffix(MimeCache* cache, const char* filename, const char** suffix_pos)
{
    const char* root = cache->suffix_roots;
    int n = cache->n_suffix_roots;
    const char* mime_type = nullptr;
    const char* ret = nullptr;

    if (G_UNLIKELY(!filename || !*filename || n == 0))
        return nullptr;
    if (cache->has_reverse_suffix) /* since mime.cache ver: 1.1 */
    {
        const char* suffix;
        const char* leaf_node;
        const char* _suffix_pos = (const char*)-1;
        int fn_len = strlen(filename);
        suffix = g_utf8_find_prev_char(filename, filename + fn_len);
        leaf_node =
            lookup_reverse_suffix_nodes(cache->buffer, root, n, filename, suffix, &_suffix_pos);
        if (leaf_node)
        {
            mime_type = cache->buffer + VAL32(leaf_node, 4);
            // LOG_DEBUG("found: {}", mime_type);
            *suffix_pos = _suffix_pos;
            ret = mime_type;
        }
    }
    else /* before mime.cache ver: 1.1 */
    {
        const char* prev_suffix_pos = (const char*)-1;
        int i;
        for (i = 0; i < n; ++i, root += 16)
        {
            uint32_t first_child_off;
            uint32_t ch = VAL32(root, 0);
            const char* suffix;

            suffix = strchr(filename, ch);
            if (!suffix)
                continue;

            first_child_off = VAL32(root, 12);
            // FIXME: is this correct???
            n = VAL32(root, 8);
            do
            {
                mime_type = lookup_suffix_nodes(cache->buffer,
                                                cache->buffer + first_child_off,
                                                n,
                                                g_utf8_next_char(suffix));
                if (mime_type && suffix < prev_suffix_pos) /* we want the longest suffix matched. */
                {
                    ret = mime_type;
                    prev_suffix_pos = suffix;
                }
            } while ((suffix = strchr(suffix + 1, ch)));
        }
        *suffix_pos = ret ? prev_suffix_pos : (const char*)-1;
    }
    return ret;
}

static const char*
lookup_str_in_entries(MimeCache* cache, const char* entries, int n, const char* str)
{
    int upper = n;
    int lower = 0;
    int middle = upper / 2;

    if (G_LIKELY(entries && str && *str))
    {
        /* binary search */
        while (upper >= lower)
        {
            const char* entry = entries + middle * 8;
            const char* str2 = cache->buffer + VAL32(entry, 0);
            int comp = strcmp(str, str2);
            if (comp < 0)
                upper = middle - 1;
            else if (comp > 0)
                lower = middle + 1;
            else /* comp == 0 */
                return (cache->buffer + VAL32(entry, 4));
            middle = (upper + lower) / 2;
        }
    }
    return nullptr;
}

const char*
mime_cache_lookup_alias(MimeCache* cache, const char* mime_type)
{
    return lookup_str_in_entries(cache, cache->alias, cache->n_alias, mime_type);
}

const char*
mime_cache_lookup_literal(MimeCache* cache, const char* filename)
{
    /* FIXME: weight is used in literal lookup after mime.cache v1.1.
     * However, it's poorly documented. So I've no idea how to implement this. */
    if (cache->has_str_weight)
    {
        const char* entries = cache->literals;
        int n = cache->n_literals;
        int upper = n;
        int lower = 0;
        int middle = upper / 2;

        if (G_LIKELY(entries && filename && *filename))
        {
            /* binary search */
            while (upper >= lower)
            {
                /* The entry size is different in v 1.1 */
                const char* entry = entries + middle * 12;
                const char* str2 = cache->buffer + VAL32(entry, 0);
                int comp = strcmp(filename, str2);
                if (comp < 0)
                    upper = middle - 1;
                else if (comp > 0)
                    lower = middle + 1;
                else /* comp == 0 */
                    return (cache->buffer + VAL32(entry, 4));
                middle = (upper + lower) / 2;
            }
        }
        return nullptr;
    }
    return lookup_str_in_entries(cache, cache->literals, cache->n_literals, filename);
}

const char*
mime_cache_lookup_glob(MimeCache* cache, const char* filename, int* glob_len)
{
    const char* entry = cache->globs;
    const char* type = nullptr;
    int max_glob_len = 0;

    /* entry size is changed in mime.cache 1.1 */
    std::size_t entry_size = cache->has_str_weight ? 12 : 8;

    for (unsigned int i = 0; i < cache->n_globs; ++i)
    {
        const char* glob = cache->buffer + VAL32(entry, 0);
        int _glob_len;
        if (fnmatch(glob, filename, 0) == 0 && (_glob_len = strlen(glob)) > max_glob_len)
        {
            max_glob_len = _glob_len;
            type = (cache->buffer + VAL32(entry, 4));
        }
        entry += entry_size;
    }
    *glob_len = max_glob_len;
    return type;
}

const char**
mime_cache_lookup_parents(MimeCache* cache, const char* mime_type)
{
    const char* parents = lookup_str_in_entries(cache, cache->parents, cache->n_parents, mime_type);
    if (!parents)
        return nullptr;
    uint32_t n = VAL32(parents, 0);
    parents += 4;

    const char** result = (const char**)g_new(char*, n + 1);

    for (unsigned int i = 0; i < n; ++i)
    {
        uint32_t parent_off = VAL32(parents, i * 4);
        const char* parent = cache->buffer + parent_off;
        result[i] = parent;
    }
    result[n] = nullptr;
    return result;
}
