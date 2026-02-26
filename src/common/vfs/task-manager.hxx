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

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

#include <cstdint>

#include <sigc++/sigc++.h>

namespace vfs
{
struct chmod_task final
{
    enum class options
    {
        recursive,
    };

    std::set<options> options;
    std::filesystem::perms mode;
    std::filesystem::path path;
};

struct chown_task final
{
    enum class options
    {
        recursive,
    };

    std::set<options> options;
    std::string user;
    std::string group;
    std::filesystem::path path;
};

struct copy_task final
{
    std::filesystem::copy_options options = std::filesystem::copy_options::none;
    std::filesystem::path source;
    std::filesystem::path destination;
};

struct move_task final
{
    std::filesystem::copy_options options = std::filesystem::copy_options::none;
    std::filesystem::path source;
    std::filesystem::path destination;
};

struct trash_task final
{
    std::filesystem::path path;
};

struct remove_task final
{
    enum class options
    {
        recursive,
        force,
    };

    std::set<options> options;
    std::filesystem::path path;
};

struct create_directory_task final
{
    std::filesystem::path path;
};

struct create_file_task final
{
    std::filesystem::path path;
};

struct create_symlink_task final
{
    enum class options
    {
        force,
    };

    std::set<options> options;
    std::filesystem::path target;
    std::filesystem::path name;
};

enum class collision_resolve
{
    none,
    pending,
    overwrite,     // Overwrite current file / Ask
    overwrite_all, // Overwrite all existing files without prompt
    skip,          // Do not overwrite current file
    skip_all,      // Do not overwrite any files
    rename,        // Rename file
    merge,
    cancel,
};

struct task_collision final
{
    std::uint64_t task_id;
    std::filesystem::path source;
    std::filesystem::path destination;

    std::function<void(std::uint64_t, collision_resolve, std::filesystem::path)> resolved;
};

struct task_error final
{
    std::uint64_t task_id;
    std::string message;
};

class task_manager final
{
  public:
    task_manager() noexcept;
    ~task_manager() noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::task_manager> create() noexcept;

    void add(const vfs::chmod_task& task) noexcept;
    void add(const vfs::chown_task& task) noexcept;
    void add(const vfs::copy_task& task) noexcept;
    void add(const vfs::move_task& task) noexcept;
    void add(const vfs::trash_task& task) noexcept;
    void add(const vfs::remove_task& task) noexcept;
    void add(const vfs::create_directory_task& task) noexcept;
    void add(const vfs::create_file_task& task) noexcept;
    void add(const vfs::create_symlink_task& task) noexcept;

    void resume(const std::uint64_t task_id) noexcept;
    void pause(const std::uint64_t task_id) noexcept;
    void stop(const std::uint64_t task_id) noexcept;
    void remove(const std::uint64_t task_id) noexcept;

    void resume_all() noexcept;
    void pause_all() noexcept;
    void stop_all() noexcept;
    void remove_all() noexcept;

    [[nodiscard]] bool empty() noexcept;

  private:
    struct task_item final
    {
        enum class status
        {
            pending,
            running,
            paused,
            finished,
            error,
        };

        task_item(const std::uint64_t task_id) : id(task_id) {}

        std::uint64_t id;
        std::stop_source stop_source;
        std::atomic<status> state{status::pending};
        std::function<void(const std::stop_token&, task_item&)> action;

        // pause/stop handling
        std::mutex pause_mutex;
        std::condition_variable_any pause_cv;

        [[nodiscard]] bool
        check_pause(const std::stop_token& stoken) noexcept
        {
            if (stoken.stop_requested() || stop_source.stop_requested())
            {
                return false;
            }

            if (state == status::paused)
            {
                std::unique_lock p_lock(pause_mutex);
                pause_cv.wait(p_lock,
                              stoken,
                              [this, &stoken]
                              {
                                  return state != status::paused || stoken.stop_requested() ||
                                         stop_source.stop_requested();
                              });
            }

            return !stoken.stop_requested() && !stop_source.stop_requested();
        }

        // collision handling
        collision_resolve resolve_action{collision_resolve::pending};
        std::filesystem::path new_name; // only for collision_resolve::rename
        std::mutex collision_mutex;
        std::condition_variable_any collision_cv;

        void
        wait_for_resolve(const std::stop_token& stoken) noexcept
        {
            std::unique_lock c_lock(collision_mutex);
            collision_cv.wait(c_lock,
                              stoken,
                              [this, &stoken]
                              {
                                  return resolve_action != collision_resolve::pending ||
                                         stoken.stop_requested();
                              });
        }
    };

    std::deque<std::uint64_t> queue_;
    std::unordered_map<std::uint64_t, std::shared_ptr<task_item>> tasks_;
    std::jthread thread_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::uint64_t next_task_id_ = 0;
    std::uint64_t active_task_id_ = 0;

    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

    [[nodiscard]] std::uint64_t
    create_task_id() noexcept
    {
        std::scoped_lock lock(mutex_);
        next_task_id_ += 1;
        return next_task_id_;
    }

    template<typename T, typename Func>
    void
    queue_task(const T& task, Func&& slot) noexcept
    {
        auto item = std::make_shared<task_item>(create_task_id());
        item->action =
            [this, item, task, slot = std::forward<Func>(slot)](const std::stop_token& stoken,
                                                                task_item& self) noexcept
        {
            if (stoken.stop_requested())
            {
                return;
            }

            try
            {
                slot(stoken, self, task);

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

    struct collision_result final
    {
        vfs::collision_resolve action;
        std::filesystem::path destination;
    };
    [[nodiscard]] collision_result
    handle_collision(const std::stop_token& stoken, const std::shared_ptr<task_item>& item,
                     const std::filesystem::path& source, const std::filesystem::path& destination,
                     const vfs::collision_resolve default_action) noexcept;

  public:
    [[nodiscard]] auto
    signal_task_added() noexcept
    {
        return this->signal_task_added_;
    }

    [[nodiscard]] auto
    signal_task_finished() noexcept
    {
        return this->signal_task_finished_;
    }

    [[nodiscard]] auto
    signal_task_error() noexcept
    {
        return this->signal_task_error_;
    }

    [[nodiscard]] auto
    signal_task_collision() noexcept
    {
        return signal_task_collision_;
    }

  private:
    sigc::signal<void(std::uint64_t)> signal_task_added_;
    sigc::signal<void(std::uint64_t)> signal_task_finished_;
    sigc::signal<void(vfs::task_error)> signal_task_error_;
    sigc::signal<void(vfs::task_collision)> signal_task_collision_;
};
} // namespace vfs
