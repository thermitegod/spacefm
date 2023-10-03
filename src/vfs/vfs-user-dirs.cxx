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

#include <filesystem>

#include <memory>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

const vfs::user_dirs_t vfs::user_dirs = std::make_unique<VFSUserDirs>();

const std::filesystem::path&
VFSUserDirs::desktop_dir() const noexcept
{
    return this->user_desktop;
}

const std::filesystem::path&
VFSUserDirs::documents_dir() const noexcept
{
    return this->user_documents;
}

const std::filesystem::path&
VFSUserDirs::download_dir() const noexcept
{
    return this->user_download;
}

const std::filesystem::path&
VFSUserDirs::music_dir() const noexcept
{
    return this->user_music;
}

const std::filesystem::path&
VFSUserDirs::pictures_dir() const noexcept
{
    return this->user_pictures;
}

const std::filesystem::path&
VFSUserDirs::public_share_dir() const noexcept
{
    return this->user_share;
}

const std::filesystem::path&
VFSUserDirs::template_dir() const noexcept
{
    return this->user_template;
}

const std::filesystem::path&
VFSUserDirs::videos_dir() const noexcept
{
    return this->user_videos;
}

const std::filesystem::path&
VFSUserDirs::home_dir() const noexcept
{
    return this->user_home;
}

const std::filesystem::path&
VFSUserDirs::cache_dir() const noexcept
{
    return this->user_cache;
}

const std::filesystem::path&
VFSUserDirs::data_dir() const noexcept
{
    return this->user_data;
}

const std::filesystem::path&
VFSUserDirs::config_dir() const noexcept
{
    return this->user_config;
}

const std::filesystem::path&
VFSUserDirs::runtime_dir() const noexcept
{
    return this->user_runtime;
}

const std::vector<std::string>&
VFSUserDirs::system_data_dirs() const noexcept
{
    return this->sys_data;
}

const std::filesystem::path&
VFSUserDirs::current_dir() const noexcept
{
    return this->current;
}

void
VFSUserDirs::program_config_dir(const std::filesystem::path& config_dir) noexcept
{
    if (!std::filesystem::exists(config_dir))
    {
        std::filesystem::create_directories(config_dir);
        std::filesystem::permissions(config_dir, std::filesystem::perms::owner_all);
    }
    this->program_config = std::filesystem::canonical(config_dir);
}

const std::filesystem::path&
VFSUserDirs::program_config_dir() const noexcept
{
    return this->program_config;
}

const std::filesystem::path&
VFSUserDirs::program_tmp_dir() const noexcept
{
    if (!std::filesystem::exists(this->tmp))
    {
        std::filesystem::create_directories(this->tmp);
        std::filesystem::permissions(this->tmp, std::filesystem::perms::owner_all);
    }

    return this->tmp;
}

void
VFSUserDirs::program_tmp_dir(const std::filesystem::path& tmp_dir) noexcept
{
    this->tmp = tmp_dir;

    if (!std::filesystem::exists(this->tmp))
    {
        std::filesystem::create_directories(this->tmp);
        std::filesystem::permissions(this->tmp, std::filesystem::perms::owner_all);
    }
}
