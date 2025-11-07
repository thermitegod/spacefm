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

                this->emit_file_created(notification.path(), false);
            })
        .on_events(
            {
                notify::event::delete_self,
                notify::event::delete_sub,
                notify::event::moved_from,
                notify::event::umount,
            },
            [this](const notify::notification& notification)
            {
                // logger::info<logger::vfs>("EVENT(deleted) {}, {}", notification.event(), notification.path().string());

                this->emit_file_deleted(notification.path());
            })
        .on_events(
            {
                notify::event::modify,
                notify::event::attrib,
            },
            [this](const notify::notification& notification)
            {
                // logger::info<logger::vfs>("EVENT(modify) {}, {}", notification.event(), notification.path().string());

                this->emit_file_changed(notification.path(), false);
            })
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
                                                          { this->emit_thumbnail_loaded(a); });

    this->thumbnailer_thread_ = std::jthread([this]() { this->thumbnailer_.run(); });
    pthread_setname_np(this->thumbnailer_thread_.native_handle(), "thumbnailer");
}

vfs::dir::~dir() noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::~dir({})  {}", logger::utils::ptr(this), this->path_.string());

    {
        std::lock_guard<std::mutex> lock(this->loader_mutex_);
        this->shutdown_ = true;
    }

    this->signal_file_created_.clear();
    this->signal_file_changed_.clear();
    this->signal_file_deleted_.clear();
    this->signal_file_listed_.clear();
    this->signal_file_thumbnail_loaded_.clear();

    this->thumbnailer_.stop();
    this->thumbnailer_thread_.join();

    this->notifier_.stop();
    this->notifier_thread_.join();

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
        dir->post_initialize();
    }
    // logger::debug<logger::vfs>("dir({})     {}", logger::utils::ptr(dir.get()), path.string());
    return dir;
}

void
vfs::dir::post_initialize() noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::post_initialize({})     {}", logger::utils::ptr(this), this->path_.string());

    this->update_avoid_changes();

    this->loader_thread_ = std::jthread([this]() { this->load_thread(); });
    pthread_setname_np(this->loader_thread_.native_handle(), "loader");
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
    // Read .hidden into string

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
    // ignore if in .hidden
    if (this->user_hidden_files_)
    {
        const auto filename = path.filename();

        const auto is_user_hidden = [&filename](const auto& hide_filename)
        { return filename == hide_filename; };
        return std::ranges::any_of(this->user_hidden_files_.value(), is_user_hidden);
    }
    return false;
}

void
vfs::dir::load_thread() noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::load_thread({})   {}", logger::utils::ptr(this), this->path_.string());

    std::unique_lock<std::mutex> lock(this->loader_mutex_);

    this->load_complete_ = false;
    this->xhidden_count_ = 0;

    // load this dirs .hidden file
    this->load_user_hidden_files();

    {
        const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

        for (const auto& dfile : std::filesystem::directory_iterator(this->path_))
        {
            if (this->shutdown_)
            {
                return;
            }

            if (this->is_file_user_hidden(dfile.path()))
            {
                this->xhidden_count_ += 1;
                continue;
            }

            this->files_.push_back(vfs::file::create(dfile.path(), this->settings_));
        }
    }

    this->signal_file_listed_.emit();

    this->load_complete_ = true;
    this->load_complete_initial_ = true;
}

void
vfs::dir::refresh() noexcept
{
    if (this->load_complete_initial_ && !this->running_refresh_)
    {
        // Only allow a refresh if the inital load has been completed.
        this->loader_thread_ = std::jthread([this]() { this->refresh_thread(); });
        pthread_setname_np(this->loader_thread_.native_handle(), "loader");
    }
}

void
vfs::dir::refresh_thread() noexcept
{
    std::unique_lock<std::mutex> lock(this->loader_mutex_);

    this->load_complete_ = false;
    this->running_refresh_ = true;

    this->xhidden_count_ = 0;

    // reload this dirs .hidden file
    this->load_user_hidden_files();

    for (const auto& dfile : std::filesystem::directory_iterator(this->path_))
    {
        if (this->shutdown_)
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
            this->emit_file_created(filename, false);
        }
    }

    for (const auto& file : this->files_)
    {
        if (this->shutdown_)
        {
            break;
        }

        // Check if existing files have been hidden
        if (this->is_file_user_hidden(file->path()))
        {
            // Use the delete signal to properly remove this file from the file list.
            this->emit_file_deleted(file->name());

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

    this->load_complete_ = true;
    this->running_refresh_ = false;
}

void
vfs::dir::global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    const auto action = [size](const auto& dir) { dir->unload_thumbnails(size); };
    std::ranges::for_each(global::dir_smart_cache.items(), action);
}

