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

#include <functional>

#include <fstream>

#include <cassert>

#include <glibmm.h>

#include <malloc.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-volume.hxx"
#include "vfs/vfs-thumbnailer.hxx"

#include "vfs/vfs-async-thread.hxx"

#include "vfs/vfs-dir.hxx"

static void vfs_dir_monitor_callback(const vfs::monitor::event event,
                                     const std::filesystem::path& path, void* user_data);

static ztd::smart_cache<std::filesystem::path, vfs::dir> dir_smart_cache;

vfs::dir::dir(const std::filesystem::path& path) : path_(path)
{
    // ztd::logger::debug("vfs::dir::dir({})   {}", fmt::ptr(this), path);

    this->avoid_changes = vfs_volume_dir_avoid_changes(this->path_);

    this->task = vfs::async_thread::create(std::bind(&vfs::dir::load_thread, this));

    this->signal_task_load_dir = this->task->add_event<spacefm::signal::task_finish>(
        std::bind(&vfs::dir::on_list_task_finished, this, std::placeholders::_1));

    this->task->run(); /* asynchronous operation */
}

vfs::dir::~dir()
{
    // ztd::logger::debug("vfs::dir::~dir({})  {}", fmt::ptr(this), path);

    this->signal_task_load_dir.disconnect();

    if (this->task)
    {
        // FIXME: should we generate a "file-list" signal to indicate the dir loading was cancelled?

        // ztd::logger::trace("this->task({})", fmt::ptr(this->task));
        this->task->cancel();
        this->task = nullptr;
    }

    // ztd::logger::trace("this->monitor({})", fmt::ptr(this->monitor));
    // ztd::logger::trace("this->thumbnailer({})", fmt::ptr(this->thumbnailer));
    this->monitor = nullptr;
    this->thumbnailer = nullptr;

    this->file_list.clear();
    this->changed_files.clear();
    this->created_files.clear();
}

const std::shared_ptr<vfs::dir>
vfs::dir::create(const std::filesystem::path& path) noexcept
{
    std::shared_ptr<vfs::dir> dir = nullptr;
    if (dir_smart_cache.contains(path))
    {
        dir = dir_smart_cache.at(path);
        // ztd::logger::debug("vfs::dir::dir({}) cache   {}", fmt::ptr(dir.get()), path);
    }
    else
    {
        dir = dir_smart_cache.create(
            path,
            std::bind([](const auto& path) { return std::make_shared<vfs::dir>(path); }, path));
        // ztd::logger::debug("vfs::dir::dir({}) new     {}", fmt::ptr(dir.get()), path);
    }
    // ztd::logger::debug("dir({})     {}", fmt::ptr(dir.get()), path);
    return dir;
}

void
vfs::dir::on_list_task_finished(bool is_cancelled)
{
    this->task = nullptr;
    this->run_event<spacefm::signal::file_listed>(is_cancelled);
    this->file_listed = true;
    this->load_complete = true;
}

const std::filesystem::path&
vfs::dir::path() const noexcept
{
    return this->path_;
}

const std::optional<std::vector<std::filesystem::path>>
vfs::dir::get_hidden_files() const noexcept
{
    std::vector<std::filesystem::path> hidden;

    // Read .hidden into string
    const auto hidden_path = this->path_ / ".hidden";

    if (!std::filesystem::is_regular_file(hidden_path))
    {
        return std::nullopt;
    }

    // test access first because open() on missing file may cause
    // long delay on nfs
    if (!have_rw_access(hidden_path))
    {
        return std::nullopt;
    }

    std::ifstream file(hidden_path);
    if (!file)
    {
        ztd::logger::error("Failed to open the file: {}", hidden_path.string());
        return std::nullopt;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const auto hidden_file = std::filesystem::path(ztd::strip(line));
        if (hidden_file.is_absolute())
        {
            ztd::logger::warn("Absolute path ignored in {}", hidden_path.string());
            continue;
        }

        hidden.push_back(hidden_file);
    }
    file.close();

    return hidden;
}

