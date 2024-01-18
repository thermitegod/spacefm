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

#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "write.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-trash-can.hxx"

static std::shared_ptr<vfs::trash_can> global_trash_can = nullptr;

void
vfs_trash_init()
{
    global_trash_can = vfs::trash_can::create();
}

////////////////

vfs::trash_can::trash_can() noexcept
{
    const auto home_id = ztd::statx(vfs::user_dirs->home_dir()).mount_id();
    const auto user_trash = vfs::user_dirs->data_dir() / "Trash";
    auto home_trash = std::make_shared<vfs::trash_can::trash_dir>(user_trash);
    this->trash_dirs_[home_id] = home_trash;
}

u64
vfs::trash_can::mount_id(const std::filesystem::path& path) noexcept
{
    return ztd::statx(path, ztd::statx::symlink::no_follow).mount_id();
}

const std::shared_ptr<vfs::trash_can>
vfs::trash_can::create() noexcept
{
    return std::make_shared<vfs::trash_can>();
}

const std::filesystem::path
vfs::trash_can::toplevel(const std::filesystem::path& path) noexcept
{
    const auto id = mount_id(path);

    std::filesystem::path mount_path = path;
    std::filesystem::path last_path;

    // ztd::logger::info("id mount {}", device(mount_path));
    // ztd::logger::info("id       {}", id);

    // walk up the path until it gets to the root of the device
    while (mount_id(mount_path) == id)
    {
        last_path = mount_path;
        mount_path = mount_path.parent_path();
    }

    // ztd::logger::info("last path   {}", last_path);
    // ztd::logger::info("mount point {}", mount_path);

    return last_path;
}

const std::shared_ptr<vfs::trash_can::trash_dir>
vfs::trash_can::get_trash_dir(const std::filesystem::path& path) noexcept
{
    const auto id = mount_id(path);

    if (this->trash_dirs_.contains(id))
    {
        return this->trash_dirs_[id];
    }

    // path on another device, cannot use $HOME trashcan
    const std::filesystem::path top_dir = toplevel(path);
    // BUGGED - only the std::format part of the path is used in creating 'trash_path',
    // do not think this is my bug.
    // const auto trash_path = top_dir / std::format("/.Trash-{}", getuid());
    const std::filesystem::path trash_path =
        std::format("{}/.Trash-{}", top_dir.string(), getuid());

    auto trash_dir = std::make_shared<vfs::trash_can::trash_dir>(trash_path);
    this->trash_dirs_[id] = trash_dir;

    return trash_dir;
}

bool
vfs::trash_can::trash(const std::filesystem::path& path) noexcept
{
    const auto trash_dir = global_trash_can->get_trash_dir(path);
    if (!trash_dir)
    {
        return false;
    }

    if (path.string().contains("Trash"))
    {
        if (path.string().ends_with("/Trash") ||
            path.string().ends_with(std::format("/.Trash-{}", getuid())))
        {
            ztd::logger::warn("Refusing to trash the Trash Dir: {}", path.string());
            return true;
        }
        else if (path.string().ends_with("/Trash/files") ||
                 path.string().ends_with(std::format("/.Trash-{}/files", getuid())))
        {
            ztd::logger::warn("Refusing to trash the Trash Files Dir: {}", path.string());
            return true;
        }
        else if (path.string().ends_with("/Trash/info") ||
                 path.string().ends_with(std::format("/.Trash-{}/info", getuid())))
        {
            ztd::logger::warn("Refusing to trash the Trash Info Dir: {}", path.string());
            return true;
        }
    }

    trash_dir->create_trash_dir();

    const std::string target_name = trash_dir->unique_name(path);
    trash_dir->create_trash_info(path, target_name);
    trash_dir->move(path, target_name);

    // ztd::logger::info("moved to trash: {}", path);

    return true;
}

bool
vfs::trash_can::restore(const std::filesystem::path& path) noexcept
{
    (void)path;
    // NOOP
    return false;
}

void
vfs::trash_can::empty() noexcept
{
    // NOOP
}

void
vfs::trash_can::empty(const std::filesystem::path& path) noexcept
{
    (void)path;
    // NOOP
}

/////////////////////////////

vfs::trash_can::trash_dir::trash_dir(const std::filesystem::path& path) noexcept : trash_path_(path)
{
    this->files_path_ = this->trash_path_ / "files";
    this->info_path_ = this->trash_path_ / "info";

    create_trash_dir();
}

const std::string
vfs::trash_can::trash_dir::unique_name(const std::filesystem::path& path) const noexcept
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
vfs::trash_can::trash_dir::check_dir_exists(const std::filesystem::path& path) noexcept
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
vfs::trash_can::trash_dir::create_trash_dir() const noexcept
{
    // ztd::logger::debug("create trash dirs {}", trash_path());

    this->check_dir_exists(this->trash_path_);
    this->check_dir_exists(this->files_path_);
    this->check_dir_exists(this->info_path_);
}

const std::string
vfs::trash_can::trash_dir::create_trash_date(
    const std::chrono::system_clock::time_point time_point) noexcept
{
    const auto date = std::chrono::floor<std::chrono::days>(time_point);

    const auto midnight = time_point - std::chrono::floor<std::chrono::days>(time_point);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(midnight);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(midnight - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(midnight - hours - minutes);

    return std::format("{0:%Y-%m-%d}T{1:%H}:{2:%M}:{3:%S}Z", date, hours, minutes, seconds);
}

void
vfs::trash_can::trash_dir::create_trash_info(const std::filesystem::path& path,
                                             const std::string_view target_name) const noexcept
{
    const auto trash_info = this->info_path_ / std::format("{}.trashinfo", target_name);

    const auto iso_time = create_trash_date(std::chrono::system_clock::now());

    const std::string trash_info_content =
        std::format("[Trash Info]\nPath={}\nDeletionDate={}\n", path.string(), iso_time);

    write_file(trash_info, trash_info_content);
}

void
vfs::trash_can::trash_dir::move(const std::filesystem::path& path,
                                const std::string_view target_name) const noexcept
{
    const auto target_path = this->files_path_ / target_name;

    // ztd::logger::info("fp {}", this->files_path);
    // ztd::logger::info("ip {}", this->info_path);
    // ztd::logger::info("tp {}", target_path);

    std::filesystem::rename(path, target_path);
}
