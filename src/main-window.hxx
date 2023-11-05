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

#include <string_view>

#include <filesystem>

#include <vector>

#include <map>

#include <gtkmm.h>

#include "types.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-file-browser.hxx"

#define MAIN_WINDOW(obj)             (static_cast<MainWindow*>(obj))
#define MAIN_WINDOW_REINTERPRET(obj) (reinterpret_cast<MainWindow*>(obj))

struct MainWindow
{
    /* Private */
    GtkApplicationWindow parent;

    /* protected */
    GtkBox* main_vbox;
    GtkWidget* menu_bar;

    // MOD
    GtkWidget* file_menu_item;
    GtkWidget* view_menu_item;
    GtkWidget* dev_menu_item;
    GtkWidget* book_menu_item;
    GtkWidget* tool_menu_item;
    GtkWidget* help_menu_item;
    GtkWidget* dev_menu;
    GtkNotebook* notebook; // MOD changed use to current panel
    GtkNotebook* panels[4];
    i32 panel_slide_x[4];
    i32 panel_slide_y[4];
    i32 panel_slide_s[4];
    std::map<panel_t, xset::main_window_panel> panel_context;
    bool panel_change;

    panel_t curpanel;

    GtkPaned* hpane_top;
    GtkPaned* hpane_bottom;
    GtkPaned* vpane;
    GtkPaned* task_vpane;

    GtkScrolledWindow* task_scroll;
    GtkWidget* task_view;

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group;
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group;
#endif

    GtkWindowGroup* wgroup;
    u32 configure_evt_timer;
    bool maximized;
    bool opened_maximized;
    bool fullscreen;

  public:
    void update_window_icon() noexcept;
    void show_panels() noexcept;
    void focus_panel(const panel_t panel) noexcept;

    PtkFileBrowser* current_file_browser() const noexcept;

    GtkWidget* create_tab_label(PtkFileBrowser* file_browser) const noexcept;
    void new_tab(const std::filesystem::path& folder_path) noexcept;
    void open_path_in_current_tab(const std::filesystem::path& path) noexcept;

    void set_window_title(PtkFileBrowser* file_browser) noexcept;
    void update_status_bar(PtkFileBrowser* file_browser) const noexcept;

    bool is_main_tasks_running() const noexcept;

    GtkNotebook* get_panel_notebook(const panel_t panel) const noexcept;

  public:
    // signals
    void on_file_browser_before_chdir(PtkFileBrowser* file_browser);
    void on_file_browser_begin_chdir(PtkFileBrowser* file_browser);
    void on_file_browser_after_chdir(PtkFileBrowser* file_browser);
    void on_file_browser_open_item(PtkFileBrowser* file_browser, const std::filesystem::path& path,
                                   ptk::open_action action);
    void on_file_browser_content_change(PtkFileBrowser* file_browser);
    void on_file_browser_sel_change(PtkFileBrowser* file_browser);
    void on_file_browser_panel_change(PtkFileBrowser* file_browser);
};

GType main_window_get_type();

/* Utility functions */
PtkFileBrowser* main_window_get_current_file_browser();

// void main_window_preference(MainWindow* main_window);

/* get last active window */
MainWindow* main_window_get_last_active();
MainWindow* main_window_get_on_current_desktop();

/* get all windows
 * The returned GList is owned and used internally by MainWindow, and
 * should not be freed.
 */
const std::vector<MainWindow*>& main_window_get_all();

void show_panels_all_windows(GtkMenuItem* item, MainWindow* main_window);
void update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser);
void main_window_reload_thumbnails_all_windows();
void main_window_toggle_thumbnails_all_windows();
void main_window_refresh_all_tabs_matching(const std::filesystem::path& path);
void main_window_close_all_invalid_tabs();
void main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser);

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

void main_window_open_network(MainWindow* main_window, const std::string_view url, bool new_tab);
void main_window_store_positions(MainWindow* main_window);

void main_window_fullscreen_activate(MainWindow* main_window);
bool main_window_keypress(MainWindow* main_window, GdkEvent* event, void* user_data);

struct main_window_counts_data
{
    panel_t panel_count;
    tab_t tab_count;
    tab_t tab_num;
};
const main_window_counts_data main_window_get_counts(PtkFileBrowser* file_browser);
