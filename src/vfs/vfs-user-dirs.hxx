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

#pragma once

#include <filesystem>

#include <vector>

#include <span>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

namespace vfs
{
namespace impl
{
struct user_dirs
{
    user_dirs();

    const std::filesystem::path& desktop_dir() const noexcept;
    const std::filesystem::path& documents_dir() const noexcept;
    const std::filesystem::path& download_dir() const noexcept;
    const std::filesystem::path& music_dir() const noexcept;
    const std::filesystem::path& pictures_dir() const noexcept;
    const std::filesystem::path& public_share_dir() const noexcept;
    const std::filesystem::path& template_dir() const noexcept;
    const std::filesystem::path& videos_dir() const noexcept;

    const std::filesystem::path& home_dir() const noexcept;
    const std::filesystem::path& cache_dir() const noexcept;
    const std::filesystem::path& data_dir() const noexcept;
    const std::filesystem::path& config_dir() const noexcept;
    const std::filesystem::path& runtime_dir() const noexcept;

    const std::span<const std::filesystem::path> system_data_dirs() const noexcept;

    const std::filesystem::path& current_dir() const noexcept;

    void program_config_dir(const std::filesystem::path& config_dir) noexcept;
    const std::filesystem::path& program_config_dir() const noexcept;

    void program_tmp_dir(const std::filesystem::path& tmp_dir) noexcept;
    const std::filesystem::path& program_tmp_dir() const noexcept;

  private:
    // GUserDirectory
#if (GTK_MAJOR_VERSION == 4)
    const std::filesystem::path user_desktop_{
        Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP)};
    const std::filesystem::path user_documents_{
        Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS)};
    const std::filesystem::path user_download_{
        Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD)};
    const std::filesystem::path user_music_{Glib::get_user_special_dir(Glib::UserDirectory::MUSIC)};
    const std::filesystem::path user_pictures_{
        Glib::get_user_special_dir(Glib::UserDirectory::PICTURES)};
    const std::filesystem::path user_share_{
        Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE)};
    const std::filesystem::path user_template_{
        Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES)};
    const std::filesystem::path user_videos_{
        Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS)};
#elif (GTK_MAJOR_VERSION == 3)
    const std::filesystem::path user_desktop_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DESKTOP)};
    const std::filesystem::path user_documents_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOCUMENTS)};
    const std::filesystem::path user_download_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOWNLOAD)};
    const std::filesystem::path user_music_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_MUSIC)};
    const std::filesystem::path user_pictures_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PICTURES)};
    const std::filesystem::path user_share_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PUBLIC_SHARE)};
    const std::filesystem::path user_template_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_TEMPLATES)};
    const std::filesystem::path user_videos_{
        Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_VIDEOS)};
#endif

    // User
    const std::filesystem::path user_home_{Glib::get_home_dir()};
    const std::filesystem::path user_cache_{Glib::get_user_cache_dir()};
    const std::filesystem::path user_data_{Glib::get_user_data_dir()};
    const std::filesystem::path user_config_{Glib::get_user_config_dir()};
    const std::filesystem::path user_runtime_{Glib::get_user_runtime_dir()};

    // System
    std::vector<std::filesystem::path> sys_data_{};

    // Current runtime dir
    const std::filesystem::path current_{Glib::get_current_dir()};

    // Program config dir
    std::filesystem::path program_config_{this->user_config_ / PACKAGE_NAME};
    std::filesystem::path tmp_{this->user_cache_ / PACKAGE_NAME};
};
} // namespace impl

using user_dirs_t = std::unique_ptr<vfs::impl::user_dirs>;

const extern user_dirs_t user_dirs;
} // namespace vfs
