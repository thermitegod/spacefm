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

#include <optional>

#include <sstream>

#include <chrono>

#include <unistd.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "write.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-file-trash.hxx"

// std::shared_ptr<VFSTrash>
// VFSTrash::instance() noexcept
// {
//     static std::shared_ptr<VFSTrash> instance = std::make_shared<VFSTrash>();
//     return instance;
// }

VFSTrash* VFSTrash::single_instance = nullptr;

VFSTrash*
VFSTrash::instance() noexcept
{
    if (!VFSTrash::single_instance)
        VFSTrash::single_instance = new VFSTrash();
    return VFSTrash::single_instance;
}

VFSTrash::VFSTrash() noexcept
{
    const dev_t home_device = device(vfs::user_dirs->home_dir()).value();
    const std::string user_trash = Glib::build_filename(vfs::user_dirs->data_dir(), "Trash");
    std::shared_ptr<VFSTrashDir> home_trash = std::make_shared<VFSTrashDir>(user_trash);
    this->trash_dirs[home_device] = home_trash;
}

std::optional<dev_t>
VFSTrash::device(std::string_view path) noexcept
{
    const auto statbuf = ztd::lstat(path);
    if (statbuf.is_valid())
    {
        return statbuf.dev();
    }
    else
    {
        return std::nullopt; // stat failed
    }
}

const std::string
VFSTrash::toplevel(std::string_view path) noexcept
{
    const dev_t dev = device(path).value();

    std::string mount_path = path.data();
    std::string last_path;

    // ztd::logger::info("dev mount {}", device(mount_path));
    // ztd::logger::info("dev       {}", dev);

    // walk up the path until it gets to the root of the device
    while (device(mount_path).value() == dev)
    {
        last_path = mount_path;
        const std::filesystem::path mount_parent = std::filesystem::path(mount_path).parent_path();
        mount_path = mount_parent.string();
    }

    // ztd::logger::info("last path   {}", last_path);
    // ztd::logger::info("mount point {}", mount_path);

    return last_path;
}

std::shared_ptr<VFSTrashDir>
VFSTrash::trash_dir(std::string_view path) noexcept
{
    const dev_t dev = device(path).value();

    if (this->trash_dirs.count(dev))
    {
        return this->trash_dirs[dev];
    }

    // on another device cannot use $HOME trashcan
    const std::string top_dir = toplevel(path);
    const std::string trash_path =
        Glib::build_filename(top_dir, fmt::format("/.Trash-{}", getuid()));

    std::shared_ptr<VFSTrashDir> trash_dir = std::make_shared<VFSTrashDir>(trash_path);
    this->trash_dirs[dev] = trash_dir;

    return trash_dir;
}

bool
VFSTrash::trash(std::string_view path) noexcept
{
    const auto trash_dir = VFSTrash::instance()->trash_dir(path);
    if (!trash_dir)
    {
        return false;
    }

    if (ztd::endswith(path, "/Trash") || ztd::endswith(path, fmt::format("/.Trash-{}", getuid())))
    {
        ztd::logger::warn("Refusing to trash Trash Dir: {}", path);
        return true;
    }

    trash_dir->create_trash_dir();

    const std::string target_name = trash_dir->unique_name(path);
    trash_dir->create_trash_info(path, target_name);
    trash_dir->move(path, target_name);

    // ztd::logger::info("moved to trash: {}", path);

    return true;
}

bool
VFSTrash::restore(std::string_view path) noexcept
{
    (void)path;
    // NOOP
    return true;
}

void
VFSTrash::empty() noexcept
{
    // NOOP
}

VFSTrashDir::VFSTrashDir(std::string_view path) noexcept
{
    this->trash_path = path.data();
    this->files_path = Glib::build_filename(this->trash_path, "files");
    this->info_path = Glib::build_filename(this->trash_path, "info");

    create_trash_dir();
}

const std::string
VFSTrashDir::unique_name(std::string_view path) const noexcept
{
    const std::string filename = std::filesystem::path(path).filename();
    const std::string basename = std::filesystem::path(path).stem();
    const std::string ext = std::filesystem::path(path).extension();

    std::string to_trash_filename = filename;
    const std::string to_trash_path = Glib::build_filename(this->files_path, to_trash_filename);

    if (!std::filesystem::exists(to_trash_path))
    {
        return to_trash_filename;
    }

    for (usize i = 1; true; ++i)
    {
        const std::string check_to_trash_filename = fmt::format("{}_{}{}", basename, i, ext);
        const std::string check_to_trash_path =
            Glib::build_filename(this->files_path, check_to_trash_filename);
        if (!std::filesystem::exists(check_to_trash_path))
        {
            to_trash_filename = check_to_trash_filename;
            break;
        }
    }

    return to_trash_filename;
}

void
VFSTrashDir::check_dir_exists(std::string_view path) noexcept
{
    if (std::filesystem::is_directory(path))
    {
        return;
    }

    // ztd::logger::info("trash mkdir {}", path);
    std::filesystem::create_directories(path);
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

void
VFSTrashDir::create_trash_dir() const noexcept
{
    // ztd::logger::debug("create trash dirs {}", trash_path());

    this->check_dir_exists(this->trash_path);
    this->check_dir_exists(this->files_path);
    this->check_dir_exists(this->info_path);
}

void
VFSTrashDir::create_trash_info(std::string_view path, std::string_view target_name) const noexcept
{
    const std::string trash_info =
        Glib::build_filename(this->info_path, fmt::format("{}.trashinfo", target_name));

    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm* local_time = std::localtime(&t);
    std::ostringstream iso_time;
    iso_time << std::put_time(local_time, "%FT%TZ");

    const std::string trash_info_content =
        fmt::format("[Trash Info]\nPath={}\nDeletionDate={}", path, iso_time.str());

    write_file(trash_info, trash_info_content);
}

void
VFSTrashDir::move(std::string_view path, std::string_view target_name) const noexcept
{
    const std::string target_path = Glib::build_filename(this->files_path, target_name.data());

    // ztd::logger::info("fp {}", this->files_path);
    // ztd::logger::info("ip {}", this->info_path);
    // ztd::logger::info("tp {}", target_path);

    std::filesystem::rename(path, target_path);
}
