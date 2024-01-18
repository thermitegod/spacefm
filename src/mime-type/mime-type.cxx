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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <array>

#include <span>

#include <optional>

#include <algorithm>

#include <fcntl.h>

#include <pugixml.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "mime-type/chrome/mime-utils.hxx"

#include "mime-type/mime-type.hxx"

// https://www.rfc-editor.org/rfc/rfc6838#section-4.2
inline constexpr u32 MIME_HEADER_MAX_SIZE = 127;

static bool
mime_type_is_data_plain_text(const std::span<const std::byte> data)
{
    if (data.size() == 0)
    {
        return false;
    }

    for (const auto d : data)
    {
        if (d == (std::byte)'\0')
        {
            return false;
        }
    }
    return true;
}

const std::string
mime_type_get_by_file(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    if (!std::filesystem::exists(status))
    {
        return XDG_MIME_TYPE_UNKNOWN.data();
    }

    if (std::filesystem::is_other(status))
    {
        return XDG_MIME_TYPE_UNKNOWN.data();
    }

    if (std::filesystem::is_directory(status))
    {
        return XDG_MIME_TYPE_DIRECTORY.data();
    }

    auto type = GetFileMimeType(path);
    if (type != XDG_MIME_TYPE_UNKNOWN)
    {
        return type;
    }

    const auto file_size = std::filesystem::file_size(path);
    if (file_size == 0 || std::filesystem::is_other(status))
    {
        // empty file can be viewed as text file
        return XDG_MIME_TYPE_PLAIN_TEXT.data();
    }

    /* Check for executable file */
    if (have_x_access(path))
    {
        return XDG_MIME_TYPE_EXECUTABLE.data();
    }

    const auto fd = open(path.c_str(), O_RDONLY, 0);
    if (fd != -1)
    {
        std::array<std::byte, MIME_HEADER_MAX_SIZE> data{};

        const auto length = read(fd, data.data(), data.size());
        if (length == -1)
        {
            return XDG_MIME_TYPE_UNKNOWN.data();
        }

        /* check for plain text */
        if (mime_type_is_data_plain_text(data))
        {
            type = XDG_MIME_TYPE_PLAIN_TEXT.data();
        }

        close(fd);

        return type;
    }

    return XDG_MIME_TYPE_UNKNOWN.data();
}

// returns - icon_name, icon_desc
static std::optional<std::array<std::string, 2>>
mime_type_parse_xml_file(const std::filesystem::path& path, bool is_local)
{
    // ztd::logger::info("MIME XML = {}", path);

    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.c_str());

    if (!result)
    {
        ztd::logger::error("XML parsing error: {}", result.description());
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

    // ztd::logger::info("MIME XML | icon_name = {} | comment = {}", icon_name, comment);
    return std::array{icon_name, comment};
}

const std::array<std::string, 2>
mime_type_get_desc_icon(const std::string_view type)
{
    /*  //sfm 0.7.7+ FIXED:
     * According to specs on freedesktop.org, user_data_dir has
     * higher priority than system_data_dirs, but in most cases, there was
     * no file, or very few files in user_data_dir, so checking it first will
     * result in many unnecessary open() system calls, yealding bad performance.
     * Since the spec really sucks, we do not follow it here.
     */

    const std::string user_path =
        std::format("{}/mime/{}.xml", vfs::user_dirs->data_dir().string(), type);
    if (faccessat(0, user_path.data(), F_OK, AT_EACCESS) != -1)
    {
        const auto icon_data = mime_type_parse_xml_file(user_path, true);
        if (icon_data)
        {
            return icon_data.value();
        }
    }

    // look in system dirs
    for (const auto& sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const std::string sys_path = std::format("{}/mime/{}.xml", sys_dir.string(), type);
        if (faccessat(0, sys_path.data(), F_OK, AT_EACCESS) != -1)
        {
            const auto icon_data = mime_type_parse_xml_file(sys_path, false);
            if (icon_data)
            {
                return icon_data.value();
            }
        }
    }
    return {"", ""};
}

bool
mime_type_is_text(const std::string_view mime_type) noexcept
{
    if (mime_type == "application/pdf")
    {
        // seems to think this is XDG_MIME_TYPE_PLAIN_TEXT
        return false;
    }
    if (!mime_type.starts_with("text/") && !mime_type.starts_with("application/"))
    {
        return false;
    }
    return false;
}

bool
mime_type_is_executable(const std::string_view mime_type) noexcept
{
    /*
     * Only executable types can be executale.
     * Since some common types, such as application/x-shellscript,
     * are not in mime database, we have to add them ourselves.
     */
    return (mime_type != XDG_MIME_TYPE_UNKNOWN || mime_type == XDG_MIME_TYPE_EXECUTABLE ||
            mime_type == "application/x-shellscript");
}

// Taken from file-roller .desktop file
inline constexpr std::array<std::string_view, 65> archive_mime_types{
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

bool
mime_type_is_archive(const std::string_view mime_type) noexcept
{
    return std::ranges::find(archive_mime_types, mime_type) != archive_mime_types.cend();
}

bool
mime_type_is_image(const std::string_view mime_type) noexcept
{
    return mime_type.starts_with("image/");
}

bool
mime_type_is_video(const std::string_view mime_type) noexcept
{
    return mime_type.starts_with("video/");
}

bool
mime_type_is_unknown(const std::string_view mime_type) noexcept
{
    return mime_type == XDG_MIME_TYPE_UNKNOWN;
}
