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

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-dir.hxx"

#define PTK_FILE_LIST(obj)             (static_cast<PtkFileList*>(obj))
#define PTK_FILE_LIST_REINTERPRET(obj) (reinterpret_cast<PtkFileList*>(obj))

namespace ptk::file_list
{
    enum class column
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

    // sort_dir of directory view - do not change order, saved
    // see also: ipc-command.cxx run_ipc_command() get sort_first
    enum class sort_dir
    {
        mixed,
        first,
        last
    };
} // namespace ptk::file_list

struct PtkFileList
{
    GObject parent;

    /* <private> */
    std::shared_ptr<vfs::dir> dir{nullptr};
    GList* files{nullptr};
    u32 n_files{0};

    bool show_hidden{true};
    bool big_thumbnail{true};
    u64 max_thumbnail{0};

    ptk::file_list::column sort_col{ptk::file_list::column::name};
    GtkSortType sort_order;
    bool sort_alphanum{false};
    bool sort_natural{false};
    bool sort_case{false};
    bool sort_hidden_first{false};
    ptk::file_list::sort_dir sort_dir{ptk::file_list::sort_dir::mixed};

    // Random integer to check whether an iter belongs to our model
    const i32 stamp{std::rand()};

  public:
    // signals
    void on_file_list_file_created(const std::shared_ptr<vfs::file>& file);
    void on_file_list_file_deleted(const std::shared_ptr<vfs::file>& file);
    void on_file_list_file_changed(const std::shared_ptr<vfs::file>& file);
    void on_file_list_file_thumbnail_loaded(const std::shared_ptr<vfs::file>& file);

  public:
    // Signals we connect to
    sigc::connection signal_file_created;
    sigc::connection signal_file_deleted;
    sigc::connection signal_file_changed;
    sigc::connection signal_file_thumbnail_loaded;
};

struct PtkFileListClass
{
    GObjectClass parent;
    /* Default signal handlers */
    // void (*file_created)(const std::shared_ptr<vfs::dir>& dir, const char* file_name);
    // void (*file_deleted)(const std::shared_ptr<vfs::dir>& dir, const char* file_name);
    // void (*file_changed)(const std::shared_ptr<vfs::dir>& dir, const char* file_name);
    // void (*load_complete)(const std::shared_ptr<vfs::dir>& dir);
};

GType ptk_file_list_get_type();

PtkFileList* ptk_file_list_new(const std::shared_ptr<vfs::dir>& dir, bool show_hidden);

void ptk_file_list_set_dir(PtkFileList* list, const std::shared_ptr<vfs::dir>& dir);

bool ptk_file_list_find_iter(PtkFileList* list, GtkTreeIter* it,
                             const std::shared_ptr<vfs::file>& file);

void ptk_file_list_show_thumbnails(PtkFileList* list, bool is_big, u64 max_file_size);
void ptk_file_list_sort(PtkFileList* list); // sfm
