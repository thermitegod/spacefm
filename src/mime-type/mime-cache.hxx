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

#include <string>
#include <string_view>

#include <vector>

class MimeCache
{
  public:
    MimeCache(std::string_view file_path);
    ~MimeCache();

    void reload();

    const char* lookup_literal(const char* filename);
    const char* lookup_suffix(const char* filename, const char** suffix_pos);
    const char* lookup_magic(const char* data, int len);
    const char* lookup_glob(const char* filename, int* glob_len);
    const std::vector<const char*> lookup_parents(const char* mime_type);
    const char* lookup_alias(const char* mime_type);

    const std::string& get_file_path();
    std::uint32_t get_magic_max_extent();

  private:
    void load_mime_file();
    const char* lookup_str_in_entries(const char* entries, std::uint32_t n, const char* str);
    bool magic_rule_match(const char* buf, const char* rule, const char* data, int len);
    bool magic_match(const char* buf, const char* magic, const char* data, int len);
    const char* lookup_suffix_nodes(const char* buf, const char* nodes, std::uint32_t n,
                                    const char* name);
    const char* lookup_reverse_suffix_nodes(const char* buf, const char* nodes, std::uint32_t n,
                                            const char* name, const char* suffix,
                                            const char** suffix_pos);

  private:
    std::string file_path;

    const char* buffer{nullptr};
    std::size_t buffer_size{0};

    std::uint32_t n_alias{0};
    const char* alias{nullptr};

    std::uint32_t n_parents{0};
    const char* parents{nullptr};

    std::uint32_t n_literals{0};
    const char* literals{nullptr};

    std::uint32_t n_globs{0};
    const char* globs{nullptr};

    std::uint32_t n_suffix_roots{0};
    const char* suffix_roots{nullptr};

    std::uint32_t n_magics{0};
    std::uint32_t magic_max_extent{0};
    const char* magics{nullptr};
};
