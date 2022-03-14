/*
 *
 * License: See COPYING file
 *
 */

#include <string>
#include <filesystem>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

struct VFSDirXDG
{
    // GUserDirectory
    // clang-format off
    const std::string user_desktop{Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP)};
    const std::string user_documents{Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS)};
    const std::string user_download{Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD)};
    const std::string user_music{Glib::get_user_special_dir(Glib::UserDirectory::MUSIC)};
    const std::string user_pictures{Glib::get_user_special_dir(Glib::UserDirectory::PICTURES)};
    const std::string user_public_share{Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE)};
    const std::string user_template{Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES)};
    const std::string user_videos{Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS)};
    // clang-format on

    // User
    const std::string user_home{Glib::get_home_dir()};
    const std::string user_cache{Glib::get_user_cache_dir()};
    const std::string user_data{Glib::get_user_data_dir()};
    const std::string user_config{Glib::get_user_config_dir()};
    const std::string user_runtime{Glib::get_user_runtime_dir()};

    // System
    const std::vector<std::string> sys_data{Glib::get_system_data_dirs()};
};

VFSDirXDG vfs_dir_xdg;

const std::string&
vfs_user_desktop_dir() noexcept
{
    return vfs_dir_xdg.user_desktop;
}

const std::string&
vfs_user_documents_dir() noexcept
{
    return vfs_dir_xdg.user_documents;
}

const std::string&
vfs_user_download_dir() noexcept
{
    return vfs_dir_xdg.user_download;
}

const std::string&
vfs_user_music_dir() noexcept
{
    return vfs_dir_xdg.user_music;
}

const std::string&
vfs_user_pictures_dir() noexcept
{
    return vfs_dir_xdg.user_pictures;
}

const std::string&
vfs_user_public_share_dir() noexcept
{
    return vfs_dir_xdg.user_public_share;
}

const std::string&
vfs_user_template_dir() noexcept
{
    return vfs_dir_xdg.user_template;
}

const std::string&
vfs_user_videos_dir() noexcept
{
    return vfs_dir_xdg.user_videos;
}

const std::string&
vfs_user_home_dir() noexcept
{
    return vfs_dir_xdg.user_home;
}

const std::string&
vfs_user_cache_dir() noexcept
{
    return vfs_dir_xdg.user_cache;
}

const std::string&
vfs_user_data_dir() noexcept
{
    return vfs_dir_xdg.user_data;
}

const std::string&
vfs_user_config_dir() noexcept
{
    return vfs_dir_xdg.user_config;
}

const std::string&
vfs_user_runtime_dir() noexcept
{
    return vfs_dir_xdg.user_runtime;
}

const std::vector<std::string>&
vfs_system_data_dir() noexcept
{
    return vfs_dir_xdg.sys_data;
}

std::string
vfs_current_dir() noexcept
{
    return Glib::get_current_dir();
}
