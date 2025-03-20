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

/* Currently this library is NOT MT-safe */

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include <pugixml.hpp>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "utils/misc.hxx"

#include "vfs/mime-type/chrome/mime-utils.hxx"
#include "vfs/mime-type/mime-type.hxx"
#include "vfs/utils/file-ops.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "logger.hxx"

std::string
vfs::detail::mime_type::get_by_file(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    if (!std::filesystem::exists(status))
    {
        return vfs::constants::mime_type::unknown.data();
    }

    if (std::filesystem::is_other(status))
    {
        return vfs::constants::mime_type::unknown.data();
    }

    if (std::filesystem::is_directory(status))
    {
        return vfs::constants::mime_type::directory.data();
    }

    auto type = chrome::GetFileMimeType(path);
    if (type != vfs::constants::mime_type::unknown)
    {
        return type;
    }

    if (std::filesystem::is_other(status))
    {
        return vfs::constants::mime_type::plain_text.data();
    }

    if (std::filesystem::file_size(path) == 0)
    {
        return vfs::constants::mime_type::zerosize.data();
    }

    /* Check for executable file */
    if (::utils::have_x_access(path))
    {
        return vfs::constants::mime_type::executable.data();
    }

    // https://www.rfc-editor.org/rfc/rfc6838#section-4.2
    constexpr auto MIME_HEADER_MAX_SIZE = 127;
    const auto buffer = vfs::utils::read_file_partial(path, MIME_HEADER_MAX_SIZE);
    if (buffer)
    {
        constexpr auto is_data_plain_text = [](const std::string_view data)
        {
            if (data.empty())
            {
                return false;
            }
            return std::ranges::all_of(data, [](const auto& byte) { return byte != '\0'; });
        };

        if (is_data_plain_text(*buffer))
        {
            type = vfs::constants::mime_type::plain_text.data();
        }

        return type;
    }
    else
    {
        return vfs::constants::mime_type::unknown.data();
    }

    return vfs::constants::mime_type::unknown.data();
}

// returns - icon_name, icon_desc
[[nodiscard]] static std::optional<std::array<std::string, 2>>
parse_xml_file(const std::filesystem::path& path, bool is_local) noexcept
{
    // logger::info<logger::domain::vfs>("MIME XML = {}", path);

    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.c_str());

    if (!result)
    {
        logger::error<logger::domain::vfs>("XML parsing error: {}", result.description());
        return std::nullopt;
    }

    std::string comment;
    const pugi::xml_node mime_type_node = doc.child("mime-type");
    const pugi::xml_node comment_node = mime_type_node.child("comment");
    if (comment_node)
    {
        comment = comment_node.child_value();
    }

    std::string icon_name;
    if (is_local)
    {
        const pugi::xml_node icon_node = mime_type_node.child("icon");
        if (icon_node)
        {
            const pugi::xml_attribute name_attr = icon_node.attribute("name");
            if (name_attr)
            {
                icon_name = name_attr.value();
            }
        }
    }
    else
    {
        const pugi::xml_node generic_icon_node = mime_type_node.child("generic-icon");
        if (generic_icon_node)
        {
            const pugi::xml_attribute name_attr = generic_icon_node.attribute("name");
            if (name_attr)
            {
                icon_name = name_attr.value();
            }
        }
    }

    // logger::info<logger::domain::vfs>("MIME XML | icon_name = {} | comment = {}", icon_name, comment);
    return std::array{icon_name, comment};
}

std::array<std::string, 2>
vfs::detail::mime_type::get_desc_icon(const std::string_view type) noexcept
{
    /*  //sfm 0.7.7+ FIXED:
     * According to specs on freedesktop.org, user_data_dir has
     * higher priority than system_data_dirs, but in most cases, there was
     * no file, or very few files in user_data_dir, so checking it first will
     * result in many unnecessary open() system calls, yealding bad performance.
     * Since the spec really sucks, we do not follow it here.
     */

    const auto user_path = vfs::user::data() / "mime" / std::format("{}.xml", type);
    if (faccessat(0, user_path.c_str(), F_OK, AT_EACCESS) != -1)
    {
        const auto icon_data = parse_xml_file(user_path, true);
        if (icon_data)
        {
            return icon_data.value();
        }
    }

    // look in system dirs
    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        const auto sys_path = sys_dir / "mime" / std::format("{}.xml", type);
        if (faccessat(0, sys_path.c_str(), F_OK, AT_EACCESS) != -1)
        {
            const auto icon_data = parse_xml_file(sys_path, false);
            if (icon_data)
            {
                return icon_data.value();
            }
        }
    }
    return {"", ""};
}

