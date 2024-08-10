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

// #include "logger.hxx"

#include "concurrency.hxx"

#include "vfs/vfs-file.hxx"
#include "vfs/vfs-monitor.hxx"
#include "vfs/vfs-thumbnailer.hxx"

#include "signals.hxx"

namespace vfs
{
struct dir : public std::enable_shared_from_this<dir>
{
    dir() = delete;
    dir(const std::filesystem::path& path) noexcept;
    ~dir() noexcept;
    dir(const dir& other) = delete;
    dir(dir&& other) = delete;
    dir& operator=(const dir& other) = delete;
    dir& operator=(dir&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<vfs::dir>
    create(const std::filesystem::path& path) noexcept;

    // unloads thumbnails in every vfs::dir
    static void global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::span<const std::shared_ptr<vfs::file>> files() const noexcept;

    void refresh() noexcept;

    [[nodiscard]] u64 hidden_files() const noexcept;

    [[nodiscard]] bool avoid_changes() const noexcept;
    void update_avoid_changes() noexcept;

    // The dir has finished loading
    [[nodiscard]] bool is_loaded() const noexcept;
    // The dir is still loading
    [[nodiscard]] bool is_loading() const noexcept;

    [[nodiscard]] bool is_directory_empty() const noexcept;

    [[nodiscard]] bool add_hidden(const std::shared_ptr<vfs::file>& file) const noexcept;

    void load_thumbnail(const std::shared_ptr<vfs::file>& file,
                        const vfs::file::thumbnail_size size) noexcept;
    void unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;
    void enable_thumbnails(const bool enabled) noexcept;

    /* emit signals */
    void emit_file_created(const std::filesystem::path& path, bool force) noexcept;
    void emit_file_deleted(const std::filesystem::path& path) noexcept;
    void emit_file_changed(const std::filesystem::path& path, bool force) noexcept;
    void emit_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept;

    // TODO private
    void update_created_files() noexcept;
    void update_changed_files() noexcept;
    u32 change_notify_timeout{0};

  private:
    // this function is to be called right after a vfs::dir is created in ::create().
    // this is because this* only becomes a valid pointer after the constructor has finished.
    void post_initialize() noexcept;

    concurrencpp::result<bool> load_thread() noexcept;
    concurrencpp::result<bool> refresh_thread() noexcept;

    void on_monitor_event(const vfs::monitor::event event,
                          const std::filesystem::path& path) noexcept;

    void notify_file_change(const std::chrono::milliseconds timeout) noexcept;

    [[nodiscard]] const std::shared_ptr<vfs::file>
    find_file(const std::filesystem::path& filename) noexcept;
    [[nodiscard]] bool update_file_info(const std::shared_ptr<vfs::file>& file) noexcept;

    // dir .hidden file
    void load_user_hidden_files() noexcept;
    [[nodiscard]] bool is_file_user_hidden(const std::filesystem::path& path) const noexcept;
    std::optional<std::vector<std::filesystem::path>> user_hidden_files_{std::nullopt};

    std::filesystem::path path_;
    std::vector<std::shared_ptr<vfs::file>> files_;

    vfs::thumbnailer thumbnailer_{
        std::bind(&vfs::dir::emit_thumbnail_loaded, this, std::placeholders::_1)};
    const vfs::monitor monitor_{
        this->path_,
        std::bind(&vfs::dir::on_monitor_event, this, std::placeholders::_1, std::placeholders::_2)};

    std::vector<std::shared_ptr<vfs::file>> changed_files_;
    std::vector<std::filesystem::path> created_files_;

    bool avoid_changes_{true}; // disable file events, for nfs mount locations.

    bool load_complete_{false};         // is dir loaded, initial load or refresh
    bool load_complete_initial_{false}; // is dir loaded, initial load only, blocks refresh

    bool running_refresh_{false}; // is a refresh currently being run.

    bool enable_thumbnails_{true};

    bool shutdown_{false}; // use to signal that we are being destructed

    u64 xhidden_count_{0};

    std::mutex files_lock_;
    std::mutex changed_files_lock_;
    std::mutex created_files_lock_;

    // Concurrency
    std::shared_ptr<concurrencpp::thread_executor> executor_;
    concurrencpp::result<concurrencpp::result<bool>> executor_result_;
    concurrencpp::async_lock lock_;

    // Signals //
  public:
    // Signals Add Event

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_created, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Connect   : spacefm::signal::file_created");
        return this->evt_file_created.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_changed, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Connect   : spacefm::signal::file_changed");
        return this->evt_file_changed.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_deleted, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Connect   : spacefm::signal::file_deleted");
        return this->evt_file_deleted.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_listed, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Connect   : spacefm::signal::file_listed");
        return this->evt_file_listed.connect(fun);
    }

    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::file_thumbnail_loaded, sigc::connection>
    add_event(bind_fun fun) noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Connect   : spacefm::signal::file_thumbnail_loaded");
        return this->evt_file_thumbnail_loaded.connect(fun);
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_created, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Execute   : spacefm::signal::file_created");
        this->evt_file_created.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_changed, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Execute   : spacefm::signal::file_changed");
        this->evt_file_changed.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_deleted, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Execute   : spacefm::signal::file_deleted");
        this->evt_file_deleted.emit(file);
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_listed, void>
    run_event() const noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Execute   : spacefm::signal::file_listed");
        this->evt_file_listed.emit();
    }

    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::file_thumbnail_loaded, void>
    run_event(const std::shared_ptr<vfs::file>& file) const noexcept
    {
        // logger::trace<logger::domain::vfs>("Signal Execute   : spacefm::signal::file_thumbnail_loaded");
        this->evt_file_thumbnail_loaded.emit(file);
    }

  private:
    // Signal types
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_created;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_changed;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_deleted;
    sigc::signal<void()> evt_file_listed;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> evt_file_thumbnail_loaded;

  public:
    // private:
    // Signals we connect to
    sigc::connection signal_task_load_dir;
};
} // namespace vfs
