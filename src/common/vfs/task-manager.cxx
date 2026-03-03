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
#include <set>
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
    return std::make_shared<vfs::task_manager>();
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
            current->action(stoken, *current);
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
vfs::task_manager::queue_task(std::function<void(const std::stop_token&, task_item&)> slot) noexcept
{
    auto item = std::make_shared<task_item>(create_task_id());
    item->action = [this, item, slot](const std::stop_token& stoken, task_item& self) noexcept
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
                signal_task_finished_.emit(self.id);
            }
        }
        catch (const std::exception& e)
        {
            signal_task_error_.emit({self.id, e.what()});
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
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        if (t.options.contains(vfs::chmod_task::options::recursive) &&
            std::filesystem::is_directory(t.path))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(t.path))
            {
                if (!item.check_pause(stoken) || stoken.stop_requested())
                {
                    return;
                }

                std::filesystem::permissions(entry.path(), t.mode);
            }
        }
        else
        {
            std::filesystem::permissions(t.path, t.mode);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::chown_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        const auto pw = ztd::passwd::create(t.user);
        const auto gr = ztd::group::create(t.group);

        if (!pw || !gr)
        {
            throw std::runtime_error("Invalid user or group name");
        }

        const uid_t uid = pw->uid();
        const gid_t gid = gr->gid();

        auto do_chown = [uid, gid](const std::filesystem::path& p)
        {
            if (::lchown(p.c_str(), uid, gid) != 0)
            {
                throw std::filesystem::filesystem_error(
                    "Failed to change ownership",
                    p,
                    std::error_code(errno, std::generic_category()));
            }
        };

        if (t.options.contains(vfs::chown_task::options::recursive) &&
            std::filesystem::is_directory(t.path))
        {
            do_chown(t.path);

            for (const auto& entry : std::filesystem::recursive_directory_iterator(t.path))
            {
                if (!item.check_pause(stoken) || stoken.stop_requested())
                {
                    return;
                }

                do_chown(entry.path());
            }
        }
        else
        {
            do_chown(t.path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::copy_task& task) noexcept
{
    // logger::trace("copy: {} -> {}", task.source.string(), task.destination.string());

    auto slot = [this, task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        auto collision_action = collision_resolve::pending;

        auto do_copy = [](const std::filesystem::path& source,
                          const std::filesystem::path& destination,
                          const std::filesystem::copy_options options)
        {
            std::filesystem::copy_file(source,
                                       destination,
                                       options | std::filesystem::copy_options::overwrite_existing);
        };

        if (std::filesystem::is_directory(t.source))
        {
            std::filesystem::create_directories(t.destination);

            for (const auto& entry : std::filesystem::recursive_directory_iterator(t.source))
            {
                if (!item.check_pause(stoken) || stoken.stop_requested())
                {
                    return;
                }

                const auto relative = std::filesystem::relative(entry.path(), t.source);
                const auto root = t.destination / t.source.filename();
                // const auto destination = root / relative;

                // logger::trace("relative   : {}", relative.string());
                // logger::trace("root       : {}", root.string());
                // logger::trace("destination: {}", (root / relative).string());

                if (std::filesystem::is_directory(entry.path()))
                {
                    std::filesystem::create_directories(root / relative);
                }
                else
                {
                    auto result = handle_collision(stoken,
                                                   tasks_.at(item.id),
                                                   entry.path(),
                                                   root / relative,
                                                   collision_action);

                    if (result.action == collision_resolve::overwrite_all ||
                        result.action == collision_resolve::skip_all)
                    {
                        collision_action = result.action;
                    }

                    if (result.action == collision_resolve::skip ||
                        result.action == collision_resolve::skip_all)
                    {
                        continue;
                    }
                    else if (result.action == collision_resolve::cancel)
                    {
                        return;
                    }

                    do_copy(entry.path(), result.destination, t.options);
                }
            }
        }
        else
        {
            auto result = handle_collision(stoken,
                                           tasks_.at(item.id),
                                           t.source,
                                           t.destination / t.source.filename(),
                                           collision_action);

            if (result.action == collision_resolve::skip ||
                result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::cancel)
            {
                return;
            }

            do_copy(t.source, result.destination, t.options);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::move_task& task) noexcept
{
    // logger::trace("move: {} -> {}", task.source.string(), task.destination.string());

    auto slot = [this, task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        const auto src_stat = ztd::lstat::create(t.source);
        const auto dest_stat = ztd::stat::create(t.destination);
        const bool same_device = src_stat && dest_stat && (src_stat->dev() == dest_stat->dev());

        bool has_skipped = false;
        auto collision_action = collision_resolve::pending;

        auto do_move = [same_device](const std::filesystem::path& source,
                                     const std::filesystem::path& destination,
                                     const std::filesystem::copy_options options)
        {
            std::filesystem::create_directories(destination.parent_path());

            if (same_device)
            {
                if (std::filesystem::exists(destination))
                {
                    std::filesystem::remove(destination);
                }
                std::filesystem::rename(source, destination);
            }
            else
            {
                std::filesystem::copy_file(source,
                                           destination,
                                           options |
                                               std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(source);
            }
        };

        if (std::filesystem::is_directory(t.source))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(t.source))
            {
                if (!item.check_pause(stoken) || stoken.stop_requested())
                {
                    return;
                }

                const auto relative = std::filesystem::relative(entry.path(), t.source);
                const auto root = t.destination / t.source.filename();
                // const auto destination = root / relative;

                // logger::trace("relative   : {}", relative.string());
                // logger::trace("root       : {}", root.string());
                // logger::trace("destination: {}", (root / relative).string());

                if (std::filesystem::is_directory(entry.path()))
                {
                    std::filesystem::create_directories(root / relative);
                }
                else
                {
                    auto result = handle_collision(stoken,
                                                   tasks_.at(item.id),
                                                   entry.path(),
                                                   root / relative,
                                                   collision_action);

                    if (result.action == collision_resolve::overwrite_all ||
                        result.action == collision_resolve::skip_all)
                    {
                        collision_action = result.action;
                    }

                    if (result.action == collision_resolve::skip ||
                        result.action == collision_resolve::skip_all)
                    {
                        has_skipped = true;
                        continue;
                    }
                    else if (result.action == collision_resolve::cancel)
                    {
                        return;
                    }

                    do_move(entry.path(), result.destination, t.options);
                }
            }

            if (!has_skipped)
            {
                std::filesystem::remove_all(t.source);
            }
        }
        else
        {
            auto result = handle_collision(stoken,
                                           tasks_.at(item.id),
                                           t.source,
                                           t.destination / t.source.filename(),
                                           collision_action);

            if (result.action == collision_resolve::skip ||
                result.action == collision_resolve::skip_all ||
                result.action == collision_resolve::cancel)
            {
                return;
            }

            do_move(t.source, result.destination, t.options);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::trash_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        (void)stoken;
        (void)item;

        // TODO error case
        auto _ = vfs::trash_can::trash(t.path);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::remove_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        if (t.options.contains(vfs::remove_task::options::recursive))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(t.path))
            {
                if (!item.check_pause(stoken) || stoken.stop_requested())
                {
                    return;
                }

                std::filesystem::remove(entry.path());
            }
            std::filesystem::remove(t.path);
        }
        else
        {
            std::filesystem::remove(t.path);
        }
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_directory_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        (void)stoken;
        (void)item;

        std::filesystem::create_directories(t.path);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_file_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        (void)stoken;
        (void)item;

        if (std::filesystem::exists(t.path))
        {
            throw std::filesystem::filesystem_error("Path already exists",
                                                    t.path,
                                                    std::make_error_code(std::errc::file_exists));
        }

        std::ofstream(t.path);
    };
    queue_task(slot);
}

void
vfs::task_manager::add(const vfs::create_symlink_task& task) noexcept
{
    auto slot = [task](const std::stop_token& stoken, task_item& item)
    {
        const auto& t = task;

        (void)stoken;
        (void)item;

        if (t.options.contains(vfs::create_symlink_task::options::force) &&
            std::filesystem::exists(t.name))
        {
            std::filesystem::remove(t.name);
        }
        std::filesystem::create_symlink(t.target, t.name);
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
