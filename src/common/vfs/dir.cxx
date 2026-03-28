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
#if (GTK_MAJOR_VERSION == 3)
#include "vfs/settings.hxx"
#endif
#include "vfs/thumbnailer.hxx"
#include "vfs/volume.hxx"

#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

namespace global
{
static ztd::smart_cache<std::filesystem::path, vfs::dir> dir_smart_cache;
}

#if (GTK_MAJOR_VERSION == 4)
vfs::dir::dir(const std::filesystem::path& path) noexcept
    : path_(path), notifier_(path_, {
                                        notify::event::create,
                                        notify::event::moved_to,
                                        notify::event::delete_self,
                                        notify::event::delete_sub,
                                        notify::event::moved_from,
                                        notify::event::umount,
                                        notify::event::modify,
                                        notify::event::attrib,
                                    })
#elif (GTK_MAJOR_VERSION == 3)
vfs::dir::dir(const std::filesystem::path& path,
              const std::shared_ptr<vfs::settings>& settings) noexcept
    : path_(path), settings_(settings), notifier_(path_, {
                                                             notify::event::create,
                                                             notify::event::moved_to,
                                                             notify::event::delete_self,
                                                             notify::event::delete_sub,
                                                             notify::event::moved_from,
                                                             notify::event::umount,
                                                             notify::event::modify,
                                                             notify::event::attrib,
                                                         })
#endif
{
    // logger::debug<logger::vfs>("vfs::dir::dir({})   {}", logger::utils::ptr(this), path_.string());

    // create events
    notifier_.signal_create().connect([this](const auto& p) { on_file_created(p); });
    notifier_.signal_moved_to().connect([this](const auto& p) { on_file_created(p); });
    // delete events
    notifier_.signal_delete().connect([this](const auto& p) { on_file_deleted(p); });
    notifier_.signal_moved_from().connect([this](const auto& p) { on_file_deleted(p); });
    // modify events
    notifier_.signal_modify().connect([this](const auto& p) { on_file_changed(p); });
    notifier_.signal_attrib().connect([this](const auto& p) { on_file_changed(p); });
    // self event
    notifier_.signal_delete_self().connect([this](const auto& p) { on_self_deleted(p); });
    notifier_.signal_umount().connect([this](const auto& p) { on_self_deleted(p); });

    notifier_thread_ =
        std::jthread([this](const std::stop_token& stoken) { notifier_.run(stoken); });
    pthread_setname_np(notifier_thread_.native_handle(), "notifier");

    thumbnailer_.signal_thumbnail_created().connect([this](auto a) { on_thumbnail_loaded(a); });

    thumbnailer_thread_ =
        std::jthread([this](const std::stop_token& stoken) { thumbnailer_.run(stoken); });
    pthread_setname_np(thumbnailer_thread_.native_handle(), "thumbnailer");

    update_avoid_changes();

    loader_thread_ = std::jthread([this](const std::stop_token& stoken) { load_thread(stoken); });
    pthread_setname_np(loader_thread_.native_handle(), "loader");
}

vfs::dir::~dir() noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::~dir({})  {}", logger::utils::ptr(this), path_.string());

    signal_files_created().clear();
    signal_files_changed().clear();
    signal_files_deleted().clear();

#if (GTK_MAJOR_VERSION == 4)
    signal_directory_loaded().clear();
    signal_directory_refresh().clear();
#elif (GTK_MAJOR_VERSION == 3)
    signal_file_listed().clear();
#endif
    signal_thumbnail_loaded().clear();
    signal_directory_deleted().clear();

    thumbnailer_thread_.request_stop();
    thumbnailer_thread_.join();

    notifier_thread_.request_stop();
    notifier_thread_.join();

    loader_thread_.request_stop();
    loader_thread_.join();
}

#if (GTK_MAJOR_VERSION == 4)
std::shared_ptr<vfs::dir>
vfs::dir::create(const std::filesystem::path& path, const bool permanent) noexcept
#elif (GTK_MAJOR_VERSION == 3)
std::shared_ptr<vfs::dir>
vfs::dir::create(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings,
                 const bool permanent) noexcept