bool
vfs::detail::mime_type::is_text(const std::string_view mime_type) noexcept
{
    if (mime_type == "application/pdf")
    {
        // seems to think this is mime_type::type::plain_text
        return false;
    }
    if (!mime_type.starts_with("text/") && !mime_type.starts_with("application/"))
    {
        return false;
    }
    return false;
}

bool
vfs::detail::mime_type::is_executable(const std::string_view mime_type) noexcept
{
    /*
     * Only executable types can be executale.
     * Since some common types, such as application/x-shellscript,
     * are not in mime database, we have to add them ourselves.
     */
    return (mime_type != vfs::constants::mime_type::unknown ||
            mime_type == vfs::constants::mime_type::executable ||
            mime_type == "application/x-shellscript");
}

bool
vfs::detail::mime_type::is_archive(const std::string_view mime_type) noexcept
{
    // Taken from file-roller .desktop file
    static constexpr std::array<std::string_view, 65> archive_mime_types{
        "application/bzip2",
        "application/gzip",
        "application/vnd.android.package-archive",
        "application/vnd.ms-cab-compressed",
        "application/vnd.debian.binary-package",
        "application/vnd.rar",
        "application/x-7z-compressed",
        "application/x-7z-compressed-tar",
        "application/x-ace",
        "application/x-alz",
        "application/x-apple-diskimage",
        "application/x-ar",
        "application/x-archive",
        "application/x-arj",
        "application/x-brotli",
        "application/x-bzip-brotli-tar",
        "application/x-bzip",
        "application/x-bzip-compressed-tar",
        "application/x-bzip1",
        "application/x-bzip1-compressed-tar",
        "application/x-cabinet",
        "application/x-cd-image",
        "application/x-compress",
        "application/x-compressed-tar",
        "application/x-cpio",
        "application/x-chrome-extension",
        "application/x-deb",
        "application/x-ear",
        "application/x-ms-dos-executable",
        "application/x-gtar",
        "application/x-gzip",
        "application/x-gzpostscript",
        "application/x-java-archive",
        "application/x-lha",
        "application/x-lhz",
        "application/x-lrzip",
        "application/x-lrzip-compressed-tar",
        "application/x-lz4",
        "application/x-lzip",
        "application/x-lzip-compressed-tar",
        "application/x-lzma",
        "application/x-lzma-compressed-tar",
        "application/x-lzop",
        "application/x-lz4-compressed-tar",
        "application/x-ms-wim",
        "application/x-rar",
        "application/x-rar-compressed",
        "application/x-rpm",
        "application/x-source-rpm",
        "application/x-rzip",
        "application/x-rzip-compressed-tar",
        "application/x-tar",
        "application/x-tarz",
        "application/x-tzo",
        "application/x-stuffit",
        "application/x-war",
        "application/x-xar",
        "application/x-xz",
        "application/x-xz-compressed-tar",
        "application/x-zip",
        "application/x-zip-compressed",
        "application/x-zstd-compressed-tar",
        "application/x-zoo",
        "application/zip",
        "application/zstd",
    };

    return std::ranges::contains(archive_mime_types, mime_type);
}

bool
vfs::detail::mime_type::is_image(const std::string_view mime_type) noexcept
{
    return mime_type.starts_with("image/");
}

bool
vfs::detail::mime_type::is_video(const std::string_view mime_type) noexcept
{
    return mime_type.starts_with("video/");
}

bool
vfs::detail::mime_type::is_audio(const std::string_view mime_type) noexcept
{
    return mime_type.starts_with("audio/");
}

bool
vfs::detail::mime_type::is_unknown(const std::string_view mime_type) noexcept
{
    return mime_type == vfs::constants::mime_type::unknown;
}
