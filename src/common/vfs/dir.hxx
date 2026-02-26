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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/notify-cpp/controller.hxx"
#if (GTK_MAJOR_VERSION == 3)
#include "vfs/settings.hxx"
#endif
#include "vfs/thumbnailer.hxx"

namespace vfs
{
class dir final : public std::enable_shared_from_this<dir>
{
  public:
    dir() = delete;
#if (GTK_MAJOR_VERSION == 4)
    explicit dir(const std::filesystem::path& path) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    explicit dir(const std::filesystem::path& path,
                 const std::shared_ptr<vfs::settings>& settings) noexcept;
#endif
    ~dir() noexcept;
    dir(const dir& other) = delete;
    dir(dir&& other) = delete;
    dir& operator=(const dir& other) = delete;
    dir& operator=(dir&& other) = delete;

#if (GTK_MAJOR_VERSION == 4)
    [[nodiscard]] static std::shared_ptr<vfs::dir> create(const std::filesystem::path& path,
                                                          const bool permanent = false) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    [[nodiscard]] static std::shared_ptr<vfs::dir>
    create(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings,
           const bool permanent = false) noexcept;
#endif

#if (GTK_MAJOR_VERSION == 4)
// TODO
#elif (GTK_MAJOR_VERSION == 3)
    // unloads thumbnails in every vfs::dir
    static void global_unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;
#endif

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::span<const std::shared_ptr<vfs::file>> files() const noexcept;

    void refresh() noexcept;

    [[nodiscard]] u64 hidden_files() const noexcept;

    [[nodiscard]] bool avoid_changes() const noexcept;
    void update_avoid_changes() noexcept;

    [[nodiscard]] bool is_loaded() const noexcept;
    [[nodiscard]] bool is_loading() const noexcept;

    [[nodiscard]] bool is_directory_empty() const noexcept;

    [[nodiscard]] bool add_hidden(const std::shared_ptr<vfs::file>& file) noexcept;
    [[nodiscard]] bool add_hidden(const std::span<const std::shared_ptr<vfs::file>> files) noexcept;

#if (GTK_MAJOR_VERSION == 4)
    void load_thumbnails(const std::int32_t size) noexcept;
    void load_thumbnail(const std::shared_ptr<vfs::file>& file, const std::int32_t size) noexcept;
    void unload_thumbnails(const std::int32_t size) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    void load_thumbnails(const vfs::file::thumbnail_size size) noexcept;
    void load_thumbnail(const std::shared_ptr<vfs::file>& file,
                        const vfs::file::thumbnail_size size) noexcept;
    void unload_thumbnails(const vfs::file::thumbnail_size size) noexcept;
#endif
    void enable_thumbnails(const bool enabled) noexcept;

  private:
    void load_thread(const std::stop_token& stoken) noexcept;
    void refresh_thread(const std::stop_token& stoken) noexcept;

    [[nodiscard]] std::shared_ptr<vfs::file>
    find_file(const std::filesystem::path& filename) noexcept;
    [[nodiscard]] bool update_file(const std::shared_ptr<vfs::file>& file) noexcept;
    void remove_file(const std::shared_ptr<vfs::file>& file) noexcept;

    // dir .hidden file
    [[nodiscard]] bool write_hidden() const noexcept;
    void load_user_hidden_files() noexcept;
    [[nodiscard]] bool is_file_user_hidden(const std::filesystem::path& path) const noexcept;
    std::optional<std::vector<std::filesystem::path>> user_hidden_files_{std::nullopt};

    // handle file events
    void on_file_created(const std::filesystem::path& path) noexcept;
    void on_file_deleted(const std::filesystem::path& path) noexcept;
    void on_file_changed(const std::filesystem::path& path) noexcept;
    void on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept;

    std::filesystem::path path_;

    std::vector<std::shared_ptr<vfs::file>> files_;
    std::mutex files_lock_;

    std::jthread loader_thread_;
    std::mutex loader_mutex_;

    vfs::thumbnailer thumbnailer_;
    std::jthread thumbnailer_thread_;

    notify::controller notifier_;
    std::jthread notifier_thread_;

    bool enable_thumbnails_{true};
    bool avoid_changes_{true};            // disable file events, for nfs mount locations.
    std::atomic_bool load_running_{true}; // is dir loaded, initial load or refresh
    u64 xhidden_count_;                   // filenames starting with '.' and user hidden files

    // batch handling for file events
    void notify_file_change(const std::chrono::milliseconds timeout) noexcept;
    // file change notify
    void update_created_files() noexcept;
    void update_changed_files() noexcept;
    void update_deleted_files() noexcept;
    bool timer_running_ = false;
    Glib::SignalTimeout timer_ = Glib::signal_timeout();

    struct file_events
    {
        std::mutex deleted_lock;
        std::vector<std::shared_ptr<vfs::file>> deleted;

        std::mutex changed_lock;
        std::vector<std::shared_ptr<vfs::file>> changed;

        std::mutex created_lock;
        std::vector<std::filesystem::path> created; // filenames only
    };
    file_events events_;

#if (GTK_MAJOR_VERSION == 3)
    std::shared_ptr<vfs::settings> settings_;
#endif

  public:
    // Signals

    [[nodiscard]] auto
    signal_files_changed() noexcept
    {
        return this->signal_files_changed_;
    }

    [[nodiscard]] auto
    signal_files_created() noexcept
    {
        return this->signal_files_created_;
    }

    [[nodiscard]] auto
    signal_files_deleted() noexcept
    {
        return this->signal_files_deleted_;
    }

    [[nodiscard]] auto
    signal_file_listed() noexcept
    {
        return this->signal_file_listed_;
    }

    [[nodiscard]] auto
    signal_thumbnail_loaded() noexcept
    {
        return this->signal_file_thumbnail_loaded_;
    }

    /**
     * The directory this vfs::dir was created for has been deleted
     */
    [[nodiscard]] auto
    signal_directory_deleted() noexcept
    {
        return this->signal_directory_deleted_;
    }

  private:
    // Signals
    sigc::signal<void(std::vector<std::shared_ptr<vfs::file>>)> signal_files_created_;
    sigc::signal<void(std::vector<std::shared_ptr<vfs::file>>)> signal_files_changed_;
    sigc::signal<void(std::vector<std::shared_ptr<vfs::file>>)> signal_files_deleted_;
    sigc::signal<void()> signal_file_listed_;
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_file_thumbnail_loaded_;
    sigc::signal<void()> signal_directory_deleted_;
};
} // namespace vfs
