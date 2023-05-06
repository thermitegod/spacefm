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

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <memory>

#include <glibmm.h>
struct VFSUserDirs
{
  public:
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

    const std::vector<std::string>& system_data_dirs() const noexcept;

    const std::filesystem::path& current_dir() const noexcept;

    void program_config_dir(const std::string_view config_dir) noexcept;
    const std::filesystem::path& program_config_dir() const noexcept;

    void program_tmp_dir(const std::string_view tmp_dir) noexcept;
    const std::filesystem::path& program_tmp_dir() const noexcept;

  private:
    // GUserDirectory
    // clang-format off
    const std::filesystem::path user_desktop{Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP)};
    const std::filesystem::path user_documents{Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS)};
    const std::filesystem::path user_download{Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD)};
    const std::filesystem::path user_music{Glib::get_user_special_dir(Glib::UserDirectory::MUSIC)};
    const std::filesystem::path user_pictures{Glib::get_user_special_dir(Glib::UserDirectory::PICTURES)};
    const std::filesystem::path user_share{Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE)};
    const std::filesystem::path user_template{Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES)};
    const std::filesystem::path user_videos{Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS)};
    // clang-format on

    // User
    const std::filesystem::path user_home{Glib::get_home_dir()};
    const std::filesystem::path user_cache{Glib::get_user_cache_dir()};
    const std::filesystem::path user_data{Glib::get_user_data_dir()};
    const std::filesystem::path user_config{Glib::get_user_config_dir()};
    const std::filesystem::path user_runtime{Glib::get_user_runtime_dir()};

    // System
    const std::vector<std::string> sys_data{Glib::get_system_data_dirs()};

    // Current runtime dir
    const std::filesystem::path current{Glib::get_current_dir()};

    // Program config dir
    std::filesystem::path program_config{this->user_config / PACKAGE_NAME};
    std::filesystem::path tmp{this->user_cache / PACKAGE_NAME};
};

namespace vfs
{
    using user_dirs_t = std::unique_ptr<VFSUserDirs>;

    const extern user_dirs_t user_dirs;
} // namespace vfs
