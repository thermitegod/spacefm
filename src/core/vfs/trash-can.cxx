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
#include <mutex>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <unordered_map>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/trash-can.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/utils.hxx"

#include "logger.hxx"

namespace
{
std::shared_mutex trash_cache_mutex;
std::unordered_map<u64, std::shared_ptr<vfs::trash_can>> trash_cache;
} // namespace

vfs::trash_can::trash_can(const std::filesystem::path& path) noexcept
    : trash_path_(path), files_path_(path / "files"), info_path_(path / "info")
{
    // create_trash_dir();
}

u64
vfs::trash_can::mount_id(const std::filesystem::path& path) noexcept
{
    const auto stat = ztd::statx::create(path, ztd::statx::symlink::no_follow);
    return stat->mount_id();
}

std::shared_ptr<vfs::trash_can>
vfs::trash_can::create(const std::filesystem::path& path) noexcept
{
    const auto id = mount_id(path);

    {
        std::shared_lock lock(trash_cache_mutex);
        const auto it = trash_cache.find(id);
        if (it != trash_cache.cend())
        {
            return it->second;
        }
    }

    std::filesystem::path trash_path;
    if (id == mount_id(vfs::user::home()))
    {
        trash_path = vfs::user::data() / "Trash";
    }
    else
    {
        trash_path = toplevel(path) / std::format(".Trash-{}", getuid());
    }

    struct hack : public vfs::trash_can
    {
        hack(const std::filesystem::path& path) : trash_can(path) {}
    };
    const auto trash = std::make_shared<hack>(trash_path);

    std::unique_lock lock(trash_cache_mutex);
    const auto [it, _] = trash_cache.try_emplace(id, trash);
    return it->second;
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

bool
vfs::trash_can::trash(const std::filesystem::path& path) noexcept
{
    const auto trash = vfs::trash_can::create(path);
    if (!trash)
    {
        return false;
    }

    if (trash->is_trash_dir(path))
    {
        logger::warn<logger::vfs>("Refusing to trash a Trash Dir: {}", path);
        return false;
    }

    trash->create_trash_dir();

    const auto target_name = trash->unique_filename(path);
    trash->create_trash_info(path, target_name);
    trash->move(path, target_name);
    // logger::debug<logger::vfs>("vfs::trash_can({}) trash   {}", trash->trash_path_, path);

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

std::filesystem::path
vfs::trash_can::unique_filename(const std::filesystem::path& path) const noexcept
{
    return vfs::utils::unique_path(files_path_, path.filename(), "_").filename();
}

void
vfs::trash_can::create_trash_dir() const noexcept
{
    const auto create_dir = [](const std::filesystem::path& path)
    {
        if (!std::filesystem::is_directory(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    };

    create_dir(trash_path_);
    create_dir(files_path_);
    create_dir(info_path_);
}

void
vfs::trash_can::create_trash_info(const std::filesystem::path& path,
                                  const std::filesystem::path& target_filename) const noexcept
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

    const auto ec = trashinfo_write(info_path_ / target_filename += ".trashinfo",
                                    {.path = path_value, .time = std::chrono::system_clock::now()});

    logger::error_if<logger::vfs>(ec, "Failed to write trash info file: {}", ec.message());
}

void
vfs::trash_can::move(const std::filesystem::path& path,
                     const std::filesystem::path& target_filename) const noexcept
{
    std::error_code ec;
    std::filesystem::rename(path, files_path_ / target_filename, ec);

    logger::error_if<logger::vfs>(ec, "Failed to trash file: {}", ec.message());
}

bool
vfs::trash_can::is_trash_dir(const std::filesystem::path& path) const noexcept
{
    return path == trash_path_ || path == files_path_ || path == info_path_;
}

std::error_code
vfs::trashinfo_write(const std::filesystem::path& path, const vfs::trashinfo& info) noexcept
{
    try
    {
        const auto kf = Glib::KeyFile::create();
        // clang-format off
        kf->set_string("Trash Info", "Path", std::format("{}", info.path));
        kf->set_string("Trash Info", "DeletionDate", std::format("{:%Y-%m-%dT%H:%M:%S}", info.time));
        // clang-format on
        kf->save_to_file(path);
    }
    catch (const Glib::FileError& e)
    {
        return std::make_error_code(std::errc{e.code()});
    }
    return {};
}

std::optional<vfs::trashinfo>
vfs::trashinfo_read(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path) || path.extension() != ".trashinfo")
    {
        return std::nullopt;
    }

    const auto kf = Glib::KeyFile::create();
    const auto loaded = kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    if (!loaded)
    {
        return std::nullopt;
    }

    trashinfo info{};

    try
    {
        info.path = kf->get_string("Trash Info", "Path");
        const auto date = kf->get_string("Trash Info", "DeletionDate");
        std::istringstream stream(date.raw());
        std::chrono::from_stream(stream, "%Y-%m-%dT%H:%M:%S", info.time);
    }
    catch (const Glib::KeyFileError& e)
    {
        (void)e;
        return std::nullopt;
    }

    return info;
}
