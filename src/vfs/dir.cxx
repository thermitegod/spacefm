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
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

#include <pthread.h>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/dir.hxx"
#include "vfs/file.hxx"
#include "vfs/notify-cpp/event.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"
#include "vfs/settings.hxx"
#include "vfs/thumbnailer.hxx"
#include "vfs/volume.hxx"

#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

namespace global
{
static ztd::smart_cache<std::filesystem::path, vfs::dir> dir_smart_cache;
}

vfs::dir::dir(const std::filesystem::path& path,
              const std::shared_ptr<vfs::settings>& settings) noexcept
    : path_(path), settings_(settings)
{
    // logger::debug<logger::vfs>("vfs::dir::dir({})   {}", logger::utils::ptr(this), this->path_.string());

    this->notifier_
        .watch_directory({path,
                          {
                              notify::event::create,
                              notify::event::moved_to,
                              notify::event::delete_self,
                              notify::event::delete_sub,
                              notify::event::moved_from,
                              notify::event::umount,
                              notify::event::modify,
                              notify::event::attrib,
                          }})
        .on_events(
            {
                notify::event::create,
                notify::event::moved_to,
            },
            [this](const notify::notification& notification)
            {
                // logger::info<logger::vfs>("EVENT(created) {}, {}", notification.event(), notification.path().string());

                this->on_file_created(notification.path());
            })
        .on_events(
            {
                notify::event::delete_sub,
                notify::event::moved_from,
            },
            [this](const notify::notification& notification)
            {
                // logger::info<logger::vfs>("EVENT(deleted) {}, {}", notification.event(), notification.path().string());

                this->on_file_deleted(notification.path());
            })
        .on_events(
            {
                notify::event::modify,
                notify::event::attrib,
            },
            [this](const notify::notification& notification)
            {
                // logger::info<logger::vfs>("EVENT(modify) {}, {}", notification.event(), notification.path().string());

                this->on_file_changed(notification.path());
            })
        .on_events(
            {
                notify::event::delete_self,
                notify::event::umount,
            },
            [this](const notify::notification&)
            {
                // logger::info<logger::vfs>("EVENT(self delete) {}, {}", notification.event(), notification.path().string());

                this->signal_directory_deleted().emit();
            })
        .on_event(notify::event::ignored, [](const notify::notification&) { /* NOOP */ })
        .on_unexpected_event(
            [](const notify::notification& notification)
            {
                logger::warn<logger::vfs>("BUG unhandled inotify event: {}, {}",
                                          notification.event(),
                                          notification.path().string());
            });
    this->notifier_thread_ = std::jthread([this]() { this->notifier_.run(); });
    pthread_setname_np(this->notifier_thread_.native_handle(), "notifier");

    this->thumbnailer_.signal_thumbnail_created().connect([this](auto a)
                                                          { this->on_thumbnail_loaded(a); });

    this->thumbnailer_thread_ =
        std::jthread([this](const std::stop_token& stoken) { this->thumbnailer_.run(stoken); });
    pthread_setname_np(this->thumbnailer_thread_.native_handle(), "thumbnailer");

    this->update_avoid_changes();

    this->loader_thread_ =
        std::jthread([this](const std::stop_token& stoken) { this->load_thread(stoken); });
    pthread_setname_np(this->loader_thread_.native_handle(), "loader");
}

vfs::dir::~dir() noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::~dir({})  {}", logger::utils::ptr(this), this->path_.string());

    this->signal_files_created().clear();
    this->signal_files_changed().clear();
    this->signal_files_deleted().clear();

    this->signal_file_listed().clear();
    this->signal_thumbnail_loaded().clear();
    this->signal_directory_deleted().clear();

    this->thumbnailer_thread_.request_stop();
    this->thumbnailer_thread_.join();

    this->notifier_.stop();
    this->notifier_thread_.join();

    this->loader_thread_.request_stop();
    this->loader_thread_.join();
}

