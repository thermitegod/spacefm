/*
 * SpaceFM vfs-volume.hxx
 *
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

#include <ctime>

#include <glib.h>

#include "settings.hxx"

#define VFS_VOLUME_CALLBACK_DATA(obj) (reinterpret_cast<VFSVolumeCallbackData*>(obj))

enum VFSVolumeState
{
    VFS_VOLUME_ADDED,
    VFS_VOLUME_REMOVED,
    VFS_VOLUME_MOUNTED,   /* Not implemented */
    VFS_VOLUME_UNMOUNTED, /* Not implemented */
    VFS_VOLUME_EJECT,
    VFS_VOLUME_CHANGED
};

struct VFSVolume;

typedef void (*VFSVolumeCallback)(VFSVolume* vol, VFSVolumeState state, void* user_data);

bool vfs_volume_init();

bool vfs_volume_finalize();

const GList* vfs_volume_get_all_volumes();

void vfs_volume_add_callback(VFSVolumeCallback cb, void* user_data);

void vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data);

const char* vfs_volume_get_disp_name(VFSVolume* vol);

const char* vfs_volume_get_mount_point(VFSVolume* vol);

const char* vfs_volume_get_device(VFSVolume* vol);

const char* vfs_volume_get_fstype(VFSVolume* vol);

const char* vfs_volume_get_icon(VFSVolume* vol);

bool vfs_volume_is_mounted(VFSVolume* vol);

/* HAL build also needs this for file handler
 * ptk_location_view_create_mount_point() */
struct netmount_t
{
    char* url;
    char* fstype;
    char* host;
    char* ip;
    char* port;
    char* user;
    char* pass;
    char* path;
};

enum VFSVolumeDeviceType
{
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_OTHER // eg fuseiso mounted file
};

struct VFSVolume
{
    dev_t devnum;
    int device_type;
    char* device_file;
    char* udi;
    char* disp_name;
    char* icon;
    char* mount_point;
    uint64_t size;
    char* label;
    char* fs_type;
    bool should_autounmount : 1; // a network or ISO file was mounted
    bool is_mounted : 1;
    bool is_removable : 1;
    bool is_mountable : 1;
    bool is_audiocd : 1;
    bool is_dvd : 1;
    bool is_blank : 1;
    bool requires_eject : 1;
    bool is_user_visible : 1;
    bool nopolicy : 1;
    bool is_optical : 1;
    bool is_floppy : 1;
    bool is_table : 1;
    bool ever_mounted : 1;
    bool inhibit_auto : 1;
    std::time_t automount_time;
    void* open_main_window;
};

char* vfs_volume_get_mount_command(VFSVolume* vol, char* default_options, bool* run_in_terminal);
char* vfs_volume_get_mount_options(VFSVolume* vol, char* options);
void vfs_volume_automount(VFSVolume* vol);
void vfs_volume_set_info(VFSVolume* volume);
char* vfs_volume_device_mount_cmd(VFSVolume* vol, const char* options, bool* run_in_terminal);
char* vfs_volume_device_unmount_cmd(VFSVolume* vol, bool* run_in_terminal);
const std::string vfs_volume_device_info(VFSVolume* vol);
char* vfs_volume_handler_cmd(int mode, int action, VFSVolume* vol, const char* options,
                             netmount_t* netmount, bool* run_in_terminal, char** mount_point);

int split_network_url(const char* url, netmount_t** netmount);
bool vfs_volume_dir_avoid_changes(const char* dir);
dev_t get_device_parent(dev_t dev);
bool path_is_mounted_mtab(const char* mtab_file, const char* path, char** device_file,
                          char** fs_type);
bool mtab_fstype_is_handled_by_protocol(const char* mtab_fstype);
VFSVolume* vfs_volume_get_by_device_or_point(const char* device_file, const char* point);
