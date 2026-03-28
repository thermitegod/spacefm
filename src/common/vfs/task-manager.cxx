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

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include <cstdint>

#include <pthread.h>

#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/task-manager.hxx"
#include "vfs/trash-can.hxx"

#include "logger.hxx"

vfs::task_manager::task_manager() noexcept
{
    thread_ = std::jthread([this](const std::stop_token& stoken) { run(stoken); });
    pthread_setname_np(thread_.native_handle(), "task-manager");
}

vfs::task_manager::~task_manager() noexcept
{
    thread_.request_stop();
    thread_.join();
}

std::shared_ptr<vfs::task_manager>
vfs::task_manager::create() noexcept
{
    struct hack : public vfs::task_manager
    {
        hack() : task_manager() {}
    };

    return std::make_shared<hack>();
}

bool
vfs::task_manager::empty() noexcept
{
    std::scoped_lock lock(mutex_);
    ztd::panic_if(queue_.size() != tasks_.size(), "task manager size mismatch");
    return queue_.size() == 0 && tasks_.size() == 0;
}

void
vfs::task_manager::run(const std::stop_token& stoken) noexcept
{
    while (!stoken.stop_requested())
    {
        this->run_once(stoken);
    }
}

void
vfs::task_manager::run_once(const std::stop_token& stoken) noexcept
{
    std::shared_ptr<task_item> current;
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, stoken, [this] { return !queue_.empty(); });

        if (stoken.stop_requested())
        {
            return;
        }

        active_task_id_ = queue_.front();
        current = tasks_.at(active_task_id_);
    }

    if (current)
    {
        std::stop_callback cb(stoken, [current] { current->stop_source.request_stop(); });

        if (!current->stop_source.stop_requested())
        {
            current->state = task_item::status::running;
            current->action(stoken, current);
        }

        std::scoped_lock lock(mutex_);
        if (tasks_.contains(active_task_id_))
        {
            tasks_.erase(active_task_id_);
        }
        if (std::ranges::contains(queue_, active_task_id_))
        {
            std::erase(queue_, active_task_id_);
        }
        active_task_id_ = 0;
    }
}

void
vfs::task_manager::queue_task(
    const std::function<void(const std::stop_token&, const std::shared_ptr<task_item>&)>&
        slot) noexcept
{
    auto item = std::make_shared<task_item>(create_task_id());
    item->action = [this, item, slot](const std::stop_token& stoken,
                                      const std::shared_ptr<task_item>& self) noexcept
    {
        if (stoken.stop_requested())
        {
            return;
        }

        try
        {
            slot(stoken, self);

            if (!stoken.stop_requested())
            {
                signal_task_finished_.emit(self->id);
            }
        }
        catch (const std::exception& e)
        {
            signal_task_error_.emit({self->id, e.what()});
        }
    };

    {
        std::scoped_lock lock(mutex_);
        tasks_[item->id] = item;
        queue_.push_back(item->id);
    }
    cv_.notify_one();

    signal_task_added_.emit(item->id);
}

