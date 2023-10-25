/**
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
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

#include <span>

#include <optional>

#include "vfs/vfs-device.hxx"

#define VFS_VOLUME(obj) (static_cast<vfs::volume>(obj))

namespace vfs
{
    enum volume_state
    {
        added,
        removed,
        mounted,   // Not implemented
        unmounted, // Not implemented
        eject,
        changed,
    };

    enum volume_device_type
    {
        block,
        network,
        other, // eg fuseiso mounted file
    };
} // namespace vfs

// forward declare types
struct VFSVolume;

namespace vfs
{
    using volume = ztd::raw_ptr<VFSVolume>;

    using volume_callback = void (*)(vfs::volume vol, vfs::volume_state state, void* user_data);
} // namespace vfs

struct VFSVolume
{
  public:
    VFSVolume() = default;
    VFSVolume(const std::shared_ptr<vfs::device>& device);
    ~VFSVolume() = default;

  public:
    const std::string_view display_name() const noexcept;
    const std::string_view mount_point() const noexcept;
    const std::string_view device_file() const noexcept;
    const std::string_view fstype() const noexcept;
    const std::string_view icon() const noexcept;
    const std::string_view udi() const noexcept;
    const std::string_view label() const noexcept;

    dev_t devnum() const noexcept;
    u64 size() const noexcept;

    const std::optional<std::string> device_mount_cmd() noexcept;
    const std::optional<std::string> device_unmount_cmd() noexcept;

    bool is_device_type(vfs::volume_device_type type) const noexcept;

    bool is_mounted() const noexcept;
    bool is_removable() const noexcept;
    bool is_mountable() const noexcept;

    bool is_user_visible() const noexcept;

    bool is_optical() const noexcept;
    bool requires_eject() const noexcept;

    bool ever_mounted() const noexcept;

    // private:
    void set_info() noexcept;

    void device_added() noexcept;

  private:
    dev_t devnum_{0};
    std::string device_file_{};
    std::string udi_{};
    std::string disp_name_{};
    std::string icon_{};
    std::string mount_point_{};
    u64 size_{0};
    std::string label_{};
    std::string fstype_{};

    vfs::volume_device_type device_type_;

    bool is_mounted_{false};
    bool is_removable_{false};
    bool is_mountable_{false};

    bool is_user_visible_{false};

    bool is_optical_{false};
    bool requires_eject_{false};

    bool ever_mounted_{false};
};

bool vfs_volume_init();
void vfs_volume_finalize();

const std::span<const vfs::volume> vfs_volume_get_all_volumes();

void vfs_volume_add_callback(vfs::volume_callback cb, void* user_data);
void vfs_volume_remove_callback(vfs::volume_callback cb, void* user_data);

vfs::volume vfs_volume_get_by_device(const std::string_view device_file);

bool vfs_volume_dir_avoid_changes(const std::filesystem::path& dir);

bool is_path_mountpoint(const std::filesystem::path& path);
