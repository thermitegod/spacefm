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

#include <format>

#include <filesystem>

#include <memory>

#include <optional>

#include <sstream>

#include <chrono>

#include <unistd.h>

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
    const auto user_trash = vfs::user_dirs->data_dir() / "Trash";
    std::shared_ptr<VFSTrashDir> home_trash = std::make_shared<VFSTrashDir>(user_trash);
    this->trash_dirs_[home_device] = home_trash;
}

std::optional<dev_t>
VFSTrash::device(const std::filesystem::path& path) noexcept
{
    const auto statbuf = ztd::statx(path, ztd::statx::symlink::no_follow);
    if (statbuf)
    {
        return statbuf.dev();
    }
    else
    {
        return std::nullopt; // stat failed
    }
}

const std::filesystem::path
VFSTrash::toplevel(const std::filesystem::path& path) noexcept
{
    const dev_t dev = device(path).value();

    std::filesystem::path mount_path = path;
    std::filesystem::path last_path;

    // ztd::logger::info("dev mount {}", device(mount_path));
    // ztd::logger::info("dev       {}", dev);

    // walk up the path until it gets to the root of the device
    while (device(mount_path).value() == dev)
    {
        last_path = mount_path;
        const std::filesystem::path mount_parent = std::filesystem::path(mount_path).parent_path();
        mount_path = mount_parent;
    }

    // ztd::logger::info("last path   {}", last_path);
    // ztd::logger::info("mount point {}", mount_path);

    return last_path;
}

std::shared_ptr<VFSTrashDir>
VFSTrash::trash_dir(const std::filesystem::path& path) noexcept
{
    const dev_t dev = device(path).value();

    if (this->trash_dirs_.contains(dev))
    {
        return this->trash_dirs_[dev];
    }

    // on another device, cannot use $HOME trashcan
    const std::filesystem::path top_dir = toplevel(path);
    // BUGGED - only the std::format part of the path is used in creating 'trash_path',
    // do not think this is my bug.
    // const auto trash_path = top_dir / std::format("/.Trash-{}", getuid());
    const std::filesystem::path trash_path =
        std::format("{}/.Trash-{}", top_dir.string(), getuid());

    std::shared_ptr<VFSTrashDir> trash_dir = std::make_shared<VFSTrashDir>(trash_path);
    this->trash_dirs_[dev] = trash_dir;

    return trash_dir;
}

bool
VFSTrash::trash(const std::filesystem::path& path) noexcept
{
    const auto trash_dir = VFSTrash::instance()->trash_dir(path);
    if (!trash_dir)
    {
        return false;
    }

    if (path.string().ends_with("/Trash") ||
        path.string().ends_with(std::format("/.Trash-{}", getuid())))
    {
        ztd::logger::warn("Refusing to trash Trash Dir: {}", path.string());
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
VFSTrash::restore(const std::filesystem::path& path) noexcept
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

VFSTrashDir::VFSTrashDir(const std::filesystem::path& path) noexcept
{
    this->trash_path_ = path;
    this->files_path_ = this->trash_path_ / "files";
    this->info_path_ = this->trash_path_ / "info";

    create_trash_dir();
}

const std::string
VFSTrashDir::unique_name(const std::filesystem::path& path) const noexcept
{
    const std::string filename = path.filename();
    const std::string basename = path.stem();
    const std::string ext = path.extension();

    std::string to_trash_filename = filename;
    const std::string to_trash_path = this->files_path_ / to_trash_filename;

    if (!std::filesystem::exists(to_trash_path))
    {
        return to_trash_filename;
    }

    for (usize i = 1; true; ++i)
    {
        const std::string check_to_trash_filename = std::format("{}_{}{}", basename, i, ext);
        const auto check_to_trash_path = this->files_path_ / check_to_trash_filename;
        if (!std::filesystem::exists(check_to_trash_path))
        {
            to_trash_filename = check_to_trash_filename;
            break;
        }
    }

    return to_trash_filename;
}

void
VFSTrashDir::check_dir_exists(const std::filesystem::path& path) noexcept
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

    this->check_dir_exists(this->trash_path_);
    this->check_dir_exists(this->files_path_);
    this->check_dir_exists(this->info_path_);
}

const std::string
VFSTrashDir::create_trash_date(const std::time_t time) const noexcept
{
    const auto point = std::chrono::system_clock::from_time_t(time);

    const auto date = std::chrono::floor<std::chrono::days>(point);

    const auto midnight = point - std::chrono::floor<std::chrono::days>(point);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(midnight);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(midnight - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(midnight - hours - minutes);

    return std::format("{0:%Y-%m-%d}T{1:%H}:{2:%M}:{3:%S}", date, hours, minutes, seconds);
}

void
VFSTrashDir::create_trash_info(const std::filesystem::path& path,
                               const std::string_view target_name) const noexcept
{
    const auto trash_info = this->info_path_ / std::format("{}.trashinfo", target_name);

    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto iso_time = create_trash_date(time);

    const std::string trash_info_content =
        std::format("[Trash Info]\nPath={}\nDeletionDate={}\n", path.string(), iso_time);

    write_file(trash_info, trash_info_content);
}

void
VFSTrashDir::move(const std::filesystem::path& path,
                  const std::string_view target_name) const noexcept
{
    const auto target_path = this->files_path_ / target_name;

    // ztd::logger::info("fp {}", this->files_path);
    // ztd::logger::info("ip {}", this->info_path);
    // ztd::logger::info("tp {}", target_path);

    std::filesystem::rename(path, target_path);
}
