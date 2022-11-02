/**
 * Copyright (C) 2007 PCMan <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <vector>

#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>

#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "mime-cache.hxx"

#define LIB_MAJOR_VERSION 1
/* FIXME: since mime-cache 1.2, weight is splitted into three parts
 * only lower 8 bit contains weight, and higher bits are flags and case-sensitivity.
 * anyway, since we do not support weight at all, it will be fixed later.
 * We claimed that we support 1.2 to cheat pcmanfm as a temporary quick dirty fix
 * for the broken file manager, but this should be correctly done in the future.
 * Weight and case-sensitivity are not handled now. */
#define LIB_MINOR_VERSION 2

/* handle byte order here */
#define VAL16(buffer, idx) GUINT16_FROM_BE(*(std::uint16_t*)(buffer + idx))
#define VAL32(buffer, idx) GUINT32_FROM_BE(*(std::uint32_t*)(buffer + idx))

#define UINT32(obj) (static_cast<std::uint32_t>(obj))

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

MimeCache::MimeCache(const std::string& file_path)
{
    // LOG_INFO("MimeCache Constructor");

    this->file_path = file_path;

    load_mime_file();
}

MimeCache::~MimeCache()
{
    // LOG_INFO("MimeCache Destructor");
}

void
MimeCache::load_mime_file()
{
    // Open the file and map it into memory
    int fd = open(this->file_path.c_str(), O_RDONLY, 0);

    if (fd < 0)
        return;

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0)
    {
        close(fd);
        return;
    }

    char* buf = nullptr;
    buf = (char*)g_malloc(statbuf.st_size);
    read(fd, buf, statbuf.st_size);
    close(fd);

    unsigned int majv = VAL16(buf, MAJOR_VERSION);
    unsigned int minv = VAL16(buf, MINOR_VERSION);

    if (majv != LIB_MAJOR_VERSION || minv != LIB_MINOR_VERSION)
    {
        LOG_ERROR("shared-mime-info version error, only supports {}.{} trying to use {}.{}",
                  LIB_MAJOR_VERSION,
                  LIB_MINOR_VERSION,
                  majv,
                  minv);
        free(buf);
        return;
    }

    std::uint32_t offset;

    this->buffer = buf;
    this->buffer_size = statbuf.st_size;

    offset = VAL32(this->buffer, ALIAS_LIST);
    this->alias = this->buffer + offset + 4;
    this->n_alias = VAL32(this->buffer, offset);

    offset = VAL32(this->buffer, PARENT_LIST);
    this->parents = this->buffer + offset + 4;
    this->n_parents = VAL32(this->buffer, offset);

    offset = VAL32(this->buffer, LITERAL_LIST);
    this->literals = this->buffer + offset + 4;
    this->n_literals = VAL32(this->buffer, offset);

    offset = VAL32(this->buffer, GLOB_LIST);
    this->globs = this->buffer + offset + 4;
    this->n_globs = VAL32(this->buffer, offset);

    offset = VAL32(this->buffer, SUFFIX_TREE);
    this->suffix_roots = this->buffer + VAL32(this->buffer + offset, 4);
    this->n_suffix_roots = VAL32(this->buffer, offset);

    offset = VAL32(this->buffer, MAGIC_LIST);
    this->n_magics = VAL32(this->buffer, offset);
    this->magic_max_extent = VAL32(this->buffer + offset, 4);
    this->magics = this->buffer + VAL32(this->buffer + offset, 8);
}

void
MimeCache::reload()
{
    load_mime_file();
}

const char*
MimeCache::lookup_literal(const char* filename)
{
    /* FIXME: weight is used in literal lookup after mime.cache v1.1.
     * However, it is poorly documented. So I have no idea how to implement this. */

    const char* entries = this->literals;
    int n = this->n_literals;
    int upper = n;
    int lower = 0;
    int middle = upper / 2;

    if (entries && filename && *filename)
    {
        /* binary search */
        while (upper >= lower)
        {
            /* The entry size is different in v 1.1 */
            const char* entry = entries + middle * 12;
            const char* str2 = this->buffer + VAL32(entry, 0);
            int comp = strcmp(filename, str2);
            if (comp < 0)
                upper = middle - 1;
            else if (comp > 0)
                lower = middle + 1;
            else /* comp == 0 */
                return (this->buffer + VAL32(entry, 4));
            middle = (upper + lower) / 2;
        }
    }
    return nullptr;
}

