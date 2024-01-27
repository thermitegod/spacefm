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

#include <span>

#include <memory>

#include <gtkmm.h>

#include "xset/xset.hxx"

#include "ptk/ptk-file-browser.hxx"

#include "vfs/vfs-file.hxx"

/* sel_files is a list containing vfs::file structures
 * The list will be freed in this function, so the caller must not
 * free the list after calling this function.
 */

namespace ptk
{
struct file_menu
{
    enum class app_job
    {
        default_action,
        edit,
        edit_list,
        browse,
        browse_shared,
        edit_type,
        view,
        view_type,
        view_over,
        update,
        browse_mime,
        browse_mime_usr,
        usr
    };

    file_menu() = default;
    ~file_menu();

    ptk::browser* browser{nullptr};
    std::filesystem::path cwd{};
    std::filesystem::path file_path{};
    std::shared_ptr<vfs::file> file{nullptr};
    std::vector<std::shared_ptr<vfs::file>> sel_files;
#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group{nullptr};
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group{nullptr};
#endif
};
} // namespace ptk

struct AutoOpenCreate : public std::enable_shared_from_this<AutoOpenCreate>
{
    AutoOpenCreate(ptk::browser* file_browser, bool open_file);

    ptk::browser* file_browser{nullptr};
    bool open_file{false};
    std::filesystem::path path{};
    GFunc callback{nullptr};
};

GtkWidget* ptk_file_menu_new(ptk::browser* browser,
                             const std::span<const std::shared_ptr<vfs::file>> sel_files = {});

#if (GTK_MAJOR_VERSION == 4)
void ptk_file_menu_add_panel_view_menu(ptk::browser* browser, GtkWidget* menu,
                                       GtkEventController* accel_group);
#elif (GTK_MAJOR_VERSION == 3)
void ptk_file_menu_add_panel_view_menu(ptk::browser* browser, GtkWidget* menu,
                                       GtkAccelGroup* accel_group);
#endif

void on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, ptk::file_menu* data);

void ptk_file_menu_action(ptk::browser* browser, const xset_t& set);

void on_popup_sortby(GtkMenuItem* menuitem, ptk::browser* file_browser, i32 order);
void on_popup_list_detailed(GtkMenuItem* menuitem, ptk::browser* browser);
void on_popup_list_icons(GtkMenuItem* menuitem, ptk::browser* browser);
void on_popup_list_compact(GtkMenuItem* menuitem, ptk::browser* browser);
void on_popup_list_large(GtkMenuItem* menuitem, ptk::browser* browser);