std::shared_ptr<vfs::file>
vfs::dir::find_file(const std::filesystem::path& filename) noexcept
{
    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    const auto action = [&filename](const auto& file) { return file->name() == filename.string(); };
    const auto it = std::ranges::find_if(this->files_, action);
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

bool
vfs::dir::is_loading() const noexcept
{
    return !this->load_complete_;
}

bool
vfs::dir::is_loaded() const noexcept
{
    return this->load_complete_;
}

bool
vfs::dir::is_directory_empty() const noexcept
{
    return this->files_.empty();
}

bool
vfs::dir::update_file_info(const std::shared_ptr<vfs::file>& file) noexcept
{
    const bool file_updated = file->update();
    if (!file_updated)
    { /* The file does not exist */
        if (std::ranges::contains(this->files_, file))
        {
            const std::scoped_lock<std::mutex> files_lock(this->files_lock_);
            // TODO - FIXME - using std::ranges::remove here will
            // caues a segfault when deleting/moving/loading thumbails
            // std::ranges::remove(this->files_, file);
            this->files_.erase(std::remove(this->files_.begin(), this->files_.end(), file),
                               this->files_.end());
            if (file)
            {
                this->signal_file_deleted_.emit(file);
            }
        }
    }
    return file_updated;
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
                this->update_changed_files();
                this->update_created_files();

                this->timer_running_ = false;
            },
            static_cast<std::uint32_t>(timeout.count()),
            Glib::PRIORITY_LOW);
    }
}

void
vfs::dir::update_changed_files() noexcept
{
    const std::scoped_lock<std::mutex> changed_files_lock(this->changed_files_lock_);

    if (this->changed_files_.empty())
    {
        return;
    }

    for (const auto& file : this->changed_files_)
    {
        if (this->update_file_info(file))
        {
            this->signal_file_changed_.emit(file);
        }
        // else was deleted, signaled, and unrefed in update_file_info
    }
    this->changed_files_.clear();
}

void
vfs::dir::update_created_files() noexcept
{
    const std::scoped_lock<std::mutex> created_files_lock(this->created_files_lock_);

    if (this->created_files_.empty())
    {
        return;
    }

    for (const auto& created_file : this->created_files_)
    {
        const auto file_found = this->find_file(created_file);
        if (!file_found)
        {
            // file is not in dir this->files_
            const auto full_path = this->path_ / created_file;
            if (std::filesystem::exists(full_path))
            {
                if (this->is_file_user_hidden(created_file))
                {
                    this->xhidden_count_ += 1;
                    continue;
                }

                const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

                const auto file = vfs::file::create(full_path, this->settings_);
                this->files_.push_back(file);

                this->signal_file_created_.emit(file);
            }
            // else file does not exist in filesystem
        }
        else
        {
            // file already exists in dir this->files_
            if (this->update_file_info(file_found))
            {
                this->signal_file_changed_.emit(file_found);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
    }
    this->created_files_.clear();
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

/* signal handlers */
void
vfs::dir::emit_file_created(const std::filesystem::path& path, bool force) noexcept
{
    if (this->shutdown_)
    {
        return;
    }

    // Ignore avoid_changes for creation of files
    if (!force && this->avoid_changes_)
    {
        return;
    }

    if (path == this->path_)
    { // Special Case: The directory itself was created?
        return;
    }

    const std::scoped_lock<std::mutex> created_files_lock(this->created_files_lock_);

    this->created_files_.push_back(path.filename());

    this->notify_file_change(std::chrono::milliseconds(200));
}

void
vfs::dir::emit_file_deleted(const std::filesystem::path& path) noexcept
{
    if (this->shutdown_)
    {
        return;
    }

    if (path.filename() == this->path_.filename() && !std::filesystem::exists(this->path_))
    {
        /* Special Case: The directory itself was deleted... */
        const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

        /* clear the whole list */
        this->files_.clear();

        this->signal_file_deleted_.emit(nullptr);

        return;
    }

    const auto file_found = this->find_file(path.filename());
    if (file_found)
    {
        const std::scoped_lock<std::mutex> changed_files_lock(this->changed_files_lock_);

        if (!std::ranges::contains(this->changed_files_, file_found))
        {
            this->changed_files_.push_back(file_found);

            this->notify_file_change(std::chrono::milliseconds(200));
        }
    }
}

void
vfs::dir::emit_file_changed(const std::filesystem::path& path, bool force) noexcept
{
    // logger::info<logger::vfs>("vfs::dir::emit_file_changed dir={} filename={} avoid={}", this->path_, path.filename(), this->avoid_changes_);

    if (this->shutdown_)
    {
        return;
    }

    if (!force && this->avoid_changes_)
    {
        return;
    }

    if (path == this->path_)
    {
        // Special Case: The directory itself was changed
        this->signal_file_changed_.emit(nullptr);
        return;
    }

    const auto file_found = this->find_file(path.filename());
    if (file_found)
    {
        const std::scoped_lock<std::mutex> changed_files_lock(this->changed_files_lock_);

        if (!std::ranges::contains(this->changed_files_, file_found))
        {
            if (force)
            {
                this->changed_files_.push_back(file_found);

                this->notify_file_change(std::chrono::milliseconds(100));
            }
            else if (this->update_file_info(file_found)) // update file info the first time
            {
                this->changed_files_.push_back(file_found);

                this->notify_file_change(std::chrono::milliseconds(500));

                this->signal_file_changed_.emit(file_found);
            }
        }
    }
}

void
vfs::dir::emit_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (this->shutdown_)
    {
        return;
    }

    const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

    if (std::ranges::contains(this->files_, file))
    {
        this->signal_file_thumbnail_loaded_.emit(file);
    }
}