std::shared_ptr<vfs::dir>
vfs::dir::create(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings,
                 const bool permanent) noexcept
{
    std::shared_ptr<vfs::dir> dir = nullptr;
    if (global::dir_smart_cache.contains(path))
    {
        dir = global::dir_smart_cache.at(path);
        // logger::debug<logger::vfs>("vfs::dir::dir({}) cache   {}", logger::utils::ptr(dir.get()), this->path_.string());
    }
    else
    {
        dir = global::dir_smart_cache.create(
            path,
            [&path, &settings]() { return std::make_shared<vfs::dir>(path, settings); },
            permanent);
        // logger::debug<logger::vfs>("vfs::dir::dir({}) new     {}", logger::utils::ptr(dir.get()), this->path_.string());
    }
    // logger::debug<logger::vfs>("dir({})     {}", logger::utils::ptr(dir.get()), path.string());
    return dir;
}

const std::filesystem::path&
vfs::dir::path() const noexcept
{
    return this->path_;
}

std::span<const std::shared_ptr<vfs::file>>
vfs::dir::files() const noexcept
{
    return this->files_;
}

bool
vfs::dir::avoid_changes() const noexcept
{
    return this->avoid_changes_;
}

u64
vfs::dir::hidden_files() const noexcept
{
    return this->xhidden_count_;
}

void
vfs::dir::update_avoid_changes() noexcept
{
#if (GTK_MAJOR_VERSION == 4) // TODO
    this->avoid_changes_ = false;
#elif (GTK_MAJOR_VERSION == 3)
    this->avoid_changes_ = vfs::volume_dir_avoid_changes(this->path_);
#endif
}

void
vfs::dir::load_user_hidden_files() noexcept
{
    const auto hidden_path = this->path_ / ".hidden";

    if (!std::filesystem::is_regular_file(hidden_path))
    {
        this->user_hidden_files_ = std::nullopt;
        return;
    }

    const auto buffer = vfs::utils::read_file(hidden_path);
    if (!buffer)
    {
        logger::error<logger::vfs>("Failed to read .hidden file: {} {}",
                                   hidden_path.string(),
                                   buffer.error().message());
        this->user_hidden_files_ = std::nullopt;
        return;
    }

    std::vector<std::filesystem::path> hidden;

    for (const std::filesystem::path file : ztd::split(*buffer, "\n"))
    {
        if (file.is_absolute())
        {
            logger::warn<logger::vfs>("Absolute path ignored in {}", hidden_path.string());
            continue;
        }

        hidden.push_back(file);
    }

    this->user_hidden_files_ = hidden;
}

bool
vfs::dir::is_file_user_hidden(const std::filesystem::path& path) const noexcept
{
    if (!this->user_hidden_files_)
    {
        return false;
    }

    const auto filename = path.filename();

    return std::ranges::any_of(*this->user_hidden_files_,
                               [&filename](const auto& hide_filename)
                               { return filename == hide_filename; });
}

void
vfs::dir::load_thread(const std::stop_token& stoken) noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::load_thread({})   {}", logger::utils::ptr(this), this->path_.string());

    std::unique_lock<std::mutex> lock(this->loader_mutex_);

    this->load_running_ = true;
    this->xhidden_count_ = 0;

    // load this dirs .hidden file
    this->load_user_hidden_files();

    for (const auto& dfile : std::filesystem::directory_iterator(this->path_))
    {
        if (stoken.stop_requested())
        {
            return;
        }

        if (this->is_file_user_hidden(dfile.path()))
        {
            this->xhidden_count_ += 1;
            continue;
        }

        {
            const std::scoped_lock<std::mutex> files_lock(this->files_lock_);
            this->files_.push_back(vfs::file::create(dfile.path(), this->settings_));
        }
    }

    this->load_running_ = false;

    this->signal_file_listed().emit();
}

void
vfs::dir::refresh() noexcept
{
    if (!this->load_running_)
    {
        this->loader_thread_ =
            std::jthread([this](const std::stop_token& stoken) { this->refresh_thread(stoken); });
        pthread_setname_np(this->loader_thread_.native_handle(), "loader");
    }
}

