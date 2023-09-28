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

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <map>

#include <gtk/gtk.h>

#include "types.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-file-task.hxx"

#include "ptk/ptk-file-browser.hxx"

#define MAIN_WINDOW(obj)             (static_cast<MainWindow*>(obj))
#define MAIN_WINDOW_REINTERPRET(obj) (reinterpret_cast<MainWindow*>(obj))

struct MainWindow
{
    /* Private */
    GtkWindow parent;

    /* protected */
    GtkWidget* main_vbox;
    GtkWidget* menu_bar;

    // MOD
    GtkWidget* file_menu_item;
    GtkWidget* view_menu_item;
    GtkWidget* dev_menu_item;
    GtkWidget* book_menu_item;
    GtkWidget* tool_menu_item;
    GtkWidget* help_menu_item;
    GtkWidget* dev_menu;
    GtkWidget* notebook; // MOD changed use to current panel
    GtkWidget* panel[4];
    i32 panel_slide_x[4];
    i32 panel_slide_y[4];
    i32 panel_slide_s[4];
    std::map<panel_t, xset::main_window_panel> panel_context;
    bool panel_change;

    panel_t curpanel;

    GtkWidget* hpane_top;
    GtkWidget* hpane_bottom;
    GtkWidget* vpane;
    GtkWidget* task_vpane;
    GtkWidget* task_scroll;
    GtkWidget* task_view;

    GtkAccelGroup* accel_group;

    GtkWindowGroup* wgroup;
    u32 configure_evt_timer;
    bool maximized;
    bool opened_maximized;
    bool fullscreen;
};

GType main_window_get_type();

GtkWidget* main_window_new();

/* Utility functions */
GtkWidget* main_window_get_current_file_browser(MainWindow* mainWindow);

void main_window_add_new_tab(MainWindow* main_window, const std::filesystem::path& folder_path);

GtkWidget* main_window_create_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser);

void main_window_update_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser,
                                  const std::filesystem::path& path);

// void main_window_preference(MainWindow* main_window);

/* get last active window */
MainWindow* main_window_get_last_active();
MainWindow* main_window_get_on_current_desktop();

/* get all windows
 * The returned GList is owned and used internally by MainWindow, and
 * should not be freed.
 */
const std::vector<MainWindow*>& main_window_get_all();

// void show_panels(GtkMenuItem* item, MainWindow* main_window);
void show_panels_all_windows(GtkMenuItem* item, MainWindow* main_window);
void update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser);
void main_window_reload_thumbnails_all_windows();
void main_window_toggle_thumbnails_all_windows();
void main_window_refresh_all_tabs_matching(const std::filesystem::path& path);
void main_window_close_all_invalid_tabs();
void main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser);
const std::string main_write_exports(vfs::file_task vtask, const std::string_view value);
const std::optional<std::filesystem::path> main_window_get_tab_cwd(PtkFileBrowser* file_browser,
                                                                   tab_t tab_num);
const std::optional<std::filesystem::path> main_window_get_panel_cwd(PtkFileBrowser* file_browser,
                                                                     panel_t panel_num);
bool main_window_panel_is_visible(PtkFileBrowser* file_browser, panel_t panel);
void main_window_open_in_panel(PtkFileBrowser* file_browser, panel_t panel_num,
                               const std::filesystem::path& file_path);
void main_window_rubberband_all();
void main_window_refresh_all();
void set_panel_focus(MainWindow* main_window, PtkFileBrowser* file_browser);
void focus_panel(MainWindow* main_window, const panel_t panel);
void main_window_open_path_in_current_tab(MainWindow* main_window,
                                          const std::filesystem::path& path);
void main_window_open_network(MainWindow* main_window, const std::string_view url, bool new_tab);
void main_window_store_positions(MainWindow* main_window);

void main_window_fullscreen_activate(MainWindow* main_window);
bool main_window_keypress(MainWindow* main_window, GdkEventKey* event, xset_t known_set);

void main_window_set_window_title(MainWindow* main_window, PtkFileBrowser* file_browser);
void main_window_update_status_bar(MainWindow* main_window, PtkFileBrowser* file_browser);

struct main_window_counts_data
{
    panel_t panel_count;
    tab_t tab_count;
    tab_t tab_num;
};
const main_window_counts_data main_window_get_counts(PtkFileBrowser* file_browser);
