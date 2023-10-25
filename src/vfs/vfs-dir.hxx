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

#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-file-monitor.hxx"
#include "vfs/vfs-file-info.hxx"

#include "vfs/vfs-async-thread.hxx"

#include "signals.hxx"

// forward declare types
struct VFSFileInfo;
struct PtkFileBrowser;
struct PtkFileList;
struct VFSDir;
struct VFSThumbnailLoader;

namespace vfs
{
    using thumbnail_loader = std::shared_ptr<VFSThumbnailLoader>;

    struct dir : public std::enable_shared_from_this<dir>
    {
        // dir() = default;
        dir();
        ~dir();

        std::filesystem::path path{};
        std::vector<vfs::file_info> file_list{};

        /*<private>*/
        std::shared_ptr<vfs::file_monitor> monitor{nullptr};
        std::shared_ptr<vfs::async_thread> task{nullptr};

        bool file_listed{true};
        bool load_complete{true};
        bool cancel{true};
        bool show_hidden{true};
        bool avoid_changes{true};

        vfs::thumbnail_loader thumbnail_loader{nullptr};

        std::vector<vfs::file_info> changed_files{};
        std::vector<std::filesystem::path> created_files{};

        i64 xhidden_count{0};

      public:
        void load() noexcept;

        bool is_file_listed() const noexcept;
        bool is_directory_empty() const noexcept;

        void update_created_files() noexcept;
        void update_changed_files() noexcept;

        void unload_thumbnails(bool is_big) noexcept;

        bool add_hidden(const vfs::file_info& file) const noexcept;

        void cancel_all_thumbnail_requests() noexcept;
        void load_thumbnail(const vfs::file_info& file, const bool is_big) noexcept;

        void reload_mime_type() noexcept;

        const std::optional<std::vector<std::filesystem::path>> get_hidden_files() const noexcept;

        /* emit signals */
        void emit_file_created(const std::filesystem::path& file_name, bool force) noexcept;
        void emit_file_deleted(const std::filesystem::path& file_name,
                               const vfs::file_info& file) noexcept;
        void emit_file_changed(const std::filesystem::path& file_name, const vfs::file_info& file,
                               bool force) noexcept;
        void emit_thumbnail_loaded(const vfs::file_info& file) noexcept;

      private:
        vfs::file_info find_file(const std::filesystem::path& file_name,
                                 const vfs::file_info& file) const noexcept;
        bool update_file_info(const vfs::file_info& file) noexcept;

      private:
        std::mutex mutex;

        // Signals
      public:
        // Signals function types
        using evt_file_created__run_first__t = void(const vfs::file_info&, PtkFileBrowser*);
        using evt_file_created__run_last__t = void(const vfs::file_info&, PtkFileList*);

        using evt_file_changed__run_first__t = void(const vfs::file_info&, PtkFileBrowser*);
        using evt_file_changed__run_last__t = void(const vfs::file_info&, PtkFileList*);

        using evt_file_deleted__run_first__t = void(const vfs::file_info&, PtkFileBrowser*);
        using evt_file_deleted__run_last__t = void(const vfs::file_info&, PtkFileList*);

        using evt_file_listed_t = void(PtkFileBrowser*, bool);

        using evt_file_thumbnail_loaded_t = void(const vfs::file_info&, PtkFileList*);

        using evt_mime_change_t = void();

