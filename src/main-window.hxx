/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <vector>

#include <gtk/gtk.h>
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

#define FM_TYPE_MAIN_WINDOW (fm_main_window_get_type())
#define FM_MAIN_WINDOW(obj) (reinterpret_cast<FMMainWindow*>(obj))

enum MainWindowPanel
{ // how a panel shares vertical and horizontal space with other panels
    PANEL_NEITHER,
    PANEL_HORIZ,
    PANEL_VERT,
    PANEL_BOTH
};

struct FMMainWindow
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
    int panel_slide_x[4];
    int panel_slide_y[4];
    int panel_slide_s[4];
    char panel_context[4];
    bool panel_change;
    GtkWidget* panelbar;
    GtkWidget* panel_btn[4];
    GtkWidget* panel_image[4];
    int curpanel;
    GtkWidget* hpane_top;
    GtkWidget* hpane_bottom;
    GtkWidget* vpane;
    GtkWidget* task_vpane;
    GtkWidget* task_scroll;
    GtkWidget* task_view;

    GtkAccelGroup* accel_group;

    GtkWindowGroup* wgroup;
    unsigned int configure_evt_timer;
    bool maximized;
    bool opened_maximized;
    bool fullscreen;
};

struct FMMainWindowClass
{
    GtkWindowClass parent;
};

GType fm_main_window_get_type();

GtkWidget* fm_main_window_new();

/* Utility functions */
GtkWidget* fm_main_window_get_current_file_browser(FMMainWindow* mainWindow);

void fm_main_window_add_new_tab(FMMainWindow* main_window, const char* folder_path);

GtkWidget* fm_main_window_create_tab_label(FMMainWindow* main_window, PtkFileBrowser* file_browser);

void fm_main_window_update_tab_label(FMMainWindow* main_window, PtkFileBrowser* file_browser,
                                     const char* path);

// void fm_main_window_preference(FMMainWindow* main_window);

/* get last active window */
FMMainWindow* fm_main_window_get_last_active();
FMMainWindow* fm_main_window_get_on_current_desktop();

/* get all windows
 * The returned GList is owned and used internally by FMMainWindow, and
 * should not be freed.
 */
const std::vector<FMMainWindow*> fm_main_window_get_all();

void main_task_view_update_task(PtkFileTask* task);
void main_task_view_remove_task(PtkFileTask* task);
void main_task_pause_all_queued(PtkFileTask* ptask);
void main_task_start_queued(GtkWidget* view, PtkFileTask* new_task);
void on_close_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser);
// void show_panels(GtkMenuItem* item, FMMainWindow* main_window);
void show_panels_all_windows(GtkMenuItem* item, FMMainWindow* main_window);
void update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser);
void main_window_update_all_bookmark_views();
void main_window_toggle_thumbnails_all_windows();
void main_window_refresh_all_tabs_matching(const char* path);
void main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser);
void main_write_exports(VFSFileTask* vtask, const char* value, std::string& buf);
void on_reorder(GtkWidget* item, GtkWidget* parent);
char* main_window_get_tab_cwd(PtkFileBrowser* file_browser, int tab_num);
char* main_window_get_panel_cwd(PtkFileBrowser* file_browser, int panel_num);
void main_window_get_counts(PtkFileBrowser* file_browser, int* panel_count, int* tab_count,
                            int* tab_num);
bool main_window_panel_is_visible(PtkFileBrowser* file_browser, int panel);
void main_window_open_in_panel(PtkFileBrowser* file_browser, int panel_num, char* file_path);
void main_window_rubberband_all();
void main_window_refresh_all();
void main_window_bookmark_changed(const char* changed_set_name);
void main_context_fill(PtkFileBrowser* file_browser, XSetContext* c);
void set_panel_focus(FMMainWindow* main_window, PtkFileBrowser* file_browser);
void focus_panel(GtkMenuItem* item, void* mw, int p);
void main_window_open_path_in_current_tab(FMMainWindow* main_window, const char* path);
void main_window_open_network(FMMainWindow* main_window, const char* path, bool new_tab);
char main_window_socket_command(char* argv[], char** reply);
bool main_window_event(void* mw, XSet* preset, const char* event, int panel, int tab,
                       const char* focus, int keyval, int button, int state, bool visible);
void fm_main_window_store_positions(FMMainWindow* main_window);
