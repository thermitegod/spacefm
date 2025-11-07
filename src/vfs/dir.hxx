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

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"
#include "vfs/settings.hxx"
#include "vfs/thumbnailer.hxx"

#include "concurrency.hxx"

namespace vfs
{
class dir final : public std::enable_shared_from_this<dir>
{
  public:
    dir() = delete;
    explicit dir(const std::filesystem::path& path,
                 const std::shared_ptr<vfs::settings>& settings) noexcept;
    ~dir() noexcept;
    dir(const dir& other) = delete;
    dir(dir&& other) = delete;
    dir& operator=(const dir& other) = delete;
    dir& operator=(dir&& other) = delete;

    [[nodiscard]] static std::shared_ptr<vfs::dir>
    create(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings,
           const bool permanent = false) noexcept;

    // unloads thumbnails in every vfs::dir
    static void global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::span<const std::shared_ptr<vfs::file>> files() const noexcept;

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

    void notify_file_change(const std::chrono::milliseconds timeout) noexcept;

    [[nodiscard]] std::shared_ptr<vfs::file>
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

    vfs::thumbnailer thumbnailer_;
    std::jthread thumbnailer_thread_;

    notify::notify_controller notifier_ = notify::inotify_controller();
    std::jthread notifier_thread_;

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

    std::shared_ptr<vfs::settings> settings_;

  public:
    // Signals

    [[nodiscard]] auto
    signal_file_changed()
    {
        return this->signal_file_changed_;
    }

    [[nodiscard]] auto
    signal_file_created()
    {
        return this->signal_file_created_;
    }

    [[nodiscard]] auto
    signal_file_deleted()
    {
        return this->signal_file_deleted_;
    }

    [[nodiscard]] auto
    signal_file_listed()
    {
        return this->signal_file_listed_;
    }

    [[nodiscard]] auto
    signal_thumbnail_loaded()
    {
        return this->signal_file_thumbnail_loaded_;
    }

  private:
    // Signals
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_file_created_;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_file_changed_;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_file_deleted_;
    sigc::signal<void()> signal_file_listed_;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_file_thumbnail_loaded_;
};
} // namespace vfs
