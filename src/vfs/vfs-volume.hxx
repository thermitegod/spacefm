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

#include <chrono>

#include <glib.h>

#include "settings.hxx"

#define VFS_VOLUME(obj) (static_cast<vfs::volume>(obj))

enum VFSVolumeState
{
    VFS_VOLUME_ADDED,
    VFS_VOLUME_REMOVED,
    VFS_VOLUME_MOUNTED,   // Not implemented
    VFS_VOLUME_UNMOUNTED, // Not implemented
    VFS_VOLUME_EJECT,
    VFS_VOLUME_CHANGED
};

enum VFSVolumeDeviceType
{
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_OTHER // eg fuseiso mounted file
};

enum class SplitNetworkURL
{
    NOT_A_NETWORK_URL,
    VALID_NETWORK_URL,
    INVALID_NETWORK_URL,
};

struct netmount_t
{
    // netmount_t();
    // ~netmount_t();

    const char* url{nullptr};
    const char* fstype{nullptr};
    const char* host{nullptr};
    const char* ip{nullptr};
    const char* port{nullptr};
    const char* user{nullptr};
    const char* pass{nullptr};
    const char* path{nullptr};
};

// forward declare types
struct VFSVolume;

namespace vfs
{
    using volume = ztd::raw_ptr<VFSVolume>;
} // namespace vfs

struct VFSVolume
{
    VFSVolume();
    ~VFSVolume();

    dev_t devnum;
    i32 device_type;
    char* device_file;
    char* udi;
    char* disp_name;
    std::string icon;
    char* mount_point;
    u64 size;
    std::string label;
    char* fs_type;

    bool should_autounmount{false}; // a network or ISO file was mounted
    bool is_mounted{false};
    bool is_removable{false};
    bool is_mountable{false};
    bool is_audiocd{false};
    bool is_dvd{false};
    bool is_blank{false};
    bool requires_eject{false};
    bool is_user_visible{false};
    bool nopolicy{false};
    bool is_optical{false};
    bool is_floppy{false};
    bool is_table{false};
    bool ever_mounted{false};
    bool inhibit_auto{false};

    std::time_t automount_time;
    void* open_main_window;
};

using VFSVolumeCallback = void (*)(vfs::volume vol, VFSVolumeState state, void* user_data);

bool vfs_volume_init();

void vfs_volume_finalize();

const std::vector<vfs::volume> vfs_volume_get_all_volumes();

void vfs_volume_add_callback(VFSVolumeCallback cb, void* user_data);
void vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data);

const char* vfs_volume_get_disp_name(vfs::volume vol);
const char* vfs_volume_get_mount_point(vfs::volume vol);
const char* vfs_volume_get_device(vfs::volume vol);
const char* vfs_volume_get_fstype(vfs::volume vol);
const char* vfs_volume_get_icon(vfs::volume vol);

bool vfs_volume_is_mounted(vfs::volume vol);

char* vfs_volume_get_mount_command(vfs::volume vol, char* default_options, bool* run_in_terminal);
char* vfs_volume_get_mount_options(vfs::volume vol, char* options);
void vfs_volume_automount(vfs::volume vol);
void vfs_volume_set_info(vfs::volume volume);
char* vfs_volume_device_mount_cmd(vfs::volume vol, const char* options, bool* run_in_terminal);
char* vfs_volume_device_unmount_cmd(vfs::volume vol, bool* run_in_terminal);
const std::string vfs_volume_device_info(vfs::volume vol);
char* vfs_volume_handler_cmd(i32 mode, i32 action, vfs::volume vol, const char* options,
                             netmount_t* netmount, bool* run_in_terminal, char** mount_point);

SplitNetworkURL split_network_url(const char* url, netmount_t** netmount);
bool vfs_volume_dir_avoid_changes(const char* dir);
dev_t get_device_parent(dev_t dev);
bool path_is_mounted_mtab(const char* mtab_file, const char* path, char** device_file,
                          char** fs_type);
bool mtab_fstype_is_handled_by_protocol(const char* mtab_fstype);
vfs::volume vfs_volume_get_by_device_or_point(const char* device_file, const char* point);
