/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <gtk/gtk.h>

#include "../vfs/vfs-dir.hxx"

#define PTK_TYPE_FILE_BROWSER (ptk_file_browser_get_type())
#define PTK_FILE_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PTK_TYPE_FILE_BROWSER, PtkFileBrowser))
#define PTK_IS_FILE_BROWSER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_BROWSER))

enum PtkFBViewMode
{
    PTK_FB_ICON_VIEW,
    PTK_FB_LIST_VIEW,
    PTK_FB_COMPACT_VIEW
};

enum PtkFBSortOrder
{
    PTK_FB_SORT_BY_NAME = 0,
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

typedef struct PtkFileBrowser
{
    /* parent class */
    GtkVBox parent;

    /* <private> */
    GList* history;
    GList* curHistory;
    GList* histsel;    // MOD added
    GList* curhistsel; // MOD added

    VFSDir* dir;
    GtkTreeModel* file_list;
    int max_thumbnail;
    int n_sel_files;
    off_t sel_size;
    unsigned int sel_change_idle;

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
    int drag_x;
    int drag_y;
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
    unsigned int single_click_timeout;

    // MOD
    int mypanel;
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
    GtkWidget* side_book_scroll;
    GtkWidget* side_book;
    GtkWidget* side_dir;
    GtkWidget* side_dev;
    GtkWidget* status_bar;
    GtkFrame* status_frame;
    GtkLabel* status_label;
    GtkWidget* status_image;
    GtkWidget* toolbar;
    GtkWidget* side_toolbar;
    GSList* toolbar_widgets[10];

    bool bookmark_button_press : 1;
    GtkTreeIter book_iter_inserted;
    char* select_path;
    char* status_bar_custom;
} PtkFileBrowser;

enum PtkOpenAction
{
    PTK_OPEN_DIR,
    PTK_OPEN_NEW_TAB,
    PTK_OPEN_NEW_WINDOW,
    PTK_OPEN_TERMINAL,
    PTK_OPEN_FILE
};

typedef struct PtkFileBrowserClass
{
    GtkPanedClass parent;

    /* Default signal handlers */
    void (*before_chdir)(PtkFileBrowser* file_browser, const char* path, bool* cancel);
    void (*begin_chdir)(PtkFileBrowser* file_browser);
    void (*after_chdir)(PtkFileBrowser* file_browser);
    void (*open_item)(PtkFileBrowser* file_browser, const char* path, int action);
    void (*content_change)(PtkFileBrowser* file_browser);
    void (*sel_change)(PtkFileBrowser* file_browser);
    void (*pane_mode_change)(PtkFileBrowser* file_browser);
} PtkFileBrowserClass;

GType ptk_file_browser_get_type(void);

GtkWidget* ptk_file_browser_new(int curpanel, GtkWidget* notebook, GtkWidget* task_view,
                                void* main_window);

/*
 * folder_path should be encodede in on-disk encoding
 */
bool ptk_file_browser_chdir(PtkFileBrowser* file_browser, const char* folder_path,
                            PtkFBChdirMode mode);

/*
 * returned path should be encodede in on-disk encoding
 * returned path should be encodede in UTF-8
 */
const char* ptk_file_browser_get_cwd(PtkFileBrowser* file_browser);

unsigned int ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser);
unsigned int ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser);

unsigned int ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, uint64_t* sel_size);

void ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser);

void ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show);

void ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click);
void ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, unsigned int timeout);

/* Sorting files */
void ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser, PtkFBSortOrder order);

void ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order);

void ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, const char* setname);
void ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser);

GList* ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser);

/* Return a list of selected filenames (full paths in on-disk encoding) */
void ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser);

void ptk_file_browser_paste_link(PtkFileBrowser* file_browser);   // MOD added
void ptk_file_browser_paste_target(PtkFileBrowser* file_browser); // MOD added

void ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_last(PtkFileBrowser* file_browser); // MOD added
void ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                     const char* search_key); // sfm
void ptk_file_browser_canon(PtkFileBrowser* file_browser, const char* path);

void ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser, GList* files, char* cwd);

void ptk_file_browser_file_properties(PtkFileBrowser* file_browser, int page);

void ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser);
void ptk_file_browser_view_as_list(PtkFileBrowser* file_browser);

void ptk_file_browser_hide_selected(PtkFileBrowser* file_browser, GList* files, char* cwd);

void ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, int max_file_size);

void ptk_file_browser_emit_open(PtkFileBrowser* file_browser, const char* path,
                                PtkOpenAction action);

// MOD
int ptk_file_browser_no_access(const char* cwd, const char* smode);
void ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser);
void ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser);
void ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, int t);
void ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, int job2);
void ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser);

bool ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                     PtkFileBrowser* file_browser);
void ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser);
void ptk_file_browser_focus_me(PtkFileBrowser* file_browser);
void ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, int tab_num, char* file_path);
void ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser,
                                    GList* sel_files, char* cwd);
void ptk_file_browser_copycmd(PtkFileBrowser* file_browser, GList* sel_files, char* cwd,
                              char* setname);
void ptk_file_browser_on_action(PtkFileBrowser* browser, char* setname);
GList* folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model);
void ptk_file_browser_select_file(PtkFileBrowser* file_browser, const char* path);
void ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename,
                                       bool do_select);
void ptk_file_browser_seek_path(PtkFileBrowser* file_browser, const char* seek_dir,
                                const char* seek_name);
void ptk_file_browser_add_toolbar_widget(void* set_ptr, GtkWidget* widget);
void ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, void* set_ptr,
                                             char tool_type);
void ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                        GdkEventButton* event);