#endif
{
    std::shared_ptr<vfs::dir> dir = nullptr;
    if (global::dir_smart_cache.contains(path))
    {
        dir = global::dir_smart_cache.at(path);
        // logger::debug<logger::vfs>("vfs::dir::dir({}) cache   {}", logger::utils::ptr(dir.get()), path_.string());
    }
    else
    {
#if (GTK_MAJOR_VERSION == 4)
        struct hack : public vfs::dir
        {
            hack(const std::filesystem::path& path) : dir(path) {}
        };

        dir = global::dir_smart_cache.create(
            path,
            [&path]() { return std::make_shared<hack>(path); },
            permanent);
#elif (GTK_MAJOR_VERSION == 3)
        struct hack : public vfs::dir
        {
            hack(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings)
                : dir(path, settings)
            {
            }
        };

        dir = global::dir_smart_cache.create(
            path,
            [&path, &settings]() { return std::make_shared<hack>(path, settings); },
            permanent);
#endif
        // logger::debug<logger::vfs>("vfs::dir::dir({}) new     {}", logger::utils::ptr(dir.get()), path_.string());
    }
    // logger::debug<logger::vfs>("dir({})     {}", logger::utils::ptr(dir.get()), path.string());
    return dir;
}

const std::filesystem::path&
vfs::dir::path() const noexcept
{
    return path_;
}

std::span<const std::shared_ptr<vfs::file>>
vfs::dir::files() const noexcept
{
    return files_;
}

bool
vfs::dir::avoid_changes() const noexcept
{
    return avoid_changes_;
}

u64
vfs::dir::hidden_files() const noexcept
{
    return xhidden_count_;
}

void
vfs::dir::update_avoid_changes() noexcept
{
#if (GTK_MAJOR_VERSION == 4) // TODO
    avoid_changes_ = false;
#elif (GTK_MAJOR_VERSION == 3)
    avoid_changes_ = vfs::volume_dir_avoid_changes(path_);
#endif
}

void
vfs::dir::load_user_hidden_files() noexcept
{
    const auto hidden_path = path_ / ".hidden";

    if (!std::filesystem::is_regular_file(hidden_path))
    {
        user_hidden_files_ = std::nullopt;
        return;
    }

    const auto buffer = vfs::utils::read_file(hidden_path);
    if (!buffer)
    {
        logger::error<logger::vfs>("Failed to read .hidden file: {} {}",
                                   hidden_path.string(),
                                   buffer.error().message());
        user_hidden_files_ = std::nullopt;
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

    user_hidden_files_ = hidden;
}

bool
vfs::dir::is_file_user_hidden(const std::filesystem::path& path) const noexcept
{
    if (!user_hidden_files_)
    {
        return false;
    }

    const auto filename = path.filename();

    return std::ranges::any_of(*user_hidden_files_,
                               [&filename](const auto& hide_filename)
                               { return filename == hide_filename; });
}

void
vfs::dir::load_thread(const std::stop_token& stoken) noexcept
{
    // logger::debug<logger::vfs>("vfs::dir::load_thread({})   {}", logger::utils::ptr(this), path_.string());

    std::scoped_lock lock(loader_mutex_);

    load_running_ = true;
    xhidden_count_ = 0;

    // load this dirs .hidden file
    load_user_hidden_files();

    files_.reserve(1024);

    for (const auto& dfile : std::filesystem::directory_iterator(path_))
    {
        if (stoken.stop_requested())
        {
            return;
        }

        if (is_file_user_hidden(dfile.path()))
        {
            xhidden_count_ += 1;
            continue;
        }

        {
            std::scoped_lock files_lock(files_lock_);
#if (GTK_MAJOR_VERSION == 4)
            files_.push_back(vfs::file::create(dfile.path()));
#elif (GTK_MAJOR_VERSION == 3)
            files_.push_back(vfs::file::create(dfile.path(), settings_));
#endif
        }
    }

    load_running_ = false;

#if (GTK_MAJOR_VERSION == 4)
    signal_directory_loaded().emit();
#elif (GTK_MAJOR_VERSION == 3)
    signal_file_listed().emit();
#endif
}

void
vfs::dir::refresh() noexcept
{
    if (!load_running_)
    {
        loader_thread_.request_stop();
        loader_thread_.join();

        loader_thread_ =
            std::jthread([this](const std::stop_token& stoken) { refresh_thread(stoken); });
        pthread_setname_np(loader_thread_.native_handle(), "loader");
    }
}

void
vfs::dir::refresh_thread(const std::stop_token& stoken) noexcept
{
    std::scoped_lock lock(loader_mutex_);

    load_running_ = true;
    xhidden_count_ = 0;

    // reload this dirs .hidden file
    load_user_hidden_files();

    for (const auto& dfile : std::filesystem::directory_iterator(path_))
    {
        if (stoken.stop_requested())
        {
            break;
        }

        // Check if new files are hidden
        if (is_file_user_hidden(dfile.path()))
        {
            xhidden_count_ += 1;
            continue;
        }

        const auto filename = dfile.path().filename();
        if (find_file(filename) == nullptr)
        {
            on_file_created(filename);
        }
    }

    {
        std::scoped_lock files_lock(files_lock_);
        for (const auto& file : files_)
        {
            if (stoken.stop_requested())
            {
                break;
            }

            // Check if existing files have been hidden
            if (is_file_user_hidden(file->path()))
            {
                // Use the delete signal to properly remove this file from the file list.
                on_file_deleted(file->name());

                xhidden_count_ += 1;
                continue;
            }

#if (GTK_MAJOR_VERSION == 3)
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
#endif
        }
    }

    load_running_ = false;

#if (GTK_MAJOR_VERSION == 4)
    signal_directory_refresh().emit();
#elif (GTK_MAJOR_VERSION == 3)
    signal_file_listed().emit();
#endif
}

#if (GTK_MAJOR_VERSION == 3)
void
vfs::dir::global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    std::ranges::for_each(global::dir_smart_cache.items(),
                          [size](const auto& dir) { dir->unload_thumbnails(size); });
}
#endif

