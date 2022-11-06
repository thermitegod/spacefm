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

#include <sigc++/sigc++.h>

#include <gtk/gtk.h>

#include "vfs/vfs-dir.hxx"
#include "settings.hxx"

#include "types.hxx"

#define PTK_FILE_BROWSER(obj)             (static_cast<PtkFileBrowser*>(obj))
#define PTK_FILE_BROWSER_REINTERPRET(obj) (reinterpret_cast<PtkFileBrowser*>(obj))

#define PTK_TYPE_FILE_BROWSER    (ptk_file_browser_get_type())
#define PTK_IS_FILE_BROWSER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_BROWSER))

enum PtkFBViewMode
{
    PTK_FB_ICON_VIEW,
    PTK_FB_LIST_VIEW,
    PTK_FB_COMPACT_VIEW
};

enum PtkFBSortOrder
{
    PTK_FB_SORT_BY_NAME,
    PTK_FB_SORT_BY_SIZE,
    PTK_FB_SORT_BY_MTIME,
    PTK_FB_SORT_BY_TYPE,
    PTK_FB_SORT_BY_PERM,
    PTK_FB_SORT_BY_OWNER
};

enum PtkFBChdirMode
{
    PTK_FB_CHDIR_NORMAL,
    PTK_FB_CHDIR_ADD_HISTORY,
    PTK_FB_CHDIR_NO_HISTORY,
    PTK_FB_CHDIR_BACK,
    PTK_FB_CHDIR_FORWARD
};

enum PtkOpenAction
{
    PTK_OPEN_DIR,
    PTK_OPEN_NEW_TAB,
    PTK_OPEN_NEW_WINDOW,
    PTK_OPEN_TERMINAL,
    PTK_OPEN_FILE
};

// forward declare
struct PtkFileBrowser;
struct MainWindow;

struct PtkFileBrowser
{
    /* parent class */
    GtkVBox parent;

    /* <private> */
    GList* history;
    GList* curHistory;
    GList* histsel;    // MOD added
    GList* curhistsel; // MOD added

    vfs::dir dir;
    GtkTreeModel* file_list;
    i32 max_thumbnail;
    i32 n_sel_files;
    off_t sel_size;
    off_t sel_disk_size;
    u32 sel_change_idle;

    // path bar auto seek
    bool inhibit_focus;
    char* seek_name;

    /* side pane */
    GtkWidget* side_pane_buttons;
    GtkToggleToolButton* location_btn;
    GtkToggleToolButton* dir_tree_btn;

    GtkSortType sort_type;
    PtkFBSortOrder sort_order : 4;
    PtkFBViewMode view_mode : 2;

    bool single_click : 1;
    bool show_hidden_files : 1;
    bool large_icons : 1;
    bool busy : 1;
    bool pending_drag_status : 1;
    dev_t drag_source_dev;
    ino_t drag_source_inode;
    i32 drag_x;
    i32 drag_y;
    bool pending_drag_status_tree : 1;
    dev_t drag_source_dev_tree;
    bool is_drag : 1;
    bool skip_release : 1;
    bool menu_shown : 1;
    char* book_set_name;

    /* directory view */
    GtkWidget* folder_view;
    GtkWidget* folder_view_scroll;
    GtkCellRenderer* icon_render;
    u32 single_click_timeout;

    // MOD
    panel_t mypanel;
    GtkWidget* mynotebook;
    GtkWidget* task_view;
    void* main_window;
    GtkWidget* toolbox;
    GtkWidget* path_bar;
    GtkWidget* hpane;
    GtkWidget* side_vbox;
    GtkWidget* side_toolbox;
    GtkWidget* side_vpane_top;
    GtkWidget* side_vpane_bottom;
    GtkWidget* side_dir_scroll;
    GtkWidget* side_dev_scroll;
    GtkWidget* side_dir;
    GtkWidget* side_dev;
    GtkWidget* status_bar;
    GtkFrame* status_frame;
    GtkLabel* status_label;
    GtkWidget* status_image;
    GtkWidget* toolbar;
    GtkWidget* side_toolbar;
    GSList* toolbar_widgets[10];

