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

#include "vfs/user-dirs.hxx"

std::filesystem::path
vfs::user::desktop() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DESKTOP);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DESKTOP);
#endif
}

std::filesystem::path
vfs::user::documents() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOCUMENTS);
#endif
}

std::filesystem::path
vfs::user::download() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_DOWNLOAD);
#endif
}

std::filesystem::path
vfs::user::music() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::MUSIC);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_MUSIC);
#endif
}

std::filesystem::path
vfs::user::pictures() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::PICTURES);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PICTURES);
#endif
}

std::filesystem::path
vfs::user::public_share() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::PUBLIC_SHARE);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_PUBLIC_SHARE);
#endif
}

std::filesystem::path
vfs::user::templates() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::TEMPLATES);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_TEMPLATES);
#endif
}

std::filesystem::path
vfs::user::videos() noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    return Glib::get_user_special_dir(Glib::UserDirectory::VIDEOS);
#elif (GTK_MAJOR_VERSION == 3)
    return Glib::get_user_special_dir(Glib::UserDirectory::USER_DIRECTORY_VIDEOS);
#endif
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

vfs::user::thumbnail_cache_data
vfs::user::thumbnail_cache() noexcept
{
    static const auto data = thumbnail_cache_data{
        .parent = vfs::user::cache() / "thumbnails",
        .normal = vfs::user::cache() / "thumbnails/normal",
        .large = vfs::user::cache() / "thumbnails/large",
        .x_large = vfs::user::cache() / "thumbnails/x-large",
        .xx_large = vfs::user::cache() / "thumbnails/xx-large",
        .fail = vfs::user::cache() /
                std::format("thumbnails/fail/{}-{}", PACKAGE_NAME, PACKAGE_VERSION),
    };
    return data;
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
vfs::program::data() noexcept
{
    static auto path = vfs::user::data() / PACKAGE_NAME;
    return path;
}

std::filesystem::path
vfs::program::tmp() noexcept
{
    return vfs::user::cache() / PACKAGE_NAME;
}
