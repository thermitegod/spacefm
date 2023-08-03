/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <optional>

#include <memory>

#include <gdk/gdk.h>

#include <ztd/ztd.hxx>

#include "mime-type/mime-type.hxx"

struct VFSMimeType
{
  public:
    VFSMimeType(const std::string_view type_name);
    ~VFSMimeType();

    GdkPixbuf* icon(bool big) noexcept;

    // Get mime-type string
    const std::string_view type() const noexcept;

    // Get human-readable description of mime-type
    const std::string_view description() noexcept;

    // Get available actions (applications) for this mime-type
    // returned vector should be freed with g_strfreev when not needed.
    const std::vector<std::string> actions() const noexcept;

    // returned string should be freed with g_strfreev when not needed.
    const std::optional<std::string> default_action() const noexcept;

    void set_default_action(const std::string_view desktop_id) noexcept;

    void remove_action(const std::string_view desktop_id) noexcept;

    // If user-custom desktop file is created, it is returned in custom_desktop.
    const std::string add_action(const std::string_view desktop_id) noexcept;

  private:
    std::string type_{};        // mime_type-type string
    std::string description_{}; // description of the mimele type
    GdkPixbuf* big_icon_{nullptr};
    GdkPixbuf* small_icon_{nullptr};
    i32 icon_size_big_{0};
    i32 icon_size_small_{0};
};

namespace vfs
{
    using mime_type_callback_entry = void (*)();

    using mime_type = std::shared_ptr<VFSMimeType>;
} // namespace vfs

void vfs_mime_type_init();
void vfs_mime_type_finalize();

vfs::mime_type vfs_mime_type_new(const std::string_view type_name);

vfs::mime_type vfs_mime_type_get_from_file(const std::filesystem::path& file_path);
vfs::mime_type vfs_mime_type_get_from_type(const std::string_view type);

//////////////////////

void vfs_mime_type_append_action(const std::string_view type, const std::string_view desktop_id);

const std::optional<std::filesystem::path>
vfs_mime_type_locate_desktop_file(const std::string_view desktop_id);
const std::optional<std::filesystem::path>
vfs_mime_type_locate_desktop_file(const std::filesystem::path& dir,
                                  const std::string_view desktop_id);