void
vfs::dir::load_thread()
{
    this->file_listed = false;
    this->load_complete = false;
    this->xhidden_count = 0;

    /* Install file alteration monitor */
    if (!this->monitor)
    {
        this->monitor = vfs::monitor::create(this->path_, vfs_dir_monitor_callback, this);
    }

    // MOD  dir contains .hidden file?
    const auto hidden_files = this->get_hidden_files();

    for (const auto& dfile : std::filesystem::directory_iterator(this->path_))
    {
        if (this->task->is_canceled())
        {
            break;
        }

        const auto file_name = dfile.path().filename();
        const auto full_path = this->path_ / file_name;

        // MOD ignore if in .hidden
        if (hidden_files)
        {
            bool hide_file = false;
            for (const auto& hidden_file : hidden_files.value())
            {
                // if (ztd::same(hidden_file.string(), file_name.string()))
                std::error_code ec;
                const bool equivalent = std::filesystem::equivalent(hidden_file, file_name, ec);
                if (!ec && equivalent)
                {
                    hide_file = true;
                    this->xhidden_count++;
                    break;
                }
            }
            if (hide_file)
            {
                continue;
            }
        }

        const auto file = vfs::file::create(full_path);
        this->file_list.emplace_back(file);
    }
}

/* Callback function which will be called when monitored events happen */
static void
vfs_dir_monitor_callback(const vfs::monitor::event event, const std::filesystem::path& path,
                         void* user_data)
{
    assert(user_data != nullptr);
    const auto dir = static_cast<vfs::dir*>(user_data)->shared_from_this();

    switch (event)
    {
        case vfs::monitor::event::created:
            dir->emit_file_created(path.filename(), false);
            break;
        case vfs::monitor::event::deleted:
            dir->emit_file_deleted(path.filename(), nullptr);
            break;
        case vfs::monitor::event::changed:
            dir->emit_file_changed(path.filename(), nullptr, false);
            break;
        case vfs::monitor::event::other:
            break;
    }
}

void
vfs_dir_mime_type_reload()
{
    // ztd::logger::debug("reload mime-type");
    // const auto action = [](const auto& dir) { dir.second.lock()->reload_mime_type(); };
    // std::ranges::for_each(dir_smart_cache, action);
}

/**
* vfs::dir class
*/

const std::shared_ptr<vfs::file>
vfs::dir::find_file(const std::filesystem::path& file_name,
                    const std::shared_ptr<vfs::file>& file) const noexcept
{
    for (const auto& file2 : this->file_list)
    {
        if (file == file2)
        {
            return file2;
        }
        if (ztd::same(file2->name(), file_name.string()))
        {
            return file2;
        }
    }
    return nullptr;
}

bool
vfs::dir::add_hidden(const std::shared_ptr<vfs::file>& file) const noexcept
{
    const auto file_path = std::filesystem::path() / this->path_ / ".hidden";
    const std::string data = std::format("{}\n", file->name());

    return write_file(file_path, data);
}

void
vfs::dir::cancel_all_thumbnail_requests() noexcept
{
    this->thumbnailer = nullptr;
}

void
vfs::dir::load_thumbnail(const std::shared_ptr<vfs::file>& file, const bool is_big) noexcept
{
    vfs_thumbnail_request(this->shared_from_this(), file, is_big);
}

bool
vfs::dir::is_file_listed() const noexcept
{
    return this->file_listed;
}

bool
vfs::dir::is_directory_empty() const noexcept
{
    return this->file_list.empty();
}

bool
vfs::dir::update_file_info(const std::shared_ptr<vfs::file>& file) noexcept
{
    bool ret = false;

    const bool is_file_valid = file->update();
    if (is_file_valid)
    {
        ret = true;
    }
    else /* The file does not exist */
    {
        if (ztd::contains(this->file_list, file))
        {
            ztd::remove(this->file_list, file);
            if (file)
            {
                this->run_event<spacefm::signal::file_deleted>(file);
            }
        }
        ret = false;
    }

    return ret;
}

void
vfs::dir::update_changed_files() noexcept
{
    //std::scoped_lock<std::mutex> lock(this->mutex);

    if (this->changed_files.empty())
    {
        return;
    }

    for (const auto& file : this->changed_files)
    {
        if (this->update_file_info(file))
        {
            this->run_event<spacefm::signal::file_changed>(file);
        }
        // else was deleted, signaled, and unrefed in update_file_info
    }
    this->changed_files.clear();
}

