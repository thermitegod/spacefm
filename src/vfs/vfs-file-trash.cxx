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

#include <chrono>

#include <filesystem>

#include <iostream>
#include <fstream>

#include <unistd.h>

#include <sys/stat.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-trash.hxx"

Trash* Trash::single_instance = nullptr;

Trash*
Trash::instance() noexcept
{
    if (!Trash::single_instance)
        Trash::single_instance = new Trash();
    return Trash::single_instance;
}

Trash::Trash() noexcept
{
    this->home_device = device(vfs_user_home_dir());

    const std::string user_trash_dir = Glib::build_filename(vfs_user_data_dir(), "/Trash");

    this->home_trash_dir = new TrashDir(user_trash_dir, this->home_device);

    this->trash_dirs[this->home_device] = this->home_trash_dir;
}

Trash::~Trash()
{
    // NOOP
}

dev_t
Trash::device(std::string_view path) noexcept
{
    dev_t dev = 0;
    struct stat statBuf;
    i32 result = stat(path.data(), &statBuf);
    dev = statBuf.st_dev;

    // stat failed
    if (result < 0)
        dev = static_cast<dev_t>(-1);

    return dev;
}

const std::string
Trash::toplevel(std::string_view path) noexcept
{
    const dev_t dev = device(path);

    std::string mount_path = path.data();
    std::string last_path;

    // LOG_INFO("dev mount {}", device(mount_path));
    // LOG_INFO("dev       {}", dev);

    // walk up the path until it gets to the root of the device
    while (device(mount_path) == dev)
    {
        last_path = mount_path;
        std::filesystem::path mount_parent = std::filesystem::path(mount_path).parent_path();
        mount_path = std::string(mount_parent);
    }

    // LOG_INFO("last path   {}", last_path);
    // LOG_INFO("mount point {}", mount_path);

    return last_path;
}

TrashDir*
Trash::trash_dir(std::string_view path) noexcept
{
    const dev_t dev = device(path);

    if (this->trash_dirs.count(dev))
        return this->trash_dirs[dev];

    // on another device cannot use $HOME trashcan
    const std::string top_dir = toplevel(path);
    const std::string uid = std::to_string(getuid());
    const std::string trashPath = Glib::build_filename(top_dir, fmt::format("/.Trash-{}", uid));

    TrashDir* trash_dir = new TrashDir(trashPath, dev);
    this->trash_dirs[dev] = trash_dir;

    return trash_dir;
}

bool
Trash::trash(std::string_view path) noexcept
{
    TrashDir* trash_dir = instance()->trash_dir(path);

    if (!trash_dir)
        return false;

    if (std::filesystem::is_symlink(path))
    {
        // TODO std::filesystem::rename does not support symlinks
        // should limit too only regular files and directories;

        // LOG_WARN("Cannot trash symlink: {}", path);
        return false;
    }

    trash_dir->create_trash_dir();

    const std::string target_name = trash_dir->unique_name(path);
    trash_dir->create_trash_info(path, target_name);
    trash_dir->move(path, target_name);

    // LOG_INFO("moved to trash: {}", path);

    return true;
}

bool
Trash::restore(std::string_view path) noexcept
{
    (void)path;
    // NOOP
    return true;
}

void
Trash::empty() noexcept
{
    // NOOP
}

TrashDir::TrashDir(std::string_view path, dev_t device) noexcept
{
    this->trash_dir_path = path;
    this->trash_dir_files_path = Glib::build_filename(this->trash_dir_path, "files");
    this->trash_dir_info_path = Glib::build_filename(this->trash_dir_path, "info");
    this->trash_dir_device = device;

    create_trash_dir();
}

const std::string
TrashDir::unique_name(std::string_view path) const noexcept
{
    const std::string filename = std::filesystem::path(path).filename();
    const std::string basename = std::filesystem::path(path).stem();
    const std::string ext = std::filesystem::path(path).extension();

    std::string to_trash_filename = filename;
    const std::string to_trash_path = Glib::build_filename(files_path(), to_trash_filename);

    if (std::filesystem::exists(to_trash_path))
    {
        for (i32 i = 1; true; ++i)
        {
            const std::string ii = std::to_string(i);
            const std::string check_to_trash_filename = basename + "_" + ii + ext;
            const std::string check_to_trash_path =
                Glib::build_filename(files_path(), check_to_trash_filename);
            if (!std::filesystem::exists(check_to_trash_path))
            {
                to_trash_filename = check_to_trash_filename;
                break;
            }
        }
    }
    return to_trash_filename;
}

void
TrashDir::check_dir_exists(std::string_view path) noexcept
{
    if (std::filesystem::is_directory(path))
        return;

    // LOG_INFO("trash mkdir {}", path);
    std::filesystem::create_directories(path);
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

void
TrashDir::create_trash_dir() const noexcept
{
    // LOG_DEBUG("create trash dirs {}", trash_path());

    check_dir_exists(trash_path());
    check_dir_exists(files_path());
    check_dir_exists(info_path());
}

void
TrashDir::create_trash_info(std::string_view path, std::string_view target_name) const noexcept
{
    const std::string trash_info =
        Glib::build_filename(info_path(), fmt::format("{}.trashinfo", target_name));

    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char iso_time[100];
    std::strftime(iso_time, sizeof(iso_time), "%FT%TZ", std::localtime(&t));

    std::ofstream TrashInfoFile(trash_info);
    TrashInfoFile << "[Trash Info]\n";
    TrashInfoFile << "Path=" << path << "\n";
    TrashInfoFile << "DeletionDate=" << iso_time << "\n";
    TrashInfoFile.close();
}

void
TrashDir::move(std::string_view path, std::string_view target_name) const noexcept
{
    // LOG_INFO("fp {}", files_path());
    // LOG_INFO("ip {}", info_path());
    const std::string target_path = Glib::build_filename(files_path(), target_name.data());
    std::filesystem::rename(path, target_path);
}

const std::string&
TrashDir::trash_path() const noexcept
{
    return this->trash_dir_path;
}

dev_t
TrashDir::device() const noexcept
{
    return this->trash_dir_device;
}

const std::string&
TrashDir::files_path() const noexcept
{
    return this->trash_dir_files_path;
}

const std::string&
TrashDir::info_path() const noexcept
{
    return this->trash_dir_info_path;
}
