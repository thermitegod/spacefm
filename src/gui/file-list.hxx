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

#include <memory>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/dir.hxx"
#include "vfs/file.hxx"

#define PTK_FILE_LIST(obj) (static_cast<gui::file_list*>(obj))
#define PTK_FILE_LIST_REINTERPRET(obj) (reinterpret_cast<gui::file_list*>(obj))

namespace gui
{
struct file_list
{
    enum class column : std::uint8_t
    { // Columns of directory view
        big_icon,
        small_icon,
        name,
        size,
        bytes,
        type,
        mime,
        perm,
        owner,
        group,
        atime,
        btime,
        ctime,
        mtime,
        info,
    };

    enum class sort_dir : std::uint8_t
    { // sort_dir of directory view - do not change order, saved in config
        mixed,
        first,
        last
    };

    [[nodiscard]] static gui::file_list* create(const std::shared_ptr<vfs::dir>& dir,
                                                const bool show_hidden,
                                                const std::string_view pattern) noexcept;

    GObject parent;

    /* <private> */
    std::shared_ptr<vfs::dir> dir{nullptr};
    GList* files{nullptr};

    bool show_hidden{true};
    // GObjects do not work with std::string
    const char* pattern;

    vfs::file::thumbnail_size thumbnail_size{vfs::file::thumbnail_size::big};
    u64 max_thumbnail{0};

    gui::file_list::column sort_col{gui::file_list::column::name};
    GtkSortType sort_order;
    bool sort_natural{false};
    bool sort_case{false};
    bool sort_hidden_first{false};
    gui::file_list::sort_dir sort_dir_{gui::file_list::sort_dir::mixed};

    // Random integer to check whether an iter belongs to our model
    std::int32_t stamp{0};

  public:
    void set_dir(const std::shared_ptr<vfs::dir>& new_dir) noexcept;
    void show_thumbnails(const vfs::file::thumbnail_size size, u64 max_file_size) noexcept;
    void sort() noexcept;

    [[nodiscard]] bool is_pattern_match(const std::filesystem::path& filename) const noexcept;

  private:
    void file_created(const std::shared_ptr<vfs::file>& file) noexcept;
    void file_changed(const std::shared_ptr<vfs::file>& file) noexcept;

  public:
    // signals
    void on_file_list_file_created(const std::shared_ptr<vfs::file>& file) noexcept;
    void on_file_list_file_deleted(const std::shared_ptr<vfs::file>& file) noexcept;
    void on_file_list_file_changed(const std::shared_ptr<vfs::file>& file) noexcept;
    void on_file_list_file_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept;

    // Signals we connect to
    sigc::connection signal_file_created;
    sigc::connection signal_file_deleted;
    sigc::connection signal_file_changed;
    sigc::connection signal_file_thumbnail_loaded;
};
} // namespace gui
