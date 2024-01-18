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

#include <filesystem>

#include <memory>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

const vfs::user_dirs_t vfs::user_dirs = std::make_unique<vfs::impl::user_dirs>();

vfs::impl::user_dirs::user_dirs()
{
    // typechange from std::string to std::filesystem::path
    for (const auto& sys_data : Glib::get_system_data_dirs())
    {
        this->sys_data_.emplace_back(sys_data);
    }
}

const std::filesystem::path&
vfs::impl::user_dirs::desktop_dir() const noexcept
{
    return this->user_desktop_;
}

const std::filesystem::path&
vfs::impl::user_dirs::documents_dir() const noexcept
{
    return this->user_documents_;
}

const std::filesystem::path&
vfs::impl::user_dirs::download_dir() const noexcept
{
    return this->user_download_;
}

const std::filesystem::path&
vfs::impl::user_dirs::music_dir() const noexcept
{
    return this->user_music_;
}

const std::filesystem::path&
vfs::impl::user_dirs::pictures_dir() const noexcept
{
    return this->user_pictures_;
}

const std::filesystem::path&
vfs::impl::user_dirs::public_share_dir() const noexcept
{
    return this->user_share_;
}

const std::filesystem::path&
vfs::impl::user_dirs::template_dir() const noexcept
{
    return this->user_template_;
}

const std::filesystem::path&
vfs::impl::user_dirs::videos_dir() const noexcept
{
    return this->user_videos_;
}

const std::filesystem::path&
vfs::impl::user_dirs::home_dir() const noexcept
{
    return this->user_home_;
}

const std::filesystem::path&
vfs::impl::user_dirs::cache_dir() const noexcept
{
    return this->user_cache_;
}

const std::filesystem::path&
vfs::impl::user_dirs::data_dir() const noexcept
{
    return this->user_data_;
}

const std::filesystem::path&
vfs::impl::user_dirs::config_dir() const noexcept
{
    return this->user_config_;
}

const std::filesystem::path&
vfs::impl::user_dirs::runtime_dir() const noexcept
{
    return this->user_runtime_;
}

const std::span<const std::filesystem::path>
vfs::impl::user_dirs::system_data_dirs() const noexcept
{
    return this->sys_data_;
}

const std::filesystem::path&
vfs::impl::user_dirs::current_dir() const noexcept
{
    return this->current_;
}

void
vfs::impl::user_dirs::program_config_dir(const std::filesystem::path& config_dir) noexcept
{
    if (!std::filesystem::exists(config_dir))
    {
        std::filesystem::create_directories(config_dir);
        std::filesystem::permissions(config_dir, std::filesystem::perms::owner_all);
    }
    this->program_config_ = std::filesystem::canonical(config_dir);
}

const std::filesystem::path&
vfs::impl::user_dirs::program_config_dir() const noexcept
{
    return this->program_config_;
}

const std::filesystem::path&
vfs::impl::user_dirs::program_tmp_dir() const noexcept
{
    if (!std::filesystem::exists(this->tmp_))
    {
        std::filesystem::create_directories(this->tmp_);
        std::filesystem::permissions(this->tmp_, std::filesystem::perms::owner_all);
    }
    return this->tmp_;
}

void
vfs::impl::user_dirs::program_tmp_dir(const std::filesystem::path& tmp_dir) noexcept
{
    this->tmp_ = tmp_dir;
    if (!std::filesystem::exists(this->tmp_))
    {
        std::filesystem::create_directories(this->tmp_);
        std::filesystem::permissions(this->tmp_, std::filesystem::perms::owner_all);
    }
}
