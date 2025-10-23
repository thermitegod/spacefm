/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
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

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

#if 0 // TODO
#include "settings/settings.hxx"
#endif

#include "vfs/utils/utils.hxx"

std::string
vfs::utils::format_file_size(u64 size_in_bytes, bool decimal) noexcept
{
#if 0 // TODO
    if (config::global::settings->use_si_prefix)
    {
        return ztd::format_filesize(size_in_bytes, ztd::base::si, decimal ? 1 : 0);
    }
    else
    {
        return ztd::format_filesize(size_in_bytes, ztd::base::iec, decimal ? 1 : 0);
    }
#else
    return ztd::format_filesize(size_in_bytes, ztd::base::iec, decimal ? 1 : 0);
#endif
}

std::array<std::string, 2>
vfs::utils::filename_stem_and_extension(const std::filesystem::path& filename) noexcept
{
    auto f = filename.string();

    const auto pos = f.find_last_of('.');
    if (pos != std::string::npos && pos != 0 && pos != f.length() - 1)
    {
        const auto [stem, _, extension] = ztd::rpartition(f, ".");
        if (stem.ends_with(".tar"))
        {
            const auto [stem2, _, extension2] = ztd::rpartition(stem, ".");
            return {stem2, std::format(".{}.{}", extension2, extension)};
        }
        else
        {
            return {stem, std::format(".{}", extension)};
        }
    }
    return {f, ""};
}

std::filesystem::path
vfs::utils::unique_path(const std::filesystem::path& path, const std::filesystem::path& filename,
                        const std::string_view tag) noexcept
{
    assert(!path.empty());
    assert(!filename.empty());

    const auto [stem, extension] = filename_stem_and_extension(filename);

    u32 n = 1;
    auto unique_path = path / std::format("{}{}", stem, extension);
    while (std::filesystem::exists(unique_path))
    { // need to see broken symlinks
        unique_path = path / std::format("{}{}{}{}", stem, tag, n, extension);
        n += 1;
    }

    return unique_path;
}
