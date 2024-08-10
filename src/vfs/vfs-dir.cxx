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

#include <format>

#include <filesystem>

#include <vector>

#include <algorithm>

#include <mutex>

#include <optional>

#include <memory>

#include <chrono>

#include <functional>

#include <fstream>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "concurrency.hxx"

#include "utils/memory.hxx"
#include "utils/write.hxx"

#include "vfs/vfs-file.hxx"
#include "vfs/vfs-thumbnailer.hxx"
#include "vfs/vfs-volume.hxx"

#include "vfs/vfs-dir.hxx"

namespace global
{
ztd::smart_cache<std::filesystem::path, vfs::dir> dir_smart_cache;
}

vfs::dir::dir(const std::filesystem::path& path) noexcept : path_(path)
{
    // logger::debug<logger::domain::vfs>("vfs::dir::dir({})   {}", logger::utils::ptr(this), this->path_.string());
}

vfs::dir::~dir() noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::dir::~dir({})  {}", logger::utils::ptr(this), this->path_.string());

    this->shutdown_ = true;

    this->signal_task_load_dir.disconnect();

    this->evt_file_created.clear();
    this->evt_file_changed.clear();
    this->evt_file_deleted.clear();
    this->evt_file_listed.clear();
    this->evt_file_thumbnail_loaded.clear();

    if (this->change_notify_timeout)
    {
        g_source_remove(this->change_notify_timeout);
    }

    this->executor_result_.get().get();
}

const std::shared_ptr<vfs::dir>
vfs::dir::create(const std::filesystem::path& path) noexcept
{
    std::shared_ptr<vfs::dir> dir = nullptr;
    if (global::dir_smart_cache.contains(path))
    {
        dir = global::dir_smart_cache.at(path);
        // logger::debug<logger::domain::vfs>("vfs::dir::dir({}) cache   {}", logger::utils::ptr(dir.get()), this->path_.string());
    }
    else
    {
        dir = global::dir_smart_cache.create(
            path,
            std::bind([](const auto& path) { return std::make_shared<vfs::dir>(path); }, path));
        // logger::debug<logger::domain::vfs>("vfs::dir::dir({}) new     {}", logger::utils::ptr(dir.get()), this->path_.string());
        dir->post_initialize();
    }
    // logger::debug<logger::domain::vfs>("dir({})     {}", logger::utils::ptr(dir.get()), path.string());
    return dir;
}

void
vfs::dir::post_initialize() noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::dir::post_initialize({})     {}", logger::utils::ptr(this), this->path_.string());

    this->update_avoid_changes();

    this->executor_ = global::runtime.thread_executor();
    this->executor_result_ = this->executor_->submit([this] { return this->load_thread(); });
}

const std::filesystem::path&
vfs::dir::path() const noexcept
{
    return this->path_;
}

const std::span<const std::shared_ptr<vfs::file>>
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
    this->avoid_changes_ = vfs::volume_dir_avoid_changes(this->path_);
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

    std::ifstream file(hidden_path);
    if (!file)
    {
        logger::error<logger::domain::vfs>("Failed to open the file: {}", hidden_path.string());

        this->user_hidden_files_ = std::nullopt;
        return;
    }

    std::vector<std::filesystem::path> hidden;

    std::string line;
    while (std::getline(file, line))
    {
        const std::filesystem::path hidden_file = ztd::strip(line); // remove newline
        if (hidden_file.is_absolute())
        {
            logger::warn<logger::domain::vfs>("Absolute path ignored in {}", hidden_path.string());
            continue;
        }

        hidden.push_back(hidden_file);
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

concurrencpp::result<bool>
vfs::dir::load_thread() noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::dir::load_thread({})   {}", logger::utils::ptr(this), this->path_.string());

    auto guard = co_await this->lock_.lock(this->executor_);

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
                co_return true;
            }

            if (this->is_file_user_hidden(dfile.path()))
            {
                this->xhidden_count_++;
                continue;
            }

            this->files_.push_back(vfs::file::create(dfile.path()));
        }
    }

    this->run_event<spacefm::signal::file_listed>();

    this->load_complete_ = true;
    this->load_complete_initial_ = true;

    co_return true;
}

void
vfs::dir::refresh() noexcept
{
    if (this->load_complete_initial_ && !this->running_refresh_)
    {
        // Only allow a refresh if the inital load has been completed.
        this->executor_result_ = this->executor_->submit([this] { return this->refresh_thread(); });
    }
}

concurrencpp::result<bool>
vfs::dir::refresh_thread() noexcept
{
    auto guard = co_await this->lock_.lock(this->executor_);

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
            this->xhidden_count_++;
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

            this->xhidden_count_++;
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

    co_return true;
}

/* Callback function which will be called when monitored events happen */
void
vfs::dir::on_monitor_event(const vfs::monitor::event event,
                           const std::filesystem::path& path) noexcept
{
    switch (event)
    {
        case vfs::monitor::event::created:
            this->emit_file_created(path, false);
            break;
        case vfs::monitor::event::deleted:
            this->emit_file_deleted(path);
            break;
        case vfs::monitor::event::changed:
            this->emit_file_changed(path, false);
            break;
        case vfs::monitor::event::other:
            break;
    }
}

void
vfs::dir::global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    const auto action = [size](const auto& dir) { dir->unload_thumbnails(size); };
    std::ranges::for_each(global::dir_smart_cache.items(), action);
}

const std::shared_ptr<vfs::file>
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
    const auto file_path = this->path_ / ".hidden";
    const std::string data = std::format("{}\n", file->name());

    return ::utils::write_file(file_path, data);
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
        // logger::debug<logger::domain::vfs>("vfs::dir::load_thumbnail()  {}", file->name());
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
                this->run_event<spacefm::signal::file_deleted>(file);
            }
        }
    }
    return file_updated;
}

static bool
notify_file_change(void* user_data) noexcept
{
    const auto dir = static_cast<vfs::dir*>(user_data)->shared_from_this();

    dir->update_changed_files();
    dir->update_created_files();

    /* remove the timeout */
    dir->change_notify_timeout = 0;
    return false;
}

void
vfs::dir::notify_file_change(const std::chrono::milliseconds timeout) noexcept
{
    if (this->change_notify_timeout == 0)
    {
        this->change_notify_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                         timeout.count(),
                                                         (GSourceFunc)::notify_file_change,
                                                         this,
                                                         nullptr);
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
            this->run_event<spacefm::signal::file_changed>(file);
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
                    this->xhidden_count_++;
                    continue;
                }

                const std::scoped_lock<std::mutex> files_lock(this->files_lock_);

                const auto file = vfs::file::create(full_path);
                this->files_.push_back(file);

                this->run_event<spacefm::signal::file_created>(file);
            }
            // else file does not exist in filesystem
        }
        else
        {
            // file already exists in dir this->files_
            if (this->update_file_info(file_found))
            {
                this->run_event<spacefm::signal::file_changed>(file_found);
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

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
    ::utils::memory_trim();
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

        this->run_event<spacefm::signal::file_deleted>(nullptr);

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
    // logger::info<logger::domain::vfs>("vfs::dir::emit_file_changed dir={} filename={} avoid={}", this->path_, path.filename(), this->avoid_changes_);

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
        this->run_event<spacefm::signal::file_changed>(nullptr);
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

                this->run_event<spacefm::signal::file_changed>(file_found);
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
        this->run_event<spacefm::signal::file_thumbnail_loaded>(file);
    }
}
