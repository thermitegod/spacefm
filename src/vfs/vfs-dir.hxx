/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <filesystem>

#include <vector>

#include <mutex>

#include <memory>

#include <chrono>

#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-file.hxx"
#include "vfs/vfs-monitor.hxx"

#include "signals.hxx"

namespace vfs
{
struct async_thread;
struct thumbnailer;

struct dir : public std::enable_shared_from_this<dir>
{
    dir() = delete;
    dir(const std::filesystem::path& path);
    ~dir();

    static const std::shared_ptr<vfs::dir> create(const std::filesystem::path& path) noexcept;

    // unloads thumbnails in every vfs::dir
    static void global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;
    // reload mime types in every vfs::dir
    static void global_reload_mime_type() noexcept;

    const std::filesystem::path& path() const noexcept;
    const std::span<const std::shared_ptr<vfs::file>> files() const noexcept;

    void refresh() noexcept;

    u64 hidden_files() const noexcept;

    bool avoid_changes() const noexcept;
    void update_avoid_changes() noexcept;

    bool is_loaded() const noexcept;
    bool is_file_listed() const noexcept;
    bool is_directory_empty() const noexcept;

    bool add_hidden(const std::shared_ptr<vfs::file>& file) const noexcept;

    void load_thumbnail(const std::shared_ptr<vfs::file>& file,
                        const vfs::file::thumbnail_size size) noexcept;
    void unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;
    void cancel_all_thumbnail_requests() noexcept;

    void reload_mime_type() noexcept;

    /* emit signals */
    void emit_file_created(const std::filesystem::path& filename, bool force) noexcept;
    void emit_file_deleted(const std::filesystem::path& filename) noexcept;
    void emit_file_changed(const std::filesystem::path& filename, bool force) noexcept;
    void emit_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept;

    // TODO private
    void update_created_files() noexcept;
    void update_changed_files() noexcept;
    std::shared_ptr<vfs::thumbnailer> thumbnailer{nullptr};
    u32 change_notify_timeout{0};

  private:
    void load_thread();

    void on_monitor_event(const vfs::monitor::event event, const std::filesystem::path& path);

    void notify_file_change(const std::chrono::milliseconds timeout) noexcept;

    // signal callback
    void on_list_task_finished(bool is_cancelled);

    const std::shared_ptr<vfs::file>
    find_file(const std::filesystem::path& filename) const noexcept;
    bool update_file_info(const std::shared_ptr<vfs::file>& file) noexcept;

    // dir .hidden file
    void load_user_hidden_files() noexcept;
    bool is_file_user_hidden(const std::filesystem::path& path) const noexcept;
    std::optional<std::vector<std::filesystem::path>> user_hidden_files_{std::nullopt};

    std::filesystem::path path_{};
    std::vector<std::shared_ptr<vfs::file>> files_{};

    std::shared_ptr<vfs::monitor> monitor_{nullptr};
    std::shared_ptr<vfs::async_thread> task_{nullptr};

    std::vector<std::shared_ptr<vfs::file>> changed_files_{};
    std::vector<std::filesystem::path> created_files_{};

    bool file_listed_{true};
    bool load_complete_{true};
    // bool cancel_{true};
    // bool show_hidden_{true};
    bool avoid_changes_{true};

    u64 xhidden_count_{0};

    std::mutex lock_;

    // Signals //
  public:
    // Signals Add Event

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_created, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::task_finish");
        return this->evt_file_created.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_changed, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::task_finish");
        return this->evt_file_changed.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_deleted, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::task_finish");
        return this->evt_file_deleted.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_listed, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::file_listed");
        return this->evt_file_listed.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_thumbnail_loaded, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::file_thumbnail_loaded");
        return this->evt_file_thumbnail_loaded.connect(fun);
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_created, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::file_created");
        this->evt_file_created.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_changed, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::file_changed");
        this->evt_file_changed.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_deleted, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::file_deleted");
        this->evt_file_deleted.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_listed, void>
    run_event(bool is_cancelled) const noexcept
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::file_listed");
        this->evt_file_listed.emit(is_cancelled);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_thumbnail_loaded, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::file_thumbnail_loaded");
        this->evt_file_thumbnail_loaded.emit(file);
    }

  private:
    // Signal types
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_created;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_changed;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_deleted;
    sigc::signal<void(bool)> evt_file_listed;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_thumbnail_loaded;

  public:
    // private:
    // Signals we connect to
    sigc::connection signal_task_load_dir;
};
} // namespace vfs
