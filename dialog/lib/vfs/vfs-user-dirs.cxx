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

#include <glibmm.h>
#include <gtkmm.h>

#include "vfs/vfs-user-dirs.hxx"

std::filesystem::path
vfs::user::desktop() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP);
}

std::filesystem::path
vfs::user::documents() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS);
}

std::filesystem::path
vfs::user::download() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD);
}

std::filesystem::path
vfs::user::music() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::MUSIC);
}

std::filesystem::path
vfs::user::pictures() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::PICTURES);
}

std::filesystem::path
vfs::user::public_share() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE);
}

std::filesystem::path
vfs::user::templates() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES);
}

std::filesystem::path
vfs::user::videos() noexcept
{
    return Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS);
}

std::filesystem::path
vfs::user::home() noexcept
{
    return Glib::get_home_dir();
}

std::filesystem::path
vfs::user::cache() noexcept
{
    return Glib::get_user_cache_dir();
}

std::filesystem::path
vfs::user::data() noexcept
{
    return Glib::get_user_data_dir();
}

std::filesystem::path
vfs::user::config() noexcept
{
    return Glib::get_user_config_dir();
}

std::filesystem::path
vfs::user::runtime() noexcept
{
    return Glib::get_user_runtime_dir();
}

namespace global
{
static std::filesystem::path config_path = vfs::user::config() / PACKAGE_NAME;
}

std::filesystem::path
vfs::program::config() noexcept
{
    return global::config_path;
}

void
vfs::program::config(const std::filesystem::path& path) noexcept
{
    global::config_path = path;
}

std::filesystem::path
vfs::program::tmp() noexcept
{
    return vfs::user::cache() / PACKAGE_NAME;
}
