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

#include <vector>

#include <memory>

#include <glibmm.h>
class VFSUserDirs
{
  public:
    const std::string& desktop_dir() const noexcept;
    const std::string& documents_dir() const noexcept;
    const std::string& download_dir() const noexcept;
    const std::string& music_dir() const noexcept;
    const std::string& pictures_dir() const noexcept;
    const std::string& public_share_dir() const noexcept;
    const std::string& template_dir() const noexcept;
    const std::string& videos_dir() const noexcept;

    const std::string& home_dir() const noexcept;
    const std::string& cache_dir() const noexcept;
    const std::string& data_dir() const noexcept;
    const std::string& config_dir() const noexcept;
    const std::string& runtime_dir() const noexcept;

    const std::vector<std::string>& system_data_dirs() const noexcept;

    const std::string& current_dir() const noexcept;

    void program_config_dir(const std::string_view config_dir) noexcept;
    const std::string& program_config_dir() const noexcept;

    void program_tmp_dir(const std::string_view tmp_dir) noexcept;
    const std::string& program_tmp_dir() const noexcept;

  private:
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
    const std::string current{Glib::get_current_dir()};

    // Program config dir
    std::string program_config{Glib::build_filename(this->user_config, PACKAGE_NAME)};
    std::string tmp{Glib::build_filename(this->user_cache, PACKAGE_NAME)};
};

namespace vfs
{
    using user_dirs_t = std::unique_ptr<VFSUserDirs>;

    const extern user_dirs_t user_dirs;
} // namespace vfs
