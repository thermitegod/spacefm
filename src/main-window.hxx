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

#include <array>
#include <map>
#include <tuple>
#include <vector>

#include "types.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"

#include <gtk/gtk.h>
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

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
    GtkWidget* plug_menu_item;
    GtkWidget* tool_menu_item;
    GtkWidget* help_menu_item;
    GtkWidget* dev_menu;
    GtkWidget* plug_menu;
    GtkWidget* notebook; // MOD changed use to current panel
    GtkWidget* panel[4];
    i32 panel_slide_x[4];
    i32 panel_slide_y[4];
    i32 panel_slide_s[4];
    std::map<panel_t, MainWindowPanel> panel_context;
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

struct MainWindowClass
{
    GtkWindowClass parent;
};

GType main_window_get_type();

GtkWidget* main_window_new();

/* Utility functions */
GtkWidget* main_window_get_current_file_browser(MainWindow* mainWindow);

void main_window_add_new_tab(MainWindow* main_window, std::string_view folder_path);

GtkWidget* main_window_create_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser);

void main_window_update_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser,
                                  std::string_view path);

// void main_window_preference(MainWindow* main_window);

/* get last active window */
MainWindow* main_window_get_last_active();
MainWindow* main_window_get_on_current_desktop();

/* get all windows
 * The returned GList is owned and used internally by MainWindow, and
 * should not be freed.
 */
const std::vector<MainWindow*>& main_window_get_all();

void main_task_view_update_task(PtkFileTask* ptask);
void main_task_view_remove_task(PtkFileTask* ptask);
void main_task_pause_all_queued(PtkFileTask* ptask);
void main_task_start_queued(GtkWidget* view, PtkFileTask* new_task);
void on_restore_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser);
void on_close_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser);
// void show_panels(GtkMenuItem* item, MainWindow* main_window);
void show_panels_all_windows(GtkMenuItem* item, MainWindow* main_window);
void update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser);
void main_window_toggle_thumbnails_all_windows();
void main_window_refresh_all_tabs_matching(std::string_view path);
void main_window_close_all_invalid_tabs();
void main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser);
const std::string main_write_exports(vfs::file_task vtask, std::string_view value);
void on_reorder(GtkWidget* item, GtkWidget* parent);
char* main_window_get_tab_cwd(PtkFileBrowser* file_browser, tab_t tab_num);
char* main_window_get_panel_cwd(PtkFileBrowser* file_browser, panel_t panel_num);
const std::array<i64, 3> main_window_get_counts(PtkFileBrowser* file_browser);
bool main_window_panel_is_visible(PtkFileBrowser* file_browser, panel_t panel);
void main_window_open_in_panel(PtkFileBrowser* file_browser, panel_t panel_num,
                               std::string_view file_path);
void main_window_rubberband_all();
void main_window_refresh_all();
void main_context_fill(PtkFileBrowser* file_browser, const xset_context_t& c);
void set_panel_focus(MainWindow* main_window, PtkFileBrowser* file_browser);
void focus_panel(GtkMenuItem* item, void* mw, panel_t p);
void main_window_open_path_in_current_tab(MainWindow* main_window, std::string_view path);
void main_window_open_network(MainWindow* main_window, std::string_view path, bool new_tab);
bool main_window_event(void* mw, xset_t preset, XSetName event, i64 panel, i64 tab,
                       const char* focus, i32 keyval, i32 button, i32 state, bool visible);
void main_window_store_positions(MainWindow* main_window);

const std::tuple<char, std::string> main_window_socket_command(char* argv[]);
