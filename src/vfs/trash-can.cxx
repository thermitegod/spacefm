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

#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

#include "vfs/trash-can.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/file-ops.hxx"
#include "vfs/utils/utils.hxx"

#include "logger.hxx"

// TODO
// - create_trash_info() reuse mount_id and toplevel, merge into vfs::trash_can::trash?

namespace global
{
const std::shared_ptr<vfs::trash_can> trash_can = vfs::trash_can::create();
}

vfs::trash_can::trash_can() noexcept
{
    const auto stat = ztd::statx::create(vfs::user::home(), ztd::statx::symlink::follow);

    const auto home_id = stat->mount_id();
    const auto user_trash = vfs::user::data() / "Trash";

    this->trash_dirs_[home_id] = std::make_shared<vfs::trash_can::trash_dir>(user_trash);
}

u64
vfs::trash_can::mount_id(const std::filesystem::path& path) noexcept
{
    const auto stat = ztd::statx::create(path, ztd::statx::symlink::no_follow);
    return stat->mount_id();
}

std::shared_ptr<vfs::trash_can>
vfs::trash_can::create() noexcept
{
    return std::make_shared<vfs::trash_can>();
}

std::filesystem::path
vfs::trash_can::toplevel(const std::filesystem::path& path) noexcept
{
    const auto id = mount_id(path);

    std::filesystem::path mount_path = path;
    std::filesystem::path last_path;

    // walk up the path until it gets to the root of the device
    while (mount_id(mount_path) == id)
    {
        last_path = mount_path;
        mount_path = mount_path.parent_path();
    }
    return last_path;
}

std::shared_ptr<vfs::trash_can::trash_dir>
vfs::trash_can::get_trash_dir(const std::filesystem::path& path) noexcept
{
    const auto id = mount_id(path);

    if (this->trash_dirs_.contains(id))
    {
        return this->trash_dirs_[id];
    }

    // path on another device, cannot use $HOME trashcan
    const auto trash_path = toplevel(path) / std::format(".Trash-{}", getuid());

    auto trash_dir = std::make_shared<vfs::trash_can::trash_dir>(trash_path);
    this->trash_dirs_[id] = trash_dir;

    return trash_dir;
}

bool
vfs::trash_can::trash(const std::filesystem::path& path) noexcept
{
    const auto trash_dir = global::trash_can->get_trash_dir(path);
    if (!trash_dir)
    {
        return false;
    }

    if (path.string().contains("Trash"))
    {
        if (path.string().ends_with("/Trash") ||
            path.string().ends_with(std::format("/.Trash-{}", getuid())))
        {
            logger::warn<logger::vfs>("Refusing to trash the Trash Dir: {}", path.string());
            return true;
        }
        else if (path.string().ends_with("/Trash/files") ||
                 path.string().ends_with(std::format("/.Trash-{}/files", getuid())))
        {
            logger::warn<logger::vfs>("Refusing to trash the Trash Files Dir: {}", path.string());
            return true;
        }
        else if (path.string().ends_with("/Trash/info") ||
                 path.string().ends_with(std::format("/.Trash-{}/info", getuid())))
        {
            logger::warn<logger::vfs>("Refusing to trash the Trash Info Dir: {}", path.string());
            return true;
        }
    }

    trash_dir->create_trash_dir();

    const auto target_name = trash_dir->unique_filename(path);
    trash_dir->create_trash_info(path, target_name);
    trash_dir->move(path, target_name);

    // logger::info<logger::vfs>("moved to trash: {}", path);

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
vfs::trash_can::empty(const std::filesystem::path& path) noexcept
{
    (void)path;
    // NOOP
}

vfs::trash_can::trash_dir::trash_dir(const std::filesystem::path& path) noexcept
    : trash_path_(path), files_path_(path / "files"), info_path_(path / "info")
{
    create_trash_dir();
}

std::filesystem::path
vfs::trash_can::trash_dir::unique_filename(const std::filesystem::path& path) const noexcept
{
    return vfs::utils::unique_path(this->files_path_, path.filename(), "_").filename();
}

void
vfs::trash_can::trash_dir::create_trash_dir() const noexcept
{
    const auto create_dir = [](const std::filesystem::path& path)
    {
        if (!std::filesystem::is_directory(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    };

    create_dir(this->trash_path_);
    create_dir(this->files_path_);
    create_dir(this->info_path_);
}

void
vfs::trash_can::trash_dir::create_trash_info(
    const std::filesystem::path& path, const std::filesystem::path& target_filename) const noexcept
{
    const auto path_value = std::invoke(
        [](const auto& path) -> std::string
        {
            const auto home = vfs::user::data() / "Trash";

            const auto home_id = vfs::trash_can::mount_id(home);
            const auto path_id = vfs::trash_can::mount_id(path);

            if (path_id == home_id)
            {
                return path.string();
            }
            else
            {
                const auto toplevel = vfs::trash_can::toplevel(path);

                return ztd::remove_prefix(path.string(), toplevel.string() += "/");
            }
        },
        path);

    auto ec = vfs::utils::write_file(
        this->info_path_ / target_filename += ".trashinfo",
        std::format("[Trash Info]\nPath={}\nDeletionDate={:%Y-%m-%dT%H:%M:%S}\n",
                    path_value,
                    std::chrono::system_clock::now()));

    logger::error_if<logger::vfs>(ec, "Failed to write trash info file: {}", ec.message());
}

void
vfs::trash_can::trash_dir::move(const std::filesystem::path& path,
                                const std::filesystem::path& target_filename) const noexcept
{
    std::error_code ec;
    std::filesystem::rename(path, this->files_path_ / target_filename, ec);

    logger::error_if<logger::vfs>(ec, "Failed to trash file: {}", ec.message());
}
