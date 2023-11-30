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

#include <filesystem>

#include <vector>

#include <span>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

class MimeCache
{
  public:
    MimeCache() = delete;
    ~MimeCache() = default;
    // ~MimeCache() { ztd::logger::info("MimeCache Destructor"); };

    MimeCache(const std::filesystem::path& file_path);

    void reload();

    const char* lookup_literal(const std::string_view filename);
    const char* lookup_suffix(const std::string_view filename, const char** suffix_pos);
    const char* lookup_magic(const std::span<const std::byte> data);
    const char* lookup_glob(const std::string_view filename, i32* glob_len);
    const std::vector<std::string> lookup_parents(const std::string_view mime_type);
    const char* lookup_alias(const std::string_view mime_type);

    const std::filesystem::path& file_path();
    u32 magic_max_extent();

  private:
    void load_mime_file();
    const char* lookup_str_in_entries(const char* entries, u32 n, const std::string_view str);
    bool magic_rule_match(const char* buf, const char* rule, const std::span<const std::byte> data);
    bool magic_match(const char* buf, const char* magic, const std::span<const std::byte> data);
    const char* lookup_suffix_nodes(const char* buf, const char* nodes, u32 n, const char* name);
    const char* lookup_reverse_suffix_nodes(const char* buf, const char* nodes, u32 n,
                                            const std::string_view name, const char* suffix,
                                            const char** suffix_pos);

  private:
    std::filesystem::path file_path_{};

    usize buffer_size_{0};
    const char* buffer_{nullptr};

    u32 n_alias_{0};
    const char* alias_{nullptr};

    u32 n_parents_{0};
    const char* parents_{nullptr};

    u32 n_literals_{0};
    const char* literals_{nullptr};

    u32 n_globs_{0};
    const char* globs_{nullptr};

    u32 n_suffix_roots_{0};
    const char* suffix_roots_{nullptr};

    u32 n_magics_{0};
    const char* magics_{nullptr};

    u32 magic_max_extent_{0};
};

using mime_cache_t = std::shared_ptr<MimeCache>;
