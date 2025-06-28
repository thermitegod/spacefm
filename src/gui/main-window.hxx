/**
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
#include <map>
#include <string_view>

#include <gtkmm.h>

#include "xset/xset.hxx"

#include "gui/file-browser.hxx"

#include "types.hxx"

#define MAIN_WINDOW(obj) (static_cast<MainWindow*>(obj))
#define MAIN_WINDOW_REINTERPRET(obj) (reinterpret_cast<MainWindow*>(obj))

struct MainWindow
{
    /* Private */
    GtkApplicationWindow parent;

    /* protected */
    GtkBox* main_vbox;
    GtkWidget* menu_bar;

    GtkWidget* file_menu_item;
    GtkWidget* view_menu_item;
    GtkWidget* dev_menu_item;
    GtkWidget* book_menu_item;
    GtkWidget* tool_menu_item;
    GtkWidget* help_menu_item;
    GtkWidget* dev_menu;
    GtkNotebook* notebook; // current panel
    // GType will cause segfaults if this is a std::unordered_map
    std::map<panel_t, GtkNotebook*> panels;
    std::map<panel_t, i32> panel_slide_x;
    std::map<panel_t, i32> panel_slide_y;
    std::map<panel_t, i32> panel_slide_s;
    std::map<panel_t, xset::main_window_panel> panel_context;
    bool panel_change;

    panel_t curpanel;

    GtkPaned* hpane_top;
    GtkPaned* hpane_bottom;
    GtkPaned* vpane;
    GtkPaned* task_vpane;

    GtkScrolledWindow* task_scroll;
    GtkWidget* task_view;

    GtkAccelGroup* accel_group_;

    GtkWindowGroup* wgroup;
    u32 configure_evt_timer;
    bool maximized;
    bool opened_maximized;
    bool fullscreen;

    std::shared_ptr<config::settings> settings_;

  public:
    void update_window_icon() noexcept;
    void show_panels() noexcept;
    void focus_panel(const panel_t panel) noexcept;

    gui::browser* current_browser() const noexcept;

    GtkWidget* create_tab_label(gui::browser* browser) const noexcept;
    void new_tab(const std::filesystem::path& folder_path) noexcept;
    void open_path_in_current_tab(const std::filesystem::path& path) const noexcept;

    void set_window_title(gui::browser* browser) noexcept;

    bool is_main_tasks_running() const noexcept;

    GtkNotebook* get_panel_notebook(const panel_t panel) const noexcept;

    void open_network(const std::string_view url, const bool new_tab) const noexcept;
    void fullscreen_activate() noexcept;

    bool keypress(GdkEvent* event, void* user_data) noexcept;
    bool keypress_found_key(const xset_t& set) noexcept;

    void store_positions() noexcept;

    void close_window() noexcept;
    void add_new_window() noexcept;

    void open_terminal() const noexcept;

    void rebuild_menus() noexcept;
    void rebuild_menu_file(gui::browser* browser) noexcept;
    void rebuild_menu_view(gui::browser* browser) noexcept;
    void rebuild_menu_device(gui::browser* browser) noexcept;
    void rebuild_menu_bookmarks(gui::browser* browser) const noexcept;
    void rebuild_menu_help(gui::browser* browser) noexcept;

  public:
    // signals
    void on_browser_before_chdir(gui::browser* browser) noexcept;
    void on_browser_begin_chdir(gui::browser* browser) noexcept;
    void on_browser_after_chdir(gui::browser* browser) noexcept;
    void on_browser_open_item(gui::browser* browser, const std::filesystem::path& path,
                              gui::browser::open_action action) noexcept;
    void on_browser_content_change(gui::browser* browser) noexcept;
    void on_browser_sel_change(gui::browser* browser) noexcept;
    void on_browser_panel_change(gui::browser* browser) noexcept;
};

GType main_window_get_type() noexcept;

/* Utility functions */
gui::browser* main_window_get_current_browser() noexcept;

// void main_window_preference(MainWindow* main_window) noexcept;

/* get last active window */
MainWindow* main_window_get_last_active() noexcept;
MainWindow* main_window_get_on_current_desktop() noexcept;

std::span<MainWindow*> main_window_get_all() noexcept;

void show_panels_all_windows(GtkMenuItem* item, MainWindow* main_window) noexcept;
void update_views_all_windows(GtkWidget* item, gui::browser* browser) noexcept;
void main_window_reload_thumbnails_all_windows() noexcept;
void main_window_toggle_thumbnails_all_windows() noexcept;
void main_window_refresh_all_tabs_matching(const std::filesystem::path& path) noexcept;
void main_window_close_all_invalid_tabs() noexcept;
void main_window_rebuild_all_toolbars(gui::browser* browser) noexcept;

void main_window_rubberband_all() noexcept;
void main_window_refresh_all() noexcept;
void set_panel_focus(MainWindow* main_window, gui::browser* browser) noexcept;
