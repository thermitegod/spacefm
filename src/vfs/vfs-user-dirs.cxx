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

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

const std::filesystem::path
vfs::user::desktop() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DESKTOP);
#endif
}

const std::filesystem::path
vfs::user::documents() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOCUMENTS);
#endif
}

const std::filesystem::path
vfs::user::download() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOWNLOAD);
#endif
}

const std::filesystem::path
vfs::user::music() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::MUSIC);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_MUSIC);
#endif
}

const std::filesystem::path
vfs::user::pictures() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::PICTURES);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PICTURES);
#endif
}

const std::filesystem::path
vfs::user::public_share() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PUBLIC_SHARE);
#endif
}

const std::filesystem::path
vfs::user::templates() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_TEMPLATES);
#endif
}

const std::filesystem::path
vfs::user::videos() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_VIDEOS);
#endif
}

const std::filesystem::path
vfs::user::home() noexcept
{
    return Glib::get_home_dir();
}

const std::filesystem::path
vfs::user::cache() noexcept
{
    return Glib::get_user_cache_dir();
}

const std::filesystem::path
vfs::user::data() noexcept
{
    return Glib::get_user_data_dir();
}

const std::filesystem::path
vfs::user::config() noexcept
{
    return Glib::get_user_config_dir();
}

const std::filesystem::path
vfs::user::runtime() noexcept
{
    return Glib::get_user_runtime_dir();
}

std::filesystem::path global_config_path = vfs::user::config() / PACKAGE_NAME;

const std::filesystem::path
vfs::program::config() noexcept
{
    return global_config_path;
}

void
vfs::program::config(const std::filesystem::path& path) noexcept
{
    global_config_path = path;
}

const std::filesystem::path
vfs::program::tmp() noexcept
{
    return vfs::user::cache() / PACKAGE_NAME;
}
