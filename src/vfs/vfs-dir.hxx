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

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

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

    // file change notify
    void update_created_files() noexcept;
    void update_changed_files() noexcept;
    bool timer_running_ = false;
    Glib::SignalTimeout timer_ = Glib::signal_timeout();

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

  public:
    // Signals

    template<spacefm::signal evt, typename bind_fun>
    sigc::connection
    add_event(bind_fun fun) noexcept
    {
        logger::trace<logger::domain::signals>("Connect({}): {}",
                                               logger::utils::ptr(this),
                                               magic_enum::enum_name(evt));

        if constexpr (evt == spacefm::signal::file_created)
        {
            return this->evt_file_created.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::file_changed)
        {
            return this->evt_file_changed.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::file_deleted)
        {
            return this->evt_file_deleted.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::file_listed)
        {
            return this->evt_file_listed.connect(fun);
        }
        else if constexpr (evt == spacefm::signal::file_thumbnail_loaded)
        {
            return this->evt_file_thumbnail_loaded.connect(fun);
        }
        else
        {
            static_assert(always_false<bind_fun>::value, "Unsupported signal type");
        }
    }

    template<spacefm::signal evt, typename... Args>
    void
    run_event(Args&&... args) noexcept
    {
        logger::trace<logger::domain::signals>("Execute({}): {}",
                                               logger::utils::ptr(this),
                                               magic_enum::enum_name(evt));

        if constexpr (evt == spacefm::signal::file_created)
        {
            static_assert(sizeof...(args) == 1 &&
                          (std::is_same_v<std::nullptr_t, std::decay_t<Args>...> ||
                           std::is_same_v<std::shared_ptr<vfs::file>, std::decay_t<Args>...>));
            this->evt_file_created.emit(std::forward<Args>(args)...);
        }
        else if constexpr (evt == spacefm::signal::file_changed)
        {
            static_assert(sizeof...(args) == 1 &&
                          (std::is_same_v<std::nullptr_t, std::decay_t<Args>...> ||
                           std::is_same_v<std::shared_ptr<vfs::file>, std::decay_t<Args>...>));
            this->evt_file_changed.emit(std::forward<Args>(args)...);
        }
        else if constexpr (evt == spacefm::signal::file_deleted)
        {
            static_assert(sizeof...(args) == 1 &&
                          (std::is_same_v<std::nullptr_t, std::decay_t<Args>...> ||
                           std::is_same_v<std::shared_ptr<vfs::file>, std::decay_t<Args>...>));
            this->evt_file_deleted.emit(std::forward<Args>(args)...);
        }
        else if constexpr (evt == spacefm::signal::file_listed)
        {
            static_assert(sizeof...(args) == 0);
            this->evt_file_listed.emit(std::forward<Args>(args)...);
        }
        else if constexpr (evt == spacefm::signal::file_thumbnail_loaded)
        {
            static_assert(sizeof...(args) == 1 &&
                          (std::is_same_v<std::nullptr_t, std::decay_t<Args>...> ||
                           std::is_same_v<std::shared_ptr<vfs::file>, std::decay_t<Args>...>));
            this->evt_file_thumbnail_loaded.emit(std::forward<Args>(args)...);
        }
        else
        {
            static_assert(always_false<decltype(evt)>::value,
                          "Unsupported signal type or incorrect number of arguments.");
        }
    }

  private:
    // Signals
    template<typename T> struct always_false : std::false_type
    {
    };

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
