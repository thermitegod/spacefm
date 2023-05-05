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

#include <vector>

#include <memory>

#include <optional>

#include "settings.hxx"

#define VFS_VOLUME(obj) (static_cast<vfs::volume>(obj))

enum VFSVolumeState
{
    ADDED,
    REMOVED,
    MOUNTED,   // Not implemented
    UNMOUNTED, // Not implemented
    EJECT,
    CHANGED,
};

enum VFSVolumeDeviceType
{
    BLOCK,
    NETWORK,
    OTHER, // eg fuseiso mounted file
};

// forward declare types
struct VFSVolume;
struct VFSMimeType;

namespace vfs
{
    using volume = ztd::raw_ptr<VFSVolume>;
} // namespace vfs

struct VFSVolume
{
  public:
    VFSVolume() = default;
    ~VFSVolume() = default;

    VFSVolumeDeviceType device_type;

    dev_t devnum{0};
    std::string device_file{};
    std::string udi{};
    std::string disp_name{};
    std::string icon{};
    std::string mount_point{};
    u64 size{0};
    std::string label{};
    std::string fs_type{};

    bool is_mounted{false};
    bool is_removable{false};
    bool is_mountable{false};

    bool is_user_visible{false};

    bool is_optical{false};
    bool requires_eject{false};

    bool ever_mounted{false};

  public:
    const std::string get_disp_name() const noexcept;
    const std::string get_mount_point() const noexcept;
    const std::string get_device_file() const noexcept;
    const std::string get_fstype() const noexcept;
    const std::string get_icon() const noexcept;

    const std::optional<std::string> device_mount_cmd() noexcept;
    const std::optional<std::string> device_unmount_cmd() noexcept;

    // private:
    void set_info() noexcept;

    void device_added() noexcept;
};

using VFSVolumeCallback = void (*)(vfs::volume vol, VFSVolumeState state, void* user_data);

bool vfs_volume_init();
void vfs_volume_finalize();

const std::vector<vfs::volume>& vfs_volume_get_all_volumes();

void vfs_volume_add_callback(VFSVolumeCallback cb, void* user_data);
void vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data);

vfs::volume vfs_volume_get_by_device(const std::string_view device_file);

bool vfs_volume_dir_avoid_changes(const std::string_view dir);

bool is_path_mountpoint(const std::string_view path);