const char*
MimeCache::lookup_suffix(const char* filename, const char** suffix_pos)
{
    const char* root = this->suffix_roots;
    std::uint32_t n = this->n_suffix_roots;
    const char* mime_type = nullptr;
    const char* ret = nullptr;

    if (!filename || !*filename || n == 0)
        return nullptr;

    const char* suffix;
    const char* leaf_node;
    const char* _suffix_pos = (const char*)-1;
    std::size_t fn_len = std::strlen(filename);
    suffix = g_utf8_find_prev_char(filename, filename + fn_len);
    leaf_node = lookup_reverse_suffix_nodes(this->buffer, root, n, filename, suffix, &_suffix_pos);
    if (leaf_node)
    {
        mime_type = this->buffer + VAL32(leaf_node, 4);
        // LOG_DEBUG("found: {}", mime_type);
        *suffix_pos = _suffix_pos;
        ret = mime_type;
    }

    return ret;
}

const char*
MimeCache::lookup_magic(const char* data, int len)
{
    const char* magic = this->magics;

    if (!data || (len == 0) || !magic)
        return nullptr;

    for (std::size_t i = 0; i < this->n_magics; ++i, magic += 16)
    {
        if (magic_match(this->buffer, magic, data, len))
            return this->buffer + VAL32(magic, 4);
    }
    return nullptr;
}

const char*
MimeCache::lookup_glob(const char* filename, int* glob_len)
{
    const char* entry = this->globs;
    const char* type = nullptr;
    int max_glob_len = 0;

    /* entry size is changed in mime.cache 1.1 */
    std::size_t entry_size = 12;

    for (std::size_t i = 0; i < this->n_globs; ++i)
    {
        const char* glob = this->buffer + VAL32(entry, 0);
        int _glob_len;
        if (fnmatch(glob, filename, 0) == 0 && (_glob_len = strlen(glob)) > max_glob_len)
        {
            max_glob_len = _glob_len;
            type = (this->buffer + VAL32(entry, 4));
        }
        entry += entry_size;
    }
    *glob_len = max_glob_len;
    return type;
}

const std::vector<const char*>
MimeCache::lookup_parents(const char* mime_type)
{
    std::vector<const char*> result;

    const char* found_parents = lookup_str_in_entries(this->parents, this->n_parents, mime_type);
    if (!found_parents)
        return result;

    std::uint32_t n = VAL32(found_parents, 0);
    found_parents += 4;

    for (std::size_t i = 0; i < n; ++i)
    {
        std::uint32_t parent_off = VAL32(found_parents, i * 4);
        const char* parent = this->buffer + parent_off;
        result.push_back(parent);
    }
    return result;
}

const char*
MimeCache::lookup_alias(const char* mime_type)
{
    return lookup_str_in_entries(this->alias, this->n_alias, mime_type);
}

const std::string&
MimeCache::get_file_path()
{
    return this->file_path;
}

std::uint32_t
MimeCache::get_magic_max_extent()
{
    return this->magic_max_extent;
}

const char*
MimeCache::lookup_str_in_entries(const char* entries, std::uint32_t n, const char* str)
{
    int upper = n;
    int lower = 0;
    int middle = upper / 2;

    if (entries && str && *str)
    {
        /* binary search */
        while (upper >= lower)
        {
            const char* entry = entries + middle * 8;
            const char* str2 = this->buffer + VAL32(entry, 0);

            int comp = strcmp(str, str2);
            if (comp < 0)
                upper = middle - 1;
            else if (comp > 0)
                lower = middle + 1;
            else /* comp == 0 */
                return (this->buffer + VAL32(entry, 4));

            middle = (upper + lower) / 2;
        }
    }
    return nullptr;
}

