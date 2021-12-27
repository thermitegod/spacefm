/*
 *
 * License: See COPYING file
 *
 */

#include <string>
#include <filesystem>

#include <glib.h>

// #include "logger.hxx"

#include "vfs-user-dir.hxx"

struct VFSDirXDG
{
    // GUserDirectory
    const std::string user_desktop{g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP)};
    const std::string user_template{g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES)};

    // User
    const std::string user_home{g_get_home_dir()};
    const std::string user_cache{g_get_user_cache_dir()};
    const std::string user_data{g_get_user_data_dir()};
    const std::string user_config{g_get_user_config_dir()};
    const std::string user_runtime{g_get_user_runtime_dir()};

    // System
    const char* const* sys_data{g_get_system_data_dirs()};
};

VFSDirXDG vfs_dir_xdg;

const char*
vfs_user_desktop_dir()
{
    return vfs_dir_xdg.user_desktop.c_str();
}

const char*
vfs_user_template_dir()
{
    return vfs_dir_xdg.user_template.c_str();
}

const char*
vfs_user_home_dir()
{
    return vfs_dir_xdg.user_home.c_str();
}

const char*
vfs_user_cache_dir()
{
    return vfs_dir_xdg.user_cache.c_str();
}

const char*
vfs_user_data_dir()
{
    return vfs_dir_xdg.user_data.c_str();
}

const char*
vfs_user_config_dir()
{
    return vfs_dir_xdg.user_config.c_str();
}

const char*
vfs_user_runtime_dir()
{
    return vfs_dir_xdg.user_runtime.c_str();
}

const char* const*
vfs_system_data_dir()
{
    return vfs_dir_xdg.sys_data;
}

const char*
vfs_current_dir()
{
    return g_get_current_dir();
}
