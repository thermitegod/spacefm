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
#include <memory>
#include <span>
#include <vector>

#include <gtkmm.h>

#include "xset/xset.hxx"

#include "gui/file-browser.hxx"

#include "vfs/file.hxx"

/* selected_files is a list containing vfs::file structures
 * The list will be freed in this function, so the caller must not
 * free the list after calling this function.
 */

namespace gui
{
struct file_menu
{
    enum class app_job : std::uint8_t
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
    ~file_menu() noexcept;
    file_menu(const file_menu& other) = delete;
    file_menu(file_menu&& other) = delete;
    file_menu& operator=(const file_menu& other) = delete;
    file_menu& operator=(file_menu&& other) = delete;

    gui::browser* browser{nullptr};
    std::filesystem::path cwd;
    std::filesystem::path file_path;
    std::shared_ptr<vfs::file> file{nullptr};
    std::vector<std::shared_ptr<vfs::file>> selected_files;
#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group{nullptr};
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group{nullptr};
#endif
};
} // namespace gui

struct AutoOpenCreate : public std::enable_shared_from_this<AutoOpenCreate>
{
    AutoOpenCreate() = delete;
    AutoOpenCreate(gui::browser* browser, bool open_file) noexcept;
    ~AutoOpenCreate() = default;
    AutoOpenCreate(const AutoOpenCreate& other) = delete;
    AutoOpenCreate(AutoOpenCreate&& other) = delete;
    AutoOpenCreate& operator=(const AutoOpenCreate& other) = delete;
    AutoOpenCreate& operator=(AutoOpenCreate&& other) = delete;

    gui::browser* browser{nullptr};
    bool open_file{false};
    std::filesystem::path path;
    GFunc callback{nullptr};
};

GtkWidget*
gui_file_menu_new(gui::browser* browser,
                  const std::span<const std::shared_ptr<vfs::file>> selected_files = {}) noexcept;

#if (GTK_MAJOR_VERSION == 4)
void gui_file_menu_add_panel_view_menu(gui::browser* browser, GtkWidget* menu,
                                       GtkEventController* accel_group) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
void gui_file_menu_add_panel_view_menu(gui::browser* browser, GtkWidget* menu,
                                       GtkAccelGroup* accel_group) noexcept;
#endif

void on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, gui::file_menu* data) noexcept;

void gui_file_menu_action(gui::browser* browser, const xset_t& set) noexcept;

void on_popup_sortby(GtkMenuItem* menuitem, gui::browser* browser, i32 order) noexcept;
void on_popup_list_detailed(GtkMenuItem* menuitem, gui::browser* browser) noexcept;
void on_popup_list_icons(GtkMenuItem* menuitem, gui::browser* browser) noexcept;
void on_popup_list_compact(GtkMenuItem* menuitem, gui::browser* browser) noexcept;
void on_popup_list_large(GtkMenuItem* menuitem, gui::browser* browser) noexcept;