bool
MimeCache::magic_rule_match(const char* buf, const char* rule, const char* data, int len)
{
    std::uint32_t offset = VAL32(rule, 0);
    std::uint32_t range = VAL32(rule, 4);

    std::uint32_t max_offset = offset + range;
    std::uint32_t val_len = VAL32(rule, 12);

    for (; offset < max_offset && (offset + val_len) <= UINT32(len); ++offset)
    {
        bool match = false;
        std::uint32_t val_off = VAL32(rule, 16);
        std::uint32_t mask_off = VAL32(rule, 20);
        const char* value = buf + val_off;
        /* FIXME: word_size and byte order are not supported! */

        if (mask_off > 0) /* compare with mask applied */
        {
            const char* mask = buf + mask_off;

            std::size_t i = 0;
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
            std::uint32_t n_children = VAL32(rule, 24);
            if (n_children > 0)
            {
                std::uint32_t first_child_off = VAL32(rule, 28);
                rule = buf + first_child_off;
                for (std::size_t i = 0; i < n_children; ++i, rule += 32)
                {
                    if (magic_rule_match(buf, rule, data, len))
                        return true;
                }
            }
            else
            {
                return true;
            }
        }
    }
    return false;
}

bool
MimeCache::magic_match(const char* buf, const char* magic, const char* data, int len)
{
    std::uint32_t n_rules = VAL32(magic, 8);
    std::uint32_t rules_off = VAL32(magic, 12);
    const char* rule = buf + rules_off;

    for (std::size_t i = 0; i < n_rules; ++i, rule += 32)
        if (magic_rule_match(buf, rule, data, len))
            return true;
    return false;
}

const char*
MimeCache::lookup_suffix_nodes(const char* buf, const char* nodes, std::uint32_t n,
                               const char* name)
{
    std::uint32_t uchar = g_unichar_tolower(g_utf8_get_char(name));

    /* binary search */
    int upper = n;
    int lower = 0;
    int middle = n / 2;

    while (upper >= lower)
    {
        const char* node = nodes + middle * 16;
        std::uint32_t ch = VAL32(node, 0);

        if (uchar < ch)
            upper = middle - 1;
        else if (uchar > ch)
            lower = middle + 1;
        else /* uchar == ch */
        {
            std::uint32_t n_children = VAL32(node, 8);
            name = g_utf8_next_char(name);

            if (n_children > 0)
            {
                std::uint32_t first_child_off;
                if (uchar == 0)
                    return nullptr;

                if (!name || name[0] == 0)
                {
                    std::uint32_t offset = VAL32(node, 4);
                    return offset ? buf + offset : nullptr;
                }
                first_child_off = VAL32(node, 12);
                return lookup_suffix_nodes(buf, (buf + first_child_off), n_children, name);
            }
            else
            {
                if (!name || name[0] == 0)
                {
                    std::uint32_t offset = VAL32(node, 4);
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
const char*
MimeCache::lookup_reverse_suffix_nodes(const char* buf, const char* nodes, std::uint32_t n,
                                       const char* name, const char* suffix,
                                       const char** suffix_pos)
{
    const char* ret = nullptr;
    const char* cur_suffix_pos = (const char*)suffix + 1;

    std::uint32_t uchar = suffix ? g_unichar_tolower(g_utf8_get_char(suffix)) : 0;
    // LOG_DEBUG("{}: suffix= '{}'", name, suffix);

    for (std::size_t i = 0; i < n; ++i)
    {
        const char* node = nodes + i * 12;
        std::uint32_t ch = VAL32(node, 0);
        const char* _suffix_pos = suffix;
        if (ch)
        {
            if (ch == uchar)
            {
                std::uint32_t n_children = VAL32(node, 4);
                std::uint32_t first_child_off = VAL32(node, 8);
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
            /* std::uint32_t weight = VAL32(node, 8); */
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
