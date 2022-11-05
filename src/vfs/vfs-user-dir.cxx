/**
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

#include <string>
#include <string_view>

#include <filesystem>

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/etc.hxx"

#include "vfs/vfs-user-dir.hxx"

class VFSDirXDG
{
  public:
    // GUserDirectory
    const std::string user_desktop{Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP)};
    const std::string user_documents{Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS)};
    const std::string user_download{Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD)};
    const std::string user_music{Glib::get_user_special_dir(Glib::UserDirectory::MUSIC)};
    const std::string user_pictures{Glib::get_user_special_dir(Glib::UserDirectory::PICTURES)};
    const std::string user_share{Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE)};
    const std::string user_template{Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES)};
    const std::string user_videos{Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS)};

    // User
    const std::string user_home{Glib::get_home_dir()};
    const std::string user_cache{Glib::get_user_cache_dir()};
    const std::string user_data{Glib::get_user_data_dir()};
    const std::string user_config{Glib::get_user_config_dir()};
    const std::string user_runtime{Glib::get_user_runtime_dir()};

    // System
    const std::vector<std::string> sys_data{Glib::get_system_data_dirs()};

    // Current runtime dir
    const std::string current_dir{Glib::get_current_dir()};

    // Program config dir
    std::string config_dir{Glib::build_filename(user_config, PACKAGE_NAME)};
    const std::string tmp_dir{Glib::build_filename(etc_settings.get_tmp_dir(), PACKAGE_NAME)};
};

const std::unique_ptr user_dirs = std::make_unique<VFSDirXDG>();

const std::string&
vfs_user_desktop_dir() noexcept
{
    return user_dirs->user_desktop;
}

const std::string&
vfs_user_documents_dir() noexcept
{
    return user_dirs->user_documents;
}

const std::string&
vfs_user_download_dir() noexcept
{
    return user_dirs->user_download;
}

const std::string&
vfs_user_music_dir() noexcept
{
    return user_dirs->user_music;
}

const std::string&
vfs_user_pictures_dir() noexcept
{
    return user_dirs->user_pictures;
}

const std::string&
vfs_user_public_share_dir() noexcept
{
    return user_dirs->user_share;
}

const std::string&
vfs_user_template_dir() noexcept
{
    return user_dirs->user_template;
}

const std::string&
vfs_user_videos_dir() noexcept
{
    return user_dirs->user_videos;
}

const std::string&
vfs_user_home_dir() noexcept
{
    return user_dirs->user_home;
}

const std::string&
vfs_user_cache_dir() noexcept
{
    return user_dirs->user_cache;
}

const std::string&
vfs_user_data_dir() noexcept
{
    return user_dirs->user_data;
}

const std::string&
vfs_user_config_dir() noexcept
{
    return user_dirs->user_config;
}

const std::string&
vfs_user_runtime_dir() noexcept
{
    return user_dirs->user_runtime;
}

const std::vector<std::string>&
vfs_system_data_dir() noexcept
{
    return user_dirs->sys_data;
}

const std::string&
vfs_current_dir() noexcept
{
    return user_dirs->current_dir;
}

void
vfs_user_set_config_dir(std::string_view config_dir) noexcept
{
    user_dirs->config_dir = config_dir.data();
}

const std::string&
vfs_user_get_config_dir() noexcept
{
    return user_dirs->config_dir;
}

const std::string&
vfs_user_get_tmp_dir() noexcept
{
    if (!std::filesystem::exists(user_dirs->tmp_dir))
    {
        std::filesystem::create_directories(user_dirs->tmp_dir);
        std::filesystem::permissions(user_dirs->tmp_dir, std::filesystem::perms::owner_all);
    }

    return user_dirs->tmp_dir;
}