std::shared_ptr<vfs::file>
vfs::dir::find_file(const std::filesystem::path& filename) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    const auto it = std::ranges::find_if(files_,
                                         [&filename](const auto& file)
                                         { return file->name() == filename.string(); });
    if (it != files_.cend())
    {
        return *it;
    }
    return nullptr;
}

bool
vfs::dir::add_hidden(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!user_hidden_files_)
    {
        user_hidden_files_ = {};
    }

    user_hidden_files_->push_back(file->path());

    return write_hidden();
}

bool
vfs::dir::add_hidden(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    if (!user_hidden_files_)
    {
        user_hidden_files_ = {};
    }

    for (const auto& file : files)
    {
        user_hidden_files_->push_back(file->path());
    }

    return write_hidden();
}

bool
vfs::dir::write_hidden() const noexcept
{
    std::string text;
    for (const auto& path : *user_hidden_files_)
    {
        text += path.string();
        text += '\n';
    }

    return vfs::utils::write_file(path_ / ".hidden", text) == vfs::error_code::none;
}

void
vfs::dir::enable_thumbnails(const bool enabled) noexcept
{
    enable_thumbnails_ = enabled;
}

#if (GTK_MAJOR_VERSION == 4)

void
vfs::dir::load_thumbnails(const std::int32_t size) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    if (!enable_thumbnails_)
    {
        return;
    }

    for (const auto& file : files_)
    {
        load_thumbnail(file, size);
    }
}

void
vfs::dir::load_thumbnail(const std::shared_ptr<vfs::file>& file, const std::int32_t size) noexcept
{
    if (enable_thumbnails_)
    {
        // logger::debug<logger::vfs>("vfs::dir::load_thumbnail()  {}", file->name());
        thumbnailer_.request({file, size});
    }
}

void
vfs::dir::unload_thumbnails(const std::int32_t size) noexcept
{
    (void)size;
#if 0 // TODO
    std::scoped_lock files_lock(files_lock_);

    for (const auto& file : files_)
    {
        file->unload_thumbnail(size);
    }
#endif
}

#elif (GTK_MAJOR_VERSION == 3)

void
vfs::dir::load_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    if (!enable_thumbnails_)
    {
        return;
    }

    for (const auto& file : files_)
    {
        load_thumbnail(file, size);
    }
}

void
vfs::dir::load_thumbnail(const std::shared_ptr<vfs::file>& file,
                         const vfs::file::thumbnail_size size) noexcept
{
    if (enable_thumbnails_)
    {
        // logger::debug<logger::vfs>("vfs::dir::load_thumbnail()  {}", file->name());
        thumbnailer_.request({file, size});
    }
}

void
vfs::dir::unload_thumbnails(const vfs::file::thumbnail_size size) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    for (const auto& file : files_)
    {
        file->unload_thumbnail(size);
    }
}

#endif

bool
vfs::dir::is_loading() const noexcept
{
    return load_running_;
}

bool
vfs::dir::is_loaded() const noexcept
{
    return !load_running_;
}

bool
vfs::dir::is_directory_empty() const noexcept
{
    return files_.empty();
}

