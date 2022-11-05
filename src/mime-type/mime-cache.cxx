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

#include <string>
#include <string_view>

#include <vector>

#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>

#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "mime-cache.hxx"

inline constexpr u64 LIB_MAJOR_VERSION = 1;
/* FIXME: since mime-cache 1.2, weight is splitted into three parts
 * only lower 8 bit contains weight, and higher bits are flags and case-sensitivity.
 * anyway, since we do not support weight at all, it will be fixed later.
 * We claimed that we support 1.2 to cheat pcmanfm as a temporary quick dirty fix
 * for the broken file manager, but this should be correctly done in the future.
 * Weight and case-sensitivity are not handled now. */
inline constexpr u64 LIB_MINOR_VERSION = 2;

/* handle byte order here */
#define VAL16(buffer, idx) GUINT16_FROM_BE(*(u16*)(buffer + idx))
#define VAL32(buffer, idx) GUINT32_FROM_BE(*(u32*)(buffer + idx))

/* cache header */
// clang-format off
inline constexpr u64 MAJOR_VERSION  = 0;
inline constexpr u64 MINOR_VERSION  = 2;
inline constexpr u64 ALIAS_LIST     = 4;
inline constexpr u64 PARENT_LIST    = 8;
inline constexpr u64 LITERAL_LIST   = 12;
inline constexpr u64 SUFFIX_TREE    = 16;
inline constexpr u64 GLOB_LIST      = 20;
inline constexpr u64 MAGIC_LIST     = 24;
// inline constexpr u64 NAMESPACE_LIST = 28;
// clang-format on