void
vfs::task_manager::add(const vfs::chmod_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        std::function<void(const std::filesystem::path&)> do_chmod =
            [&](const std::filesystem::path& path)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            if (task.recursive && std::filesystem::is_directory(path))
            {
                std::filesystem::permissions(path, task.mode);

                for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
                {
                    if (!item->check_pause(stoken) || stoken.stop_requested())
                    {
                        return;
                    }

                    std::filesystem::permissions(entry.path(), task.mode);
                }
            }
            else
            {
                std::filesystem::permissions(path, task.mode);
            }
        };

        for (const auto& path : task.paths)
        {
            do_chmod(path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::chown_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        const auto pw = ztd::passwd::create(task.user);
        const auto gr = ztd::group::create(task.group);

        if (!pw || !gr)
        {
            throw std::runtime_error("Invalid user or group name");
        }

        const uid_t uid = pw->uid();
        const gid_t gid = gr->gid();

        auto chown_wrapper = [uid, gid](const std::filesystem::path& path)
        {
            if (::lchown(path.c_str(), uid, gid) != 0)
            {
                throw std::filesystem::filesystem_error(
                    "Failed to change ownership",
                    path,
                    std::error_code(errno, std::generic_category()));
            }
        };

        std::function<void(const std::filesystem::path&)> do_chown =
            [&](const std::filesystem::path& path)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            if (task.recursive && std::filesystem::is_directory(path))
            {
                chown_wrapper(path);

                for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
                {
                    if (!item->check_pause(stoken) || stoken.stop_requested())
                    {
                        return;
                    }

                    do_chown(entry.path());
                }
            }
            else
            {
                chown_wrapper(path);
            }
        };

        for (const auto& path : task.paths)
        {
            do_chown(path);
        }
    };

    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::copy_task& task) noexcept
{
    // logger::trace("copy: {} -> {}", task.source.string(), task.destination.string());

    auto slot = [this, task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!std::filesystem::exists(task.destination))
        {
            throw std::filesystem::filesystem_error(
                "Destination path does not exist",
                task.destination,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        auto collision_action = collision_resolve::pending;

        std::function<void(const std::filesystem::path&, const std::filesystem::path&)> do_copy =
            [&](const std::filesystem::path& source, const std::filesystem::path& destination)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            if (!std::filesystem::exists(source))
            {
                throw std::filesystem::filesystem_error(
                    "Source path does not exist",
                    source,
                    std::make_error_code(std::errc::no_such_file_or_directory));
            }

            const auto result =
                handle_collision(stoken, item, source, destination, collision_action);

            if (result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::overwrite_all)
            {
                collision_action = result.action;
            }

            if (result.action == collision_resolve::skip ||
                result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::cancel)
            {
                return;
            }

            const auto actual_destination =
                (result.action == collision_resolve::rename) ? result.destination : destination;

            if (std::filesystem::is_directory(source))
            {
                std::filesystem::create_directories(actual_destination);
                for (const auto& entry : std::filesystem::directory_iterator(source))
                {
                    if (!item->check_pause(stoken) || stoken.stop_requested())
                    {
                        return;
                    }

                    do_copy(entry.path(), actual_destination / entry.path().filename());
                }
            }
            else
            {
                std::filesystem::copy_file(source, actual_destination);
            }
        };

        for (const auto& source : task.sources)
        {
            do_copy(source, task.destination / source.filename());
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::move_task& task) noexcept
{
    // logger::trace("move: {} -> {}", task.source.string(), task.destination.string());

    auto slot = [this, task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!std::filesystem::exists(task.destination))
        {
            throw std::filesystem::filesystem_error(
                "Destination path does not exist",
                task.destination,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        auto collision_action = collision_resolve::pending;

        std::function<void(const std::filesystem::path&, const std::filesystem::path&)> do_move =
            [&](const std::filesystem::path& source, const std::filesystem::path& destination)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            if (!std::filesystem::exists(source))
            {
                throw std::filesystem::filesystem_error(
                    "Source path does not exist",
                    source,
                    std::make_error_code(std::errc::no_such_file_or_directory));
            }

            const auto result =
                handle_collision(stoken, item, source, destination, collision_action);

            if (result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::overwrite_all)
            {
                collision_action = result.action;
            }

            if (result.action == collision_resolve::skip ||
                result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::cancel)
            {
                return;
            }

            const auto actual_destination =
                (result.action == collision_resolve::rename) ? result.destination : destination;

            if ((source.filename() == destination.filename()) &&
                std::filesystem::is_directory(source) &&
                std::filesystem::is_directory(actual_destination))
            { // directory merge
                for (const auto& entry : std::filesystem::directory_iterator(source))
                {
                    if (!item->check_pause(stoken) || stoken.stop_requested())
                    {
                        return;
                    }

                    do_move(entry.path(), actual_destination / entry.path().filename());
                }

                if (std::filesystem::is_empty(source))
                {
                    std::filesystem::remove(source);
                }

                return;
            }

            const auto src_stat = ztd::lstat::create(source);
            const auto dest_parent_stat = ztd::stat::create(actual_destination.parent_path());

            if (src_stat && dest_parent_stat && src_stat->dev() == dest_parent_stat->dev())
            { // same device, can just use rename
                if (std::filesystem::exists(actual_destination))
                {
                    std::filesystem::remove_all(actual_destination);
                }
                std::filesystem::rename(source, actual_destination);
            }
            else
            {
                if (std::filesystem::is_directory(source))
                {
                    std::filesystem::create_directories(actual_destination);
                    for (const auto& entry : std::filesystem::directory_iterator(source))
                    {
                        if (!item->check_pause(stoken) || stoken.stop_requested())
                        {
                            return;
                        }

                        do_move(entry.path(), actual_destination / entry.path().filename());
                    }

                    if (std::filesystem::is_empty(source))
                    {
                        std::filesystem::remove_all(source);
                    }
                }
                else
                {
                    const auto ok = std::filesystem::copy_file(
                        source,
                        actual_destination,
                        std::filesystem::copy_options::overwrite_existing);
                    if (ok)
                    {
                        std::filesystem::remove(source);
                    }
                }
            }
        };

        for (const auto& source : task.sources)
        {
            do_move(source, task.destination / source.filename());
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::rename_task& task) noexcept
{
    // logger::trace("rename: {} -> {}", task.source.string(), task.destination.string());

    auto slot = [this, task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!std::filesystem::exists(task.source))
        {
            throw std::filesystem::filesystem_error(
                "Source path does not exist",
                task.source,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        if (std::filesystem::exists(task.destination) &&
            ztd::statx::create(task.source)->mode() != ztd::statx::create(task.destination)->mode())
        {
            throw std::filesystem::filesystem_error("Cannot change type with rename",
                                                    task.source,
                                                    task.destination,
                                                    std::make_error_code(std::errc::not_supported));
        }

        auto do_rename =
            [&](const std::filesystem::path& source, const std::filesystem::path& destination)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            const auto result =
                handle_collision(stoken, item, source, destination, collision_resolve::pending);

            if (result.action == collision_resolve::skip ||
                result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::cancel ||
                result.action == collision_resolve::overwrite_all)
            {
                return;
            }

            const auto actual_destination =
                (result.action == collision_resolve::rename) ? result.destination : destination;

            if (result.action == collision_resolve::merge)
            {
                throw std::filesystem::filesystem_error(
                    "Cannot merge directories with rename",
                    task.source,
                    actual_destination,
                    std::make_error_code(std::errc::not_supported));
            }

            const auto src_stat = ztd::lstat::create(source);
            const auto dest_parent_stat = ztd::stat::create(actual_destination.parent_path());

            if (src_stat && dest_parent_stat && src_stat->dev() == dest_parent_stat->dev())
            {
                if (std::filesystem::exists(actual_destination))
                {
                    std::filesystem::remove_all(actual_destination);
                }
                std::filesystem::rename(source, actual_destination);
            }
            else
            {
                throw std::filesystem::filesystem_error(
                    "Cannot rename a file to a different device",
                    task.source,
                    actual_destination,
                    std::make_error_code(std::errc::cross_device_link));
            }
        };

        do_rename(task.source, task.destination);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::trash_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        auto do_trash = [&](const std::filesystem::path& path)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            const auto result = vfs::trash_can::trash(path);
            if (!result)
            {
                throw std::filesystem::filesystem_error(
                    "Failed to trash",
                    path,
                    std::make_error_code(std::errc::no_message));
            }
        };

        for (const auto& path : task.paths)
        {
            do_trash(path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::trash_restore_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        auto do_restore = [&](const std::filesystem::path& path)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            const auto result = vfs::trash_can::restore(path);
            if (!result)
            {
                throw std::filesystem::filesystem_error(
                    "Failed to restore",
                    path,
                    std::make_error_code(std::errc::no_message));
            }
        };

        for (const auto& path : task.paths)
        {
            do_restore(path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::remove_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        std::function<void(const std::filesystem::path&)> do_remove =
            [&](const std::filesystem::path& path)
        {
            if (!item->check_pause(stoken) || stoken.stop_requested())
            {
                return;
            }

            if (!std::filesystem::exists(path))
            {
                throw std::filesystem::filesystem_error(
                    "Trying to remove nonexistent path ",
                    path,
                    std::make_error_code(std::errc::invalid_argument));
            }

            if (std::filesystem::is_directory(path))
            {
                for (const auto& entry : std::filesystem::directory_iterator(path))
                {
                    if (!item->check_pause(stoken) || stoken.stop_requested())
                    {
                        return;
                    }

                    do_remove(entry.path());
                }

                std::filesystem::remove(path);
            }
            else
            {
                std::filesystem::remove(path);
            }
        };

        for (const auto& path : task.paths)
        {
            do_remove(path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_directory_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!item->check_pause(stoken) || stoken.stop_requested())
        {
            return;
        }

        std::filesystem::create_directories(task.path);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_file_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!item->check_pause(stoken) || stoken.stop_requested())
        {
            return;
        }

        if (std::filesystem::exists(task.path))
        {
            throw std::filesystem::filesystem_error("Path already exists",
                                                    task.path,
                                                    std::make_error_code(std::errc::file_exists));
        }

        if (!std::filesystem::exists(task.path.parent_path()))
        {
            std::filesystem::create_directories(task.path.parent_path());
        }

        std::ofstream(task.path);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_symlink_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, const std::shared_ptr<task_item>& item)
    {
        if (!item->check_pause(stoken) || stoken.stop_requested())
        {
            return;
        }

        if (std::filesystem::exists(task.name))
        {
            if (task.force)
            {
                std::filesystem::remove(task.name);
            }
            else
            {
                throw std::filesystem::filesystem_error(
                    "Path already exists",
                    task.name,
                    std::make_error_code(std::errc::file_exists));
            }
        }

        std::filesystem::create_symlink(task.target, task.name);
    };
    queue_task(slot);
}

void
vfs::task_manager::resume(const std::uint64_t task_id) noexcept
{
    (void)task_id;

    // TODO
}

void
vfs::task_manager::pause(const std::uint64_t task_id) noexcept
{
    (void)task_id;

    // TODO
}

void
vfs::task_manager::stop(const std::uint64_t task_id) noexcept
{
    (void)task_id;

    // TODO
}

void
vfs::task_manager::remove(const std::uint64_t task_id) noexcept
{
    (void)task_id;

    // TODO
}

void
vfs::task_manager::resume_all() noexcept
{
    // TODO
}

void
vfs::task_manager::pause_all() noexcept
{
    // TODO
}

void
vfs::task_manager::stop_all() noexcept
{
    // TODO
}

void
vfs::task_manager::remove_all() noexcept
{
    // TODO
}

vfs::task_manager::collision_result
vfs::task_manager::handle_collision(const std::stop_token& stoken,
                                    const std::shared_ptr<task_item>& item,
                                    const std::filesystem::path& source,
                                    const std::filesystem::path& destination,
                                    const vfs::collision_resolve default_action) noexcept
{
    if (!std::filesystem::exists(destination))
    {
        return {collision_resolve::none, destination};
    }

    // directory merge
    if ((source.filename() == destination.filename()) && std::filesystem::is_directory(source) &&
        std::filesystem::is_directory(destination))
    {
        return {collision_resolve::merge, destination};
    }

    // reusing previous action choice for current task
    if (default_action == collision_resolve::overwrite_all)
    {
        return {collision_resolve::overwrite_all, destination};
    }
    if (default_action == collision_resolve::skip_all)
    {
        return {collision_resolve::skip_all, destination};
    }

    item->state = task_item::status::paused;
    {
        std::scoped_lock c_lock(item->collision_mutex);
        item->resolve_action = collision_resolve::pending;
    }

    signal_task_collision().emit({
        .task_id = item->id,
        .source = source,
        .destination = destination,
        .resolved =
            [this](const std::uint64_t task_id,
                   const collision_resolve action,
                   const std::filesystem::path& new_name) noexcept
        {
            std::scoped_lock lock(mutex_);

            if (!tasks_.contains(task_id))
            {
                return;
            }

            auto& item = tasks_.at(task_id);

            std::scoped_lock c_lock(item->collision_mutex);
            item->resolve_action = action;
            item->new_name = new_name;
            item->collision_cv.notify_all();
        },
    });

    // block for gui resolve
    item->wait_for_resolve(stoken);

    if (stoken.stop_requested() || item->stop_source.stop_requested())
    {
        return {collision_resolve::cancel, destination};
    }
    item->state = task_item::status::running;

    return {item->resolve_action,
            (item->resolve_action == collision_resolve::rename) ? item->new_name : destination};
}
