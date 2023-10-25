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

#include "vfs/vfs-file-info.hxx"

/* sel_files is a list containing vfs::file_info structures
 * The list will be freed in this function, so the caller must not
 * free the list after calling this function.
 */

struct PtkFileMenu
{
    PtkFileMenu() = default;
    ~PtkFileMenu();

    PtkFileBrowser* browser{nullptr};
    std::filesystem::path cwd{};
    std::filesystem::path file_path{};
    std::shared_ptr<vfs::file_info> file{nullptr};
    std::vector<std::shared_ptr<vfs::file_info>> sel_files;
#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group{nullptr};
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group{nullptr};
#endif
};

struct AutoOpenCreate : public std::enable_shared_from_this<AutoOpenCreate>
{
    AutoOpenCreate(PtkFileBrowser* file_browser, bool open_file);

    PtkFileBrowser* file_browser{nullptr};
    bool open_file{false};
    std::filesystem::path path{};
    GFunc callback{nullptr};
};

GtkWidget* ptk_file_menu_new(PtkFileBrowser* browser);
GtkWidget* ptk_file_menu_new(PtkFileBrowser* browser,
                             const std::span<const std::shared_ptr<vfs::file_info>> sel_files);

#if (GTK_MAJOR_VERSION == 4)
void ptk_file_menu_add_panel_view_menu(PtkFileBrowser* browser, GtkWidget* menu,
                                       GtkEventController* accel_group);
#elif (GTK_MAJOR_VERSION == 3)
void ptk_file_menu_add_panel_view_menu(PtkFileBrowser* browser, GtkWidget* menu,
                                       GtkAccelGroup* accel_group);
#endif

void on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, PtkFileMenu* data);

void ptk_file_menu_action(PtkFileBrowser* browser, const xset_t& set);

void on_popup_sortby(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, i32 order);
void on_popup_list_detailed(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_icons(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_compact(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_large(GtkMenuItem* menuitem, PtkFileBrowser* browser);