MimeCache::MimeCache(std::string_view file_path)
{
    // LOG_INFO("MimeCache Constructor");

    this->file_path = file_path.data();

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
    i32 fd = open(this->file_path.data(), O_RDONLY, 0);

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

    u32 majv = VAL16(buf, MAJOR_VERSION);
    u32 minv = VAL16(buf, MINOR_VERSION);

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

    u32 offset;

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
MimeCache::lookup_literal(std::string_view filename)
{
    /* FIXME: weight is used in literal lookup after mime.cache v1.1.
     * However, it is poorly documented. So I have no idea how to implement this. */

    const char* entries = this->literals;
    i32 n = this->n_literals;
    i32 upper = n;
    i32 lower = 0;
    i32 middle = upper / 2;

    if (!entries)
        return nullptr;

    /* binary search */
    while (upper >= lower)
    {
        /* The entry size is different in v 1.1 */
        const char* entry = entries + middle * 12;
        const char* str = this->buffer + VAL32(entry, 0);
        const i32 comp = ztd::compare(filename, str);
        if (comp < 0)
            upper = middle - 1;
        else if (comp > 0)
            lower = middle + 1;
        else /* comp == 0 */
            return (this->buffer + VAL32(entry, 4));
        middle = (upper + lower) / 2;
    }

    return nullptr;
}

const char*
MimeCache::lookup_suffix(std::string_view filename, const char** suffix_pos)
{
    const char* root = this->suffix_roots;
    const u32 n = this->n_suffix_roots;
    const char* mime_type = nullptr;
    const char* ret = nullptr;

    if (n == 0)
        return nullptr;

    const char* _suffix_pos = (const char*)-1;
    const usize fn_len = filename.size();
    const char* suffix = g_utf8_find_prev_char(filename.data(), filename.substr(fn_len).data());
    const char* leaf_node =
        this->lookup_reverse_suffix_nodes(this->buffer, root, n, filename, suffix, &_suffix_pos);
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
MimeCache::lookup_magic(const char* data, u32 len)
{
    const char* magic = this->magics;

    if (!data || (len == 0) || !magic)
        return nullptr;

    for (usize i = 0; i < this->n_magics; ++i, magic += 16)
    {
        if (magic_match(this->buffer, magic, data, len))
            return this->buffer + VAL32(magic, 4);
    }
    return nullptr;
}

const char*
MimeCache::lookup_glob(std::string_view filename, i32* glob_len)
{
    const char* entry = this->globs;
    const char* type = nullptr;
    i32 max_glob_len = 0;

    /* entry size is changed in mime.cache 1.1 */
    usize entry_size = 12;

    for (usize i = 0; i < this->n_globs; ++i)
    {
        const char* glob = this->buffer + VAL32(entry, 0);
        i32 _glob_len;
        if (fnmatch(glob, filename.data(), 0) == 0 && (_glob_len = strlen(glob)) > max_glob_len)
        {
            max_glob_len = _glob_len;
            type = (this->buffer + VAL32(entry, 4));
        }
        entry += entry_size;
    }
    *glob_len = max_glob_len;
    return type;
}

const std::vector<std::string>
MimeCache::lookup_parents(std::string_view mime_type)
{
    std::vector<std::string> result;

    const char* found_parents =
        this->lookup_str_in_entries(this->parents, this->n_parents, mime_type);
    if (!found_parents)
        return result;

    const u32 n = VAL32(found_parents, 0);
    found_parents += 4;

    for (usize i = 0; i < n; ++i)
    {
        const u32 parent_off = VAL32(found_parents, i * 4);
        const std::string parent = this->buffer + parent_off;
        result.emplace_back(parent);
    }
    return result;
}

const char*
MimeCache::lookup_alias(std::string_view mime_type)
{
    return this->lookup_str_in_entries(this->alias, this->n_alias, mime_type);
}

const std::string&
MimeCache::get_file_path()
{
    return this->file_path;
}

u32
MimeCache::get_magic_max_extent()
{
    return this->magic_max_extent;
}

const char*
MimeCache::lookup_str_in_entries(const char* entries, u32 n, std::string_view str)
{
    i32 upper = n;
    i32 lower = 0;
    i32 middle = upper / 2;

    if (!entries || str.empty())
        return nullptr;

    /* binary search */
    while (upper >= lower)
    {
        const char* entry = entries + middle * 8;
        const char* str2 = this->buffer + VAL32(entry, 0);

        const i32 comp = ztd::compare(str, str2);
        if (comp < 0)
            upper = middle - 1;
        else if (comp > 0)
            lower = middle + 1;
        else /* comp == 0 */
            return (this->buffer + VAL32(entry, 4));

        middle = (upper + lower) / 2;
    }

    return nullptr;
}

bool
MimeCache::magic_rule_match(const char* buf, const char* rule, const char* data, u32 len)
{
    u32 offset = VAL32(rule, 0);
    u32 range = VAL32(rule, 4);

    u32 max_offset = offset + range;
    u32 val_len = VAL32(rule, 12);

    for (; offset < max_offset && (offset + val_len) <= len; ++offset)
    {
        bool match = false;
        u32 val_off = VAL32(rule, 16);
        u32 mask_off = VAL32(rule, 20);
        const char* value = buf + val_off;
        /* FIXME: word_size and byte order are not supported! */

        if (mask_off > 0) /* compare with mask applied */
        {
            const char* mask = buf + mask_off;

            usize i = 0;
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
            u32 n_children = VAL32(rule, 24);
            if (n_children > 0)
            {
                u32 first_child_off = VAL32(rule, 28);
                rule = buf + first_child_off;
                for (usize i = 0; i < n_children; ++i, rule += 32)
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
MimeCache::magic_match(const char* buf, const char* magic, const char* data, u32 len)
{
    u32 n_rules = VAL32(magic, 8);
    u32 rules_off = VAL32(magic, 12);
    const char* rule = buf + rules_off;

    for (usize i = 0; i < n_rules; ++i, rule += 32)
        if (magic_rule_match(buf, rule, data, len))
            return true;
    return false;
}

const char*
MimeCache::lookup_suffix_nodes(const char* buf, const char* nodes, u32 n, const char* name)
{
    u32 uchar = g_unichar_tolower(g_utf8_get_char(name));

    /* binary search */
    i32 upper = n;
    i32 lower = 0;
    i32 middle = n / 2;

    while (upper >= lower)
    {
        const char* node = nodes + middle * 16;
        u32 ch = VAL32(node, 0);

        if (uchar < ch)
            upper = middle - 1;
        else if (uchar > ch)
            lower = middle + 1;
        else /* uchar == ch */
        {
            u32 n_children = VAL32(node, 8);
            name = g_utf8_next_char(name);

            if (n_children > 0)
            {
                u32 first_child_off;
                if (uchar == 0)
                    return nullptr;

                if (!name || name[0] == 0)
                {
                    u32 offset = VAL32(node, 4);
                    return offset ? buf + offset : nullptr;
                }
                first_child_off = VAL32(node, 12);
                return lookup_suffix_nodes(buf, (buf + first_child_off), n_children, name);
            }
            else
            {
                if (!name || name[0] == 0)
                {
                    u32 offset = VAL32(node, 4);
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
MimeCache::lookup_reverse_suffix_nodes(const char* buf, const char* nodes, u32 n,
                                       std::string_view name, const char* suffix,
                                       const char** suffix_pos)
{
    const char* ret = nullptr;
    const char* cur_suffix_pos = (const char*)suffix + 1;

    const u32 uchar = suffix ? g_unichar_tolower(g_utf8_get_char(suffix)) : 0;
    // LOG_DEBUG("{}: suffix= '{}'", name, suffix);

    for (usize i = 0; i < n; ++i)
    {
        const char* node = nodes + i * 12;
        const u32 ch = VAL32(node, 0);
        const char* _suffix_pos = suffix;
        if (ch)
        {
            if (ch == uchar)
            {
                u32 n_children = VAL32(node, 4);
                u32 first_child_off = VAL32(node, 8);
                const char* leaf_node =
                    this->lookup_reverse_suffix_nodes(buf,
                                                      buf + first_child_off,
                                                      n_children,
                                                      name,
                                                      g_utf8_find_prev_char(name.data(), suffix),
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
            /* u32 weight = VAL32(node, 8); */
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