void
vfs::dir::refresh_thread(const std::stop_token& stoken) noexcept
{
    std::unique_lock<std::mutex> lock(this->loader_mutex_);

    this->load_running_ = true;
    this->xhidden_count_ = 0;

    // reload this dirs .hidden file
    this->load_user_hidden_files();

    for (const auto& dfile : std::filesystem::directory_iterator(this->path_))
    {
        if (stoken.stop_requested())
        {
            break;
        }

        // Check if new files are hidden
        if (this->is_file_user_hidden(dfile.path()))
        {
            this->xhidden_count_ += 1;
            continue;
        }

        const auto filename = dfile.path().filename();
        if (this->find_file(filename) == nullptr)
        {
            this->on_file_created(filename);
        }
    }

    std::vector<std::shared_ptr<vfs::file>> new_hidden;
    {
        const std::scoped_lock<std::mutex> files_lock(this->files_lock_);
        for (const auto& file : this->files_)
        {
            if (stoken.stop_requested())
            {
                break;
            }

            // Check if existing files have been hidden
            if (this->is_file_user_hidden(file->path()))
            {
                // Use the delete signal to properly remove this file from the file list.
                new_hidden.push_back(file);

                this->xhidden_count_ += 1;
                continue;
            }

            // reload thumbnails if already loaded
            if (file->is_thumbnail_loaded(vfs::file::thumbnail_size::big))
            {
                file->unload_thumbnail(vfs::file::thumbnail_size::big);
                file->load_thumbnail(vfs::file::thumbnail_size::big);
            }
            if (file->is_thumbnail_loaded(vfs::file::thumbnail_size::small))
            {
                file->unload_thumbnail(vfs::file::thumbnail_size::small);
                file->load_thumbnail(vfs::file::thumbnail_size::small);
            }
        }
    }

    // have to do this here because on_file_deleted() also uses files_lock_
    std::ranges::for_each(new_hidden,
                          [this](const auto& file) { this->on_file_deleted(file->name()); });

    this->load_running_ = false;

    this->signal_file_listed().emit();
}

void
vfs::dir::global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    std::ranges::for_each(global::dir_smart_cache.items(),
                          [size](const auto& dir) { dir->unload_thumbnails(size); });
}

std::shared_ptr<vfs::file>
vfs::dir::find_file(const std::filesystem::path& filename) noexcept
{
    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    const auto it = std::ranges::find_if(this->files_,
                                         [&filename](const auto& file)
                                         { return file->name() == filename.string(); });
    if (it != this->files_.cend())
    {
        return *it;
    }
    return nullptr;
}

bool
vfs::dir::add_hidden(const std::shared_ptr<vfs::file>& file) const noexcept
{
    return vfs::utils::write_file(this->path_ / ".hidden", std::format("{}\n", file->name())) ==
           vfs::error_code::none;
}

void
vfs::dir::enable_thumbnails(const bool enabled) noexcept
{
    this->enable_thumbnails_ = enabled;
}

void
vfs::dir::load_thumbnail(const std::shared_ptr<vfs::file>& file,
                         const vfs::file::thumbnail_size size) noexcept
{
    if (this->enable_thumbnails_)
    {
        // logger::debug<logger::vfs>("vfs::dir::load_thumbnail()  {}", file->name());
        this->thumbnailer_.request({file, size});
    }
}

void
vfs::dir::unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    for (const auto& file : this->files_)
    {
        file->unload_thumbnail(size);
    }
}

bool
vfs::dir::is_loading() const noexcept
{
    return this->load_running_;
}

bool
vfs::dir::is_loaded() const noexcept
{
    return !this->load_running_;
}

bool
vfs::dir::is_directory_empty() const noexcept
{
    return this->files_.empty();
}

bool
vfs::dir::update_file(const std::shared_ptr<vfs::file>& file) noexcept
{
    const bool updated = file->update();
    if (!updated)
    { /* The file does not exist */
        if (std::ranges::contains(this->files_, file))
        {
            this->remove_file(file);

            this->events_.deleted.push_back(file);
        }
    }
    return updated;
}

