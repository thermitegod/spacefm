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

#include <vector>

#include <atomic>

#include <gdk/gdk.h>

#include "mime-type/mime-type.hxx"

#define VFS_MIME_TYPE(obj) (static_cast<vfs::mime_type>(obj))

struct VFSMimeType
{
    std::string type;        // mime_type-type string
    std::string description; // description of the mimele type
    GdkPixbuf* big_icon;
    GdkPixbuf* small_icon;

    void ref_inc();
    void ref_dec();
    u32 ref_count();

  private:
    std::atomic<u32> n_ref{0};
};

namespace vfs
{
    using mime_type = ztd::raw_ptr<VFSMimeType>;
} // namespace vfs

void vfs_mime_type_init();

void vfs_mime_type_clean();

vfs::mime_type vfs_mime_type_get_from_file(std::string_view file_path);

vfs::mime_type vfs_mime_type_get_from_type(std::string_view type);

vfs::mime_type vfs_mime_type_new(std::string_view type_name);
void vfs_mime_type_ref(vfs::mime_type mime_type);
void vfs_mime_type_unref(vfs::mime_type mime_type);

GdkPixbuf* vfs_mime_type_get_icon(vfs::mime_type mime_type, bool big);

void vfs_mime_type_set_icon_size_big(i32 size);
void vfs_mime_type_set_icon_size_small(i32 size);

i32 vfs_mime_type_get_icon_size_big();
i32 vfs_mime_type_get_icon_size_small();

/* Get mime-type string */
const char* vfs_mime_type_get_type(vfs::mime_type mime_type);

/* Get human-readable description of mime-type */
const char* vfs_mime_type_get_description(vfs::mime_type mime_type);

/*
 * Get available actions (applications) for this mime-type
 * returned vector should be freed with g_strfreev when not needed.
 */
const std::vector<std::string> vfs_mime_type_get_actions(vfs::mime_type mime_type);

/* returned string should be freed with g_strfreev when not needed. */
char* vfs_mime_type_get_default_action(vfs::mime_type mime_type);

void vfs_mime_type_set_default_action(vfs::mime_type mime_type, std::string_view desktop_id);

void vfs_mime_type_remove_action(vfs::mime_type mime_type, std::string_view desktop_id);

/* If user-custom desktop file is created, it is returned in custom_desktop. */
void vfs_mime_type_add_action(vfs::mime_type mime_type, std::string_view desktop_id,
                              char** custom_desktop);

void vfs_mime_type_append_action(std::string_view type, std::string_view desktop_id);

GList* vfs_mime_type_add_reload_cb(GFreeFunc cb, void* user_data);

void vfs_mime_type_remove_reload_cb(GList* cb);

char* vfs_mime_type_locate_desktop_file(std::string_view dir, std::string_view desktop_id);