void
vfs::dir::update_created_files() noexcept
{
    // std::scoped_lock<std::mutex> lock(this->mutex);

    if (this->created_files.empty())
    {
        return;
    }

    for (const auto& created_file : this->created_files)
    {
        const auto file_found = this->find_file(created_file, nullptr);
        if (!file_found)
        {
            // file is not in dir file_list
            const auto full_path = std::filesystem::path() / this->path_ / created_file;
            if (std::filesystem::exists(full_path))
            {
                const auto file = vfs::file::create(full_path);
                // add new file to dir file_list
                this->file_list.emplace_back(file);

                this->run_event<spacefm::signal::file_created>(file);
            }
            // else file does not exist in filesystem
        }
        else
        {
            // file already exists in dir file_list
            if (this->update_file_info(file_found))
            {
                this->run_event<spacefm::signal::file_changed>(file_found);
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
    }
    this->created_files.clear();
}

void
vfs::dir::unload_thumbnails(bool is_big) noexcept
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    for (const auto& file : this->file_list)
    {
        if (is_big)
        {
            file->unload_big_thumbnail();
        }
        else
        {
            file->unload_small_thumbnail();
        }
    }

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
    malloc_trim(0);
}

void
vfs::dir::reload_mime_type() noexcept
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    if (this->is_directory_empty())
    {
        return;
    }

    const auto reload_file_mime_action = [](const auto& file) { file->reload_mime_type(); };
    std::ranges::for_each(this->file_list, reload_file_mime_action);

    const auto signal_file_changed_action = [this](const auto& file)
    { this->run_event<spacefm::signal::file_changed>(file); };
    std::ranges::for_each(this->file_list, signal_file_changed_action);
}

/* signal handlers */
void
vfs::dir::emit_file_created(const std::filesystem::path& file_name, bool force) noexcept
{
    (void)force;
    // Ignore avoid_changes for creation of files
    // if ( !force && dir->avoid_changes )
    //    return;

    if (std::filesystem::equivalent(file_name, this->path_))
    { // Special Case: The directory itself was created?
        return;
    }

    this->created_files.emplace_back(file_name);

    this->update_changed_files();
    this->update_created_files();
}

void
vfs::dir::emit_file_deleted(const std::filesystem::path& file_name,
                            const std::shared_ptr<vfs::file>& file) noexcept
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    if (std::filesystem::equivalent(file_name, this->path_))
    {
        /* Special Case: The directory itself was deleted... */

        /* clear the whole list */
        this->file_list.clear();

        this->run_event<spacefm::signal::file_deleted>(nullptr);

        return;
    }

    const auto file_found = this->find_file(file_name, file);
    if (file_found)
    {
        if (!ztd::contains(this->changed_files, file_found))
        {
            this->changed_files.emplace_back(file_found);

            this->update_changed_files();
            this->update_created_files();
        }
    }
}

void
vfs::dir::emit_file_changed(const std::filesystem::path& file_name,
                            const std::shared_ptr<vfs::file>& file, bool force) noexcept
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    // ztd::logger::info("vfs_dir_emit_file_changed dir={} file_name={} avoid={}", dir->path,
    // file_name, this->avoid_changes ? "true" : "false");

    if (!force && this->avoid_changes)
    {
        return;
    }

    if (std::filesystem::equivalent(file_name, this->path_))
    {
        // Special Case: The directory itself was changed
        this->run_event<spacefm::signal::file_changed>(nullptr);
        return;
    }

    const auto file_found = this->find_file(file_name, file);
    if (file_found)
    {
        if (!ztd::contains(this->changed_files, file_found))
        {
            if (force)
            {
                this->changed_files.emplace_back(file_found);

                this->update_changed_files();
                this->update_created_files();
            }
            else if (this->update_file_info(file_found)) // update file info the first time
            {
                this->changed_files.emplace_back(file_found);

                this->update_changed_files();
                this->update_created_files();

                this->run_event<spacefm::signal::file_changed>(file_found);
            }
        }
    }
}

void
vfs::dir::emit_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    const auto file_found = this->find_file(file->name(), file);
    if (file_found)
    {
        assert(file == file_found);
        this->run_event<spacefm::signal::file_thumbnail_loaded>(file_found);
    }
}