void
vfs::dir::remove_file(const std::shared_ptr<vfs::file>& file) noexcept
{
    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    this->files_.erase(std::ranges::remove(this->files_, file).begin(), this->files_.end());
}

void
vfs::dir::notify_file_change(const std::chrono::milliseconds timeout) noexcept
{
    if (!this->timer_running_)
    {
        this->timer_running_ = true;

        this->timer_.connect_once(
            [this]()
            {
                {
                    const std::scoped_lock<std::mutex> lock(this->events_.deleted_lock);
                    this->update_deleted_files();
                    this->events_.deleted.clear();
                }

                {
                    const std::scoped_lock<std::mutex> lock(this->events_.changed_lock);
                    this->update_changed_files();
                    this->events_.changed.clear();
                }

                {
                    const std::scoped_lock<std::mutex> lock(this->events_.created_lock);
                    this->update_created_files();
                    this->events_.created.clear();
                }

                this->timer_running_ = false;
            },
            static_cast<std::uint32_t>(timeout.count()),
            Glib::PRIORITY_LOW);
    }
}

void
vfs::dir::update_deleted_files() noexcept
{
    if (this->events_.deleted.empty())
    {
        return;
    }

    for (const auto& file : this->events_.deleted)
    {
        this->remove_file(file);
    }

    this->signal_files_deleted().emit(this->events_.deleted);
}

void
vfs::dir::update_changed_files() noexcept
{
    if (this->events_.changed.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<vfs::file>> changed_files;
    for (const auto& file : this->events_.changed)
    {
        if (this->update_file(file))
        {
            changed_files.push_back(file);
        }
    }
    this->signal_files_changed().emit(changed_files);
}

void
vfs::dir::update_created_files() noexcept
{
    if (this->events_.created.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<vfs::file>> created_files;
    for (const auto& created_file : this->events_.created)
    {
        const auto file = this->find_file(created_file);
        if (!file)
        {
            // file is not in this->files_
            const auto file_path = this->path_ / created_file;
            if (std::filesystem::exists(file_path))
            {
                if (this->is_file_user_hidden(created_file))
                {
                    this->xhidden_count_ += 1;
                    continue;
                }

                const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

                const auto new_file = vfs::file::create(file_path, this->settings_);
                this->files_.push_back(new_file);

                created_files.push_back(new_file);
            }
        }
        else
        {
            // file already exists in this->files_
            if (this->update_file(file))
            {
                created_files.push_back(file);
            }
        }
    }

    this->signal_files_created().emit(created_files);
}

void
vfs::dir::on_file_created(const std::filesystem::path& path) noexcept
{
    if (this->avoid_changes_)
    {
        return;
    }

    {
        const std::scoped_lock<std::mutex> lock(this->events_.created_lock);
        this->events_.created.push_back(path.filename());
    }

    this->notify_file_change(std::chrono::milliseconds(200));
}

void
vfs::dir::on_file_deleted(const std::filesystem::path& path) noexcept
{
    if (this->avoid_changes_)
    {
        return;
    }

    const auto file = this->find_file(path.filename());
    if (file)
    {
        const std::scoped_lock<std::mutex> lock(this->events_.deleted_lock);
        if (!std::ranges::contains(this->events_.deleted, file))
        {
            this->events_.deleted.push_back(file);
        }
    }

    this->notify_file_change(std::chrono::milliseconds(200));
}

void
vfs::dir::on_file_changed(const std::filesystem::path& path) noexcept
{
    if (this->avoid_changes_)
    {
        return;
    }

    if (this->path_ == path)
    { // Special Case: The directory itself was changed
        return;
    }

    const auto file = this->find_file(path.filename());
    if (file)
    {
        const std::scoped_lock<std::mutex> lock(this->events_.changed_lock);
        if (!std::ranges::contains(this->events_.changed, file))
        {
            if (this->update_file(file)) // update file info the first time
            {
                this->events_.changed.push_back(file);
            }
        }
    }

    this->notify_file_change(std::chrono::milliseconds(500));
}

void
vfs::dir::on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    if (std::ranges::contains(this->files_, file))
    {
        this->signal_thumbnail_loaded().emit(file);
    }
}