    GtkTreeIter book_iter_inserted;
    char* select_path;
    char* status_bar_custom;

    // Signals
  public:
    // Signals function types
    using evt_chdir_before_t = void(PtkFileBrowser*, MainWindow*);
    using evt_chdir_begin_t = void(PtkFileBrowser*, MainWindow*);
    using evt_chdir_after_t = void(PtkFileBrowser*, MainWindow*);
    using evt_open_file_t = void(PtkFileBrowser*, std::string_view, PtkOpenAction, MainWindow*);
    using evt_change_content_t = void(PtkFileBrowser*, MainWindow*);
    using evt_change_sel_t = void(PtkFileBrowser*, MainWindow*);
    using evt_change_pane_mode_t = void(PtkFileBrowser*, MainWindow*);

    // Signals Add Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_BEFORE, sigc::connection>::type
    add_event(evt_chdir_before_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHDIR_BEFORE");
        this->evt_data_window = window;
        return this->evt_chdir_before.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_BEGIN, sigc::connection>::type
    add_event(evt_chdir_begin_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHDIR_BEGIN");
        this->evt_data_window = window;
        return this->evt_chdir_begin.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_AFTER, sigc::connection>::type
    add_event(evt_chdir_after_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHDIR_AFTER");
        this->evt_data_window = window;
        return this->evt_chdir_after.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::OPEN_ITEM, sigc::connection>::type
    add_event(evt_open_file_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::OPEN_ITEM");
        this->evt_data_window = window;
        return this->evt_open_file.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_CONTENT, sigc::connection>::type
    add_event(evt_change_content_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHANGE_CONTENT");
        this->evt_data_window = window;
        return this->evt_change_content.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_SEL, sigc::connection>::type
    add_event(evt_change_sel_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHANGE_SEL");
        this->evt_data_window = window;
        return this->evt_change_sel.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_PANE, sigc::connection>::type
    add_event(evt_change_pane_mode_t fun, MainWindow* window)
    {
        // LOG_TRACE("Signal Connect   : EventType::CHANGE_PANE");
        this->evt_data_window = window;
        return this->evt_change_pane_mode.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_BEFORE, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHDIR_BEFORE");
        this->evt_chdir_before.emit(this, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_BEGIN, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHDIR_BEGIN");
        this->evt_chdir_begin.emit(this, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHDIR_AFTER, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHDIR_AFTER");
        this->evt_chdir_after.emit(this, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::OPEN_ITEM, void>::type
    run_event(std::string_view path, PtkOpenAction action)
    {
        // LOG_TRACE("Signal Execute   : EventType::OPEN_ITEM");
        this->evt_open_file.emit(this, path, action, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_CONTENT, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHANGE_CONTENT");
        this->evt_change_content.emit(this, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_SEL, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHANGE_SEL");
        this->evt_change_sel.emit(this, this->evt_data_window);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::CHANGE_PANE, void>::type
    run_event()
    {
        // LOG_TRACE("Signal Execute   : EventType::CHANGE_PANE");
        this->evt_change_pane_mode.emit(this, this->evt_data_window);
    }

    // Signals
  private:
    // Signal types
    sigc::signal<evt_chdir_before_t> evt_chdir_before;
    sigc::signal<evt_chdir_begin_t> evt_chdir_begin;
    sigc::signal<evt_chdir_after_t> evt_chdir_after;
    sigc::signal<evt_open_file_t> evt_open_file;
    sigc::signal<evt_change_content_t> evt_change_content;
    sigc::signal<evt_change_sel_t> evt_change_sel;
    sigc::signal<evt_change_pane_mode_t> evt_change_pane_mode;

  private:
    // Signal data
    // TODO/FIXME has to be a better way to do this
    MainWindow* evt_data_window{nullptr};

  public:
    // Signals we connect to
    sigc::connection signal_file_created;
    sigc::connection signal_file_deleted;
    sigc::connection signal_file_changed;
    sigc::connection signal_file_listed;
};

struct PtkFileBrowserClass
{
    GtkPanedClass parent;

    /* Default signal handlers */
    void (*before_chdir)(PtkFileBrowser* file_browser, std::string_view path);
    void (*begin_chdir)(PtkFileBrowser* file_browser);
    void (*after_chdir)(PtkFileBrowser* file_browser);
    void (*open_item)(PtkFileBrowser* file_browser, std::string_view path, i32 action);
    void (*content_change)(PtkFileBrowser* file_browser);
    void (*sel_change)(PtkFileBrowser* file_browser);
    void (*pane_mode_change)(PtkFileBrowser* file_browser);
};

GType ptk_file_browser_get_type();

GtkWidget* ptk_file_browser_new(i32 curpanel, GtkWidget* notebook, GtkWidget* task_view,
                                void* main_window);

/*
 * folder_path should be encodede in on-disk encoding
 */
bool ptk_file_browser_chdir(PtkFileBrowser* file_browser, std::string_view folder_path,
                            PtkFBChdirMode mode);

/*
 * returned path should be encodede in on-disk encoding
 * returned path should be encodede in UTF-8
 */
const std::string ptk_file_browser_get_cwd(PtkFileBrowser* file_browser);

u32 ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser);
u32 ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser);

u32 ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, u64* sel_size, u64* sel_disk_size);

void ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show);

void ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click);
void ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, u32 timeout);

/* Sorting files */
void ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser, PtkFBSortOrder order);

void ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order);

void ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, XSetName setname);
void ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser);

const std::vector<vfs::file_info> ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser);

/* Return a list of selected filenames (full paths in on-disk encoding) */
void ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser);

void ptk_file_browser_paste_link(PtkFileBrowser* file_browser);
void ptk_file_browser_paste_target(PtkFileBrowser* file_browser);

void ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_last(PtkFileBrowser* file_browser);
void ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                     const char* search_key);
void ptk_file_browser_canon(PtkFileBrowser* file_browser, std::string_view path);

void ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser,
                                            const std::vector<vfs::file_info>& sel_files,
                                            std::string_view cwd);

void ptk_file_browser_file_properties(PtkFileBrowser* file_browser, i32 page);

void ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_list(PtkFileBrowser* file_browser);

void ptk_file_browser_hide_selected(PtkFileBrowser* file_browser,
                                    const std::vector<vfs::file_info>& sel_files,
                                    std::string_view cwd);

void ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, i32 max_file_size);

// MOD
i32 ptk_file_browser_write_access(std::string_view cwd);
i32 ptk_file_browser_read_access(std::string_view cwd);

void ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, i32 t);
void ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, i32 job2);
void ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser);

bool ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                     PtkFileBrowser* file_browser);
void ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser);
void ptk_file_browser_focus_me(PtkFileBrowser* file_browser);
void ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, tab_t tab_num,
                                  std::string_view file_path);
void ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser,
                                    const std::vector<vfs::file_info>& sel_files,
                                    std::string_view cwd);
void ptk_file_browser_copycmd(PtkFileBrowser* file_browser,
                              const std::vector<vfs::file_info>& sel_files, std::string_view cwd,
                              XSetName setname);
void ptk_file_browser_on_action(PtkFileBrowser* browser, XSetName setname);
GList* folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model);
void ptk_file_browser_select_file(PtkFileBrowser* file_browser, std::string_view path);
void ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename,
                                       bool do_select);
void ptk_file_browser_seek_path(PtkFileBrowser* file_browser, std::string_view seek_dir,
                                std::string_view seek_name);
void ptk_file_browser_add_toolbar_widget(void* set_ptr, GtkWidget* widget);
void ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, void* set_ptr,
                                             XSetTool tool_type);
void ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                        GdkEventButton* event);
