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

#pragma once

#include <vector>

class MimeCache
{
  public:
    MimeCache(const std::string& file_path);
    ~MimeCache();

    void reload();

    const char* lookup_literal(const char* filename);
    const char* lookup_suffix(const char* filename, const char** suffix_pos);
    const char* lookup_magic(const char* data, int len);
    const char* lookup_glob(const char* filename, int* glob_len);
    std::vector<const char*> lookup_parents(const char* mime_type);
    const char* lookup_alias(const char* mime_type);

    const std::string& get_file_path();
    uint32_t get_magic_max_extent();

  private:
    std::string m_file_path;

    // since mime.cache v1.1, shared mime info v0.4
    bool m_has_reverse_suffix{true};
    bool m_has_str_weight{true};

    const char* m_buffer{nullptr};
    std::size_t m_buffer_size{0};

    uint32_t m_n_alias{0};
    const char* m_alias{nullptr};

    uint32_t m_n_parents{0};
    const char* m_parents{nullptr};

    uint32_t m_n_literals{0};
    const char* m_literals{nullptr};

    uint32_t m_n_globs{0};
    const char* m_globs{nullptr};

    uint32_t m_n_suffix_roots{0};
    const char* m_suffix_roots{nullptr};

    uint32_t m_n_magics{0};
    uint32_t m_magic_max_extent{0};
    const char* m_magics{nullptr};

    void load_mime_file();
    const char* lookup_str_in_entries(const char* entries, uint32_t n, const char* str);
    bool magic_rule_match(const char* buf, const char* rule, const char* data, int len);
    bool magic_match(const char* buf, const char* magic, const char* data, int len);
    const char* lookup_suffix_nodes(const char* buf, const char* nodes, uint32_t n,
                                    const char* name);
    const char* lookup_reverse_suffix_nodes(const char* buf, const char* nodes, uint32_t n,
                                            const char* name, const char* suffix,
                                            const char** suffix_pos);
};