        // Signals Add Event
        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_created, sigc::connection>::type
        add_event(evt_file_created__run_first__t fun, PtkFileBrowser* browser) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_created");
            this->evt_data_browser = browser;
            return this->evt_file_created__first.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_created, sigc::connection>::type
        add_event(evt_file_created__run_last__t fun, PtkFileList* list) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_created");
            this->evt_data_list = list;
            return this->evt_file_created__last.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_created, sigc::connection>::type
        add_event(evt_mime_change_t fun) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_created");
            return this->evt_mime_change.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_changed, sigc::connection>::type
        add_event(evt_file_changed__run_first__t fun, PtkFileBrowser* browser) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_changed");
            this->evt_data_browser = browser;
            return this->evt_file_changed__first.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_changed, sigc::connection>::type
        add_event(evt_file_changed__run_last__t fun, PtkFileList* list) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_changed");
            this->evt_data_list = list;
            return this->evt_file_changed__last.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_changed, sigc::connection>::type
        add_event(evt_mime_change_t fun) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_changed");
            return this->evt_mime_change.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_deleted, sigc::connection>::type
        add_event(evt_file_deleted__run_first__t fun, PtkFileBrowser* browser) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_deleted");
            this->evt_data_browser = browser;
            return this->evt_file_deleted__first.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_deleted, sigc::connection>::type
        add_event(evt_file_deleted__run_last__t fun, PtkFileList* list) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_deleted");
            this->evt_data_list = list;
            return this->evt_file_deleted__last.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_deleted, sigc::connection>::type
        add_event(evt_mime_change_t fun) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_deleted");
            return this->evt_mime_change.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_listed, sigc::connection>::type
        add_event(evt_file_listed_t fun, PtkFileBrowser* browser) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_listed");
            // this->evt_data_listed_browser = browser;
            this->evt_data_browser = browser;
            return this->evt_file_listed.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_listed, sigc::connection>::type
        add_event(evt_mime_change_t fun) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_listed");
            return this->evt_mime_change.connect(sigc::ptr_fun(fun));
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_thumbnail_loaded,
                                sigc::connection>::type
        add_event(evt_file_thumbnail_loaded_t fun, PtkFileList* list) noexcept
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::file_thumbnail_loaded");
            // this->evt_data_thumb_list = list;
            this->evt_data_list = list;
            return this->evt_file_thumbnail_loaded.connect(sigc::ptr_fun(fun));
        }

        // Signals Run Event
        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_created, void>::type
        run_event(const vfs::file_info& file) const noexcept
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::file_created");
            this->evt_mime_change.emit();
            this->evt_file_created__first.emit(file, this->evt_data_browser);
            this->evt_file_created__last.emit(file, this->evt_data_list);
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_changed, void>::type
        run_event(const vfs::file_info& file) const noexcept
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::file_changed");
            this->evt_mime_change.emit();
            this->evt_file_changed__first.emit(file, this->evt_data_browser);
            this->evt_file_changed__last.emit(file, this->evt_data_list);
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_deleted, void>::type
        run_event(const vfs::file_info& file) const noexcept
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::file_deleted");
            this->evt_mime_change.emit();
            this->evt_file_deleted__first.emit(file, this->evt_data_browser);
            this->evt_file_deleted__last.emit(file, this->evt_data_list);
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_listed, void>::type
        run_event(bool is_cancelled) const noexcept
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::file_listed");
            this->evt_mime_change.emit();
            this->evt_file_listed.emit(this->evt_data_browser, is_cancelled);
        }

        template<spacefm::signal evt>
        typename std::enable_if<evt == spacefm::signal::file_thumbnail_loaded, void>::type
        run_event(const vfs::file_info& file) const noexcept
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::file_thumbnail_loaded");
            this->evt_file_thumbnail_loaded.emit(file, this->evt_data_list);
        }

        // Signals
      private:
        // Signal types
        sigc::signal<evt_file_created__run_first__t> evt_file_created__first;
        sigc::signal<evt_file_created__run_last__t> evt_file_created__last;

        sigc::signal<evt_file_changed__run_first__t> evt_file_changed__first;
        sigc::signal<evt_file_changed__run_last__t> evt_file_changed__last;

        sigc::signal<evt_file_deleted__run_first__t> evt_file_deleted__first;
        sigc::signal<evt_file_deleted__run_last__t> evt_file_deleted__last;

        sigc::signal<evt_file_listed_t> evt_file_listed;

        sigc::signal<evt_file_thumbnail_loaded_t> evt_file_thumbnail_loaded;

        sigc::signal<evt_mime_change_t> evt_mime_change;

      private:
        // Signal data
        // TODO/FIXME has to be a better way to do this
        // PtkFileBrowser* evt_data_listed_browser{nullptr};
        PtkFileBrowser* evt_data_browser{nullptr};
        PtkFileList* evt_data_list{nullptr};
        // PtkFileList* evt_data_thumb_list{nullptr};

      public:
        // private:
        // Signals we connect to
        sigc::connection signal_task_load_dir;
    };
} // namespace vfs

const std::shared_ptr<vfs::dir> vfs_dir_get_by_path(const std::filesystem::path& path);

void vfs_dir_mime_type_reload();