bool
vfs::dir::update_file(const std::shared_ptr<vfs::file>& file) noexcept
{
    const bool updated = file->update();
    if (!updated)
    { /* The file does not exist */
        if (std::ranges::contains(files_, file))
        {
            remove_file(file);

            events_.deleted.push_back(file);
        }
    }
    return updated;
}

void
vfs::dir::remove_file(const std::shared_ptr<vfs::file>& file) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    files_.erase(std::ranges::remove(files_, file).begin(), files_.end());
}

void
vfs::dir::notify_file_change(const std::chrono::milliseconds timeout) noexcept
{
    if (!timer_running_)
    {
        timer_running_ = true;

        timer_.connect_once(
            [this]()
            {
                {
                    std::scoped_lock lock(events_.deleted_lock);
                    update_deleted_files();
                    events_.deleted.clear();
                }

                {
                    std::scoped_lock lock(events_.changed_lock);
                    update_changed_files();
                    events_.changed.clear();
                }

                {
                    std::scoped_lock lock(events_.created_lock);
                    update_created_files();
                    events_.created.clear();
                }

                timer_running_ = false;
            },
            static_cast<std::uint32_t>(timeout.count()),
            Glib::PRIORITY_LOW);
    }
}

void
vfs::dir::update_deleted_files() noexcept
{
    if (events_.deleted.empty())
    {
        return;
    }

    for (const auto& file : events_.deleted)
    {
        remove_file(file);
    }

    signal_files_deleted().emit(events_.deleted);
}

void
vfs::dir::update_changed_files() noexcept
{
    if (events_.changed.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<vfs::file>> changed_files;
    for (const auto& file : events_.changed)
    {
        if (update_file(file))
        {
            changed_files.push_back(file);
        }
    }
    signal_files_changed().emit(changed_files);
}

void
vfs::dir::update_created_files() noexcept
{
    if (events_.created.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<vfs::file>> created_files;
    for (const auto& created_file : events_.created)
    {
        const auto file = find_file(created_file);
        if (!file)
        {
            // file is not in files_
            const auto file_path = path_ / created_file;
            if (std::filesystem::exists(file_path))
            {
                if (is_file_user_hidden(created_file))
                {
                    xhidden_count_ += 1;
                    continue;
                }

                std::scoped_lock files_lock(files_lock_);

#if (GTK_MAJOR_VERSION == 4)
                const auto new_file = vfs::file::create(file_path);
#elif (GTK_MAJOR_VERSION == 3)
                const auto new_file = vfs::file::create(file_path, settings_);
#endif
                files_.push_back(new_file);

                created_files.push_back(new_file);
            }
        }
        else
        {
            // file already exists in files_
            if (update_file(file))
            {
                created_files.push_back(file);
            }
        }
    }

    signal_files_created().emit(created_files);
}

void
vfs::dir::on_file_created(const std::filesystem::path& path) noexcept
{
    if (avoid_changes_)
    {
        return;
    }

    {
        std::scoped_lock lock(events_.created_lock);
        events_.created.push_back(path.filename());
    }

    notify_file_change(std::chrono::milliseconds(200));
}

void
vfs::dir::on_file_deleted(const std::filesystem::path& path) noexcept
{
    if (avoid_changes_)
    {
        return;
    }

    const auto file = find_file(path.filename());
    if (file)
    {
        std::scoped_lock lock(events_.deleted_lock);
        if (!std::ranges::contains(events_.deleted, file))
        {
            events_.deleted.push_back(file);
        }
    }

    notify_file_change(std::chrono::milliseconds(200));
}

void
vfs::dir::on_file_changed(const std::filesystem::path& path) noexcept
{
    if (avoid_changes_)
    {
        return;
    }

    if (path_ == path)
    { // Special Case: The directory itself was changed
        return;
    }

    const auto file = find_file(path.filename());
    if (file)
    {
        std::scoped_lock lock(events_.changed_lock);
        if (!std::ranges::contains(events_.changed, file))
        {
            if (update_file(file)) // update file info the first time
            {
                events_.changed.push_back(file);
            }
        }
    }

    notify_file_change(std::chrono::milliseconds(500));
}

void
vfs::dir::on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    std::scoped_lock files_lock(files_lock_);

    if (std::ranges::contains(files_, file))
    {
        signal_thumbnail_loaded().emit(file);
    }
}

void
vfs::dir::on_self_deleted(const std::filesystem::path& path) noexcept
{
    if (path_ == path)
    {
        signal_directory_deleted().emit();
    }
}
