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

#include <string>
#include <filesystem>

#include <array>
#include <vector>

#include <malloc.h>

#include <fnmatch.h>
#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <exo/exo.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-file-misc.hxx"

#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-dir-tree-view.hxx"
#include "ptk/ptk-dir-tree.hxx"

#include "ptk/ptk-file-list.hxx"

#include "ptk/ptk-clipboard.hxx"

#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-path-entry.hxx"
#include "main-window.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "settings.hxx"
#include "utils.hxx"
#include "type-conversion.hxx"

static void ptk_file_browser_class_init(PtkFileBrowserClass* klass);
static void ptk_file_browser_init(PtkFileBrowser* file_browser);
static void ptk_file_browser_finalize(GObject* obj);
static void ptk_file_browser_get_property(GObject* obj, unsigned int prop_id, GValue* value,
                                          GParamSpec* pspec);
static void ptk_file_browser_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                                          GParamSpec* pspec);

/* Utility functions */
static GtkWidget* create_folder_view(PtkFileBrowser* file_browser, PtkFBViewMode view_mode);

static void init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view);

static GtkWidget* ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser);

static void on_dir_file_listed(VFSDir* dir, bool is_cancelled, PtkFileBrowser* file_browser);

static void ptk_file_browser_update_model(PtkFileBrowser* file_browser);

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath* folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, int x, int y);

/* signal handlers */
static void on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                                          PtkFileBrowser* file_browser);
static void on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                                         GtkTreeViewColumn* col, PtkFileBrowser* file_browser);
static void on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser);

static bool on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                              PtkFileBrowser* file_browser);
static bool on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
                                                PtkFileBrowser* file_browser);
static bool on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser);

void on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                               PtkFileBrowser* file_browser);

/* Drag & Drop */
static void on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                              int x, int y, GtkSelectionData* sel_data,
                                              unsigned int info, unsigned int time,
                                              void* user_data);

static void on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                                         GtkSelectionData* sel_data, unsigned int info,
                                         unsigned int time, PtkFileBrowser* file_browser);

static void on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                                      PtkFileBrowser* file_browser);

static bool on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x,
                                       int y, unsigned int time, PtkFileBrowser* file_browser);

static bool on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context,
                                      unsigned int time, PtkFileBrowser* file_browser);

static bool on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                                     unsigned int time, PtkFileBrowser* file_browser);

static void on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                                    PtkFileBrowser* file_browser);

/* Default signal handlers */
static void ptk_file_browser_before_chdir(PtkFileBrowser* file_browser, const char* path);
static void ptk_file_browser_after_chdir(PtkFileBrowser* file_browser);
static void ptk_file_browser_content_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_sel_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_open_item(PtkFileBrowser* file_browser, const char* path, int action);
static void ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser);
static void focus_folder_view(PtkFileBrowser* file_browser);
static void enable_toolbar(PtkFileBrowser* file_browser);
static void show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big,
                            int max_file_size);

static int file_list_order_from_sort_order(PtkFBSortOrder order);

static GtkPanedClass* parent_class = nullptr;

enum PTKFileBrowserSignal
{
    BEFORE_CHDIR_SIGNAL,
    BEGIN_CHDIR_SIGNAL,
    AFTER_CHDIR_SIGNAL,
    OPEN_ITEM_SIGNAL,
    CONTENT_CHANGE_SIGNAL,
    SEL_CHANGE_SIGNAL,
    PANE_MODE_CHANGE_SIGNAL,
    N_SIGNALS
};

static void rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);
static void rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);

static unsigned int signals[PTKFileBrowserSignal::N_SIGNALS] = {0};

static unsigned int folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = GTK_DIR_TAB_FORWARD;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

#define GDK_ACTION_ALL GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)

// must match main-window.c  main_window_socket_command
static const std::array<const char*, 6> column_titles{"Name",
                                                      "Size",
                                                      "Type",
                                                      "Permission",
                                                      "Owner",
                                                      "Modified"};

static const std::array<const char*, 6> column_names{"detcol_name",
                                                     "detcol_size",
                                                     "detcol_type",
                                                     "detcol_perm",
                                                     "detcol_owner",
                                                     "detcol_date"};

GType
ptk_file_browser_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(PtkFileBrowserClass),
            nullptr,
            nullptr,
            (GClassInitFunc)ptk_file_browser_class_init,
            nullptr,
            nullptr,
            sizeof(PtkFileBrowser),
            0,
            (GInstanceInitFunc)ptk_file_browser_init,
            nullptr,
        };
        type = g_type_register_static(GTK_TYPE_VBOX, "PtkFileBrowser", &info, (GTypeFlags)0);
    }
    return type;
}

static void
g_cclosure_marshal_VOID__POINTER_INT(GClosure* closure, GValue* return_value,
                                     unsigned int n_param_values, const GValue* param_values,
                                     void* invocation_hint, void* marshal_data)
{
    (void)return_value;
    (void)n_param_values;
    (void)invocation_hint;

    using GMarshalFunc_VOID__POINTER_INT =
        void (*)(void* data1, void* arg_1, int arg_2, void* data2);

    GMarshalFunc_VOID__POINTER_INT callback;
    GCClosure* cc = (GCClosure*)closure;
    void* data1;
    void* data2;

    if (n_param_values != 3)
    {
        LOG_ERROR("g_cclosure_marshal_VOID__POINTER_INT n_param_values != 3");
        return;
    }

    if (G_CCLOSURE_SWAP_DATA(closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer(param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer(param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__POINTER_INT)(marshal_data ? marshal_data : cc->callback);

    callback(data1,
             g_value_get_pointer(param_values + 1),
             g_value_get_int(param_values + 2),
             data2);
}

static void
ptk_file_browser_class_init(PtkFileBrowserClass* klass)
{
    GObjectClass* object_class = (GObjectClass*)klass;
    parent_class = (GtkPanedClass*)g_type_class_peek_parent(klass);

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;

    // before-chdir is emitted when PtkFileBrowser is about to change
    // its working directory. The param is the path of the dir (in UTF-8),
    signals[PTKFileBrowserSignal::BEFORE_CHDIR_SIGNAL] =
        g_signal_new("before-chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, before_chdir),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);

    signals[PTKFileBrowserSignal::BEGIN_CHDIR_SIGNAL] =
        g_signal_new("begin-chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, begin_chdir),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[PTKFileBrowserSignal::AFTER_CHDIR_SIGNAL] =
        g_signal_new("after-chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, after_chdir),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /*
     * This signal is sent when a directory is about to be opened
     * arg1 is the path to be opened, and arg2 is the type of action,
     * ex: open in tab, open in terminal...etc.
     */
    signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL] =
        g_signal_new("open-item",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, open_item),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__POINTER_INT,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_POINTER,
                     G_TYPE_INT);

    signals[PTKFileBrowserSignal::CONTENT_CHANGE_SIGNAL] =
        g_signal_new("content-change",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, content_change),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[PTKFileBrowserSignal::SEL_CHANGE_SIGNAL] =
        g_signal_new("sel-change",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, sel_change),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[PTKFileBrowserSignal::PANE_MODE_CHANGE_SIGNAL] =
        g_signal_new("pane-mode-change",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, pane_mode_change),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}

bool
ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                PtkFileBrowser* file_browser)
{
    (void)event;
    int pos;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];

    XSet* set = xset_get_panel_mode(p, "slider_positions", mode);

    if (widget == file_browser->hpane)
    {
        pos = gtk_paned_get_position(GTK_PANED(file_browser->hpane));
        if (!main_window->fullscreen)
        {
            free(set->x);
            set->x = ztd::strdup(pos);
        }
        main_window->panel_slide_x[p - 1] = pos;
        // LOG_INFO("    slide_x = {}", pos);
    }
    else
    {
        // LOG_INFO("ptk_file_browser_slider_release fb={:p}  (panel {})  mode = {}", file_browser,
        // p, fmt::ptr(mode));
        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_top));
        if (!main_window->fullscreen)
        {
            free(set->y);
            set->y = ztd::strdup(pos);
        }
        main_window->panel_slide_y[p - 1] = pos;
        // LOG_INFO("    slide_y = {}  ", pos);

        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_bottom));
        if (!main_window->fullscreen)
        {
            free(set->s);
            set->s = ztd::strdup(pos);
        }
        main_window->panel_slide_s[p - 1] = pos;
        // LOG_INFO("slide_s = {}", pos);
    }
    return false;
}

void
ptk_file_browser_select_file(PtkFileBrowser* file_browser, const char* path)
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model = nullptr;
    VFSFileInfo* file;

    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW ||
        file_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
    {
        exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
        model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
    }
    else if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        gtk_tree_selection_unselect_all(tree_sel);
    }
    if (!model)
        return;
    char* name = g_path_get_basename(path);

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (file)
            {
                const char* file_name = vfs_file_info_get_name(file);
                if (!strcmp(file_name, name))
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW ||
                        file_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                  tree_path);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    else if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
                    {
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    gtk_tree_path_free(tree_path);
                    vfs_file_info_unref(file);
                    break;
                }
                vfs_file_info_unref(file);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
    free(name);
}

static void
save_command_history(GtkEntry* entry)
{
    std::string text = gtk_entry_get_text(GTK_ENTRY(entry));

    if (text.empty())
        return;

    xset_cmd_history.push_back(text);

    // shorten to 200 entries
    while (xset_cmd_history.size() > 200)
    {
        xset_cmd_history.erase(xset_cmd_history.begin());
    }
}

static bool
on_address_bar_focus_in(GtkWidget* entry, GdkEventFocus* evt, PtkFileBrowser* file_browser)
{
    (void)entry;
    (void)evt;
    ptk_file_browser_focus_me(file_browser);
    return false;
}

static void
on_address_bar_activate(GtkWidget* entry, PtkFileBrowser* file_browser)
{
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));

    gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0); // clear selection

    if (text[0] == '\0')
        return;

    // Convert to on-disk encoding
    std::string dir_path = g_filename_from_utf8(text, -1, nullptr, nullptr, nullptr);
    std::string final_path = std::filesystem::absolute(dir_path);

    bool final_path_exists = std::filesystem::exists(final_path);

    if (!final_path_exists &&
        (text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' || text[0] == '\0'))
    {
        // command
        std::string command;
        bool as_root = false;
        bool in_terminal = false;
        bool as_task = true;
        std::string prefix;
        while (text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!')
        {
            if (text[0] == '+')
                in_terminal = true;
            else if (text[0] == '&')
                as_task = false;
            else if (text[0] == '!')
                as_root = true;

            prefix = fmt::format("{}{}", prefix, text[0]);
            text++;
        }
        bool is_space = text[0] == ' ';
        command = ztd::strip(text);
        if (command.empty())
        {
            ptk_path_entry_help(entry, GTK_WIDGET(file_browser));
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            return;
        }

        save_command_history(GTK_ENTRY(entry));

        // task
        char* task_name;
        const char* cwd;
        task_name = ztd::strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
        cwd = ptk_file_browser_get_cwd(file_browser);
        PtkFileTask* ptask;
        ptask =
            ptk_file_exec_new(task_name, cwd, GTK_WIDGET(file_browser), file_browser->task_view);
        free(task_name);
        // do not free cwd!
        ptask->task->exec_browser = file_browser;
        ptask->task->exec_command = replace_line_subs(command);
        if (as_root)
            ptask->task->exec_as_user = "root";
        if (!as_task)
            ptask->task->exec_sync = false;
        else
            ptask->task->exec_sync = !in_terminal;
        ptask->task->exec_show_output = true;
        ptask->task->exec_show_error = true;
        ptask->task->exec_export = true;
        ptask->task->exec_terminal = in_terminal;
        ptask->task->exec_keep_terminal = as_task;
        // ptask->task->exec_keep_tmp = true;
        ptk_file_task_run(ptask);
        // gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );

        // reset entry text
        prefix = fmt::format("{}{}", prefix, is_space ? " " : "");
        gtk_entry_set_text(GTK_ENTRY(entry), prefix.c_str());
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    }
    else if (!final_path_exists && text[0] == '%')
    {
        const std::string str = ztd::strip(++text);
        if (!str.empty())
        {
            save_command_history(GTK_ENTRY(entry));
            ptk_file_browser_select_pattern(nullptr, file_browser, str.c_str());
        }
    }
    else if ((text[0] != '/' && strstr(text, ":/")) || Glib::str_has_prefix(text, "//"))
    {
        save_command_history(GTK_ENTRY(entry));
        ptk_location_view_mount_network(file_browser, text, false, false);
        return;
    }
    else
    {
        // path?
        // clean double slashes
        final_path = ztd::replace(final_path, "//", "/");

        if (std::filesystem::is_directory(final_path))
        {
            // open dir
            if (strcmp(final_path.c_str(), ptk_file_browser_get_cwd(file_browser)))
                ptk_file_browser_chdir(file_browser,
                                       final_path.c_str(),
                                       PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        else if (final_path_exists)
        {
            struct stat statbuf;
            if (stat(final_path.c_str(), &statbuf) == 0 && S_ISBLK(statbuf.st_mode) &&
                ptk_location_view_open_block(final_path.c_str(), false))
            {
                // ptk_location_view_open_block opened device
            }
            else
            {
                // open dir and select file
                dir_path = g_path_get_dirname(final_path.c_str());
                if (!ztd::contains(dir_path, ptk_file_browser_get_cwd(file_browser)))
                {
                    free(file_browser->select_path);
                    file_browser->select_path = ztd::strdup(final_path);
                    ptk_file_browser_chdir(file_browser,
                                           dir_path.c_str(),
                                           PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
                }
                else
                {
                    ptk_file_browser_select_file(file_browser, final_path.c_str());
                }
            }
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);

        // inhibit auto seek because if multiple completions will change dir
        EntryData* edata = static_cast<EntryData*>(g_object_get_data(G_OBJECT(entry), "edata"));
        if (edata && edata->seek_timer)
        {
            g_source_remove(edata->seek_timer);
            edata->seek_timer = 0;
        }
    }
}

void
ptk_file_browser_add_toolbar_widget(void* set_ptr, GtkWidget* widget)
{ // store the toolbar widget created by set for later change of status
    XSet* set = XSET(set_ptr);

    if (!(set && !set->lock && set->browser && set->tool != XSetTool::NOT && GTK_IS_WIDGET(widget)))
        return;

    unsigned char x;

    switch (set->tool)
    {
        case XSetTool::UP:
            x = 0;
            break;
        case XSetTool::BACK:
        case XSetTool::BACK_MENU:
            x = 1;
            break;
        case XSetTool::FWD:
        case XSetTool::FWD_MENU:
            x = 2;
            break;
        case XSetTool::DEVICES:
            x = 3;
            break;
        case XSetTool::BOOKMARKS:
            x = 4;
            break;
        case XSetTool::TREE:
            x = 5;
            break;
        case XSetTool::SHOW_HIDDEN:
            x = 6;
            break;
        case XSetTool::CUSTOM:
            if (set->menu_style == XSetMenu::CHECK)
            {
                x = 7;
                // attach set pointer to custom checkboxes so we can find it
                g_object_set_data(G_OBJECT(widget), "set", set);
            }
            else
                return;
            break;
        case XSetTool::SHOW_THUMB:
            x = 8;
            break;
        case XSetTool::LARGE_ICONS:
            x = 9;
            break;
        case XSetTool::NOT:
        case XSetTool::HOME:
        case XSetTool::DEFAULT:
        case XSetTool::REFRESH:
        case XSetTool::NEW_TAB:
        case XSetTool::NEW_TAB_HERE:
        case XSetTool::INVALID:
        default:
            return;
    }

    set->browser->toolbar_widgets[x] = g_slist_append(set->browser->toolbar_widgets[x], widget);
}

void
ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, void* set_ptr,
                                        XSetTool tool_type)
{
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return;

    unsigned char x;
    GSList* l;
    GtkWidget* widget;
    XSet* set = XSET(set_ptr);

    if (set && !set->lock && set->menu_style == XSetMenu::CHECK && set->tool == XSetTool::CUSTOM)
    {
        // a custom checkbox is being updated
        for (l = file_browser->toolbar_widgets[7]; l; l = l->next)
        {
            if (XSET(g_object_get_data(G_OBJECT(l->data), "set")) == set)
            {
                widget = GTK_WIDGET(l->data);
                if (GTK_IS_TOGGLE_BUTTON(widget))
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
                                                 set->b == XSetB::XSET_B_TRUE);
                    return;
                }
            }
        }
        LOG_WARN("ptk_file_browser_update_toolbar_widget widget not found for set");
        return;
    }
    else if (set_ptr)
    {
        LOG_WARN("ptk_file_browser_update_toolbar_widget invalid set_ptr or set");
        return;
    }

    // builtin tool
    bool b;

    switch (tool_type)
    {
        case XSetTool::UP:
            const char* cwd;
            x = 0;
            cwd = ptk_file_browser_get_cwd(file_browser);
            b = !cwd || strcmp(cwd, "/");
            break;
        case XSetTool::BACK:
        case XSetTool::BACK_MENU:
            x = 1;
            b = file_browser->curHistory && file_browser->curHistory->prev;
            break;
        case XSetTool::FWD:
        case XSetTool::FWD_MENU:
            x = 2;
            b = file_browser->curHistory && file_browser->curHistory->next;
            break;
        case XSetTool::DEVICES:
            x = 3;
            b = !!file_browser->side_dev;
            break;
        case XSetTool::BOOKMARKS:
            x = 4;
            b = !!file_browser->side_book;
            break;
        case XSetTool::TREE:
            x = 5;
            b = !!file_browser->side_dir;
            break;
        case XSetTool::SHOW_HIDDEN:
            x = 6;
            b = file_browser->show_hidden_files;
            break;
        case XSetTool::SHOW_THUMB:
            x = 8;
            b = app_settings.show_thumbnail;
            break;
        case XSetTool::LARGE_ICONS:
            x = 9;
            b = file_browser->large_icons;
            break;
        case XSetTool::NOT:
        case XSetTool::CUSTOM:
        case XSetTool::HOME:
        case XSetTool::DEFAULT:
        case XSetTool::REFRESH:
        case XSetTool::NEW_TAB:
        case XSetTool::NEW_TAB_HERE:
        case XSetTool::INVALID:
        default:
            LOG_WARN("ptk_file_browser_update_toolbar_widget invalid tool_type");
            return;
    }

    // update all widgets in list
    for (l = file_browser->toolbar_widgets[x]; l; l = l->next)
    {
        widget = GTK_WIDGET(l->data);
        if (GTK_IS_TOGGLE_BUTTON(widget))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), b);
        else if (GTK_IS_WIDGET(widget))
            gtk_widget_set_sensitive(widget, b);
        else
        {
            LOG_WARN("ptk_file_browser_update_toolbar_widget invalid widget");
        }
    }
}

static void
enable_toolbar(PtkFileBrowser* file_browser)
{
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::BACK);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::FWD);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::UP);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::DEVICES);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::BOOKMARKS);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::TREE);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::SHOW_HIDDEN);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::SHOW_THUMB);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::LARGE_ICONS);
}

static void
rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    // LOG_INFO("rebuild_toolbox");
    if (!file_browser)
        return;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p - 1] : 0;

    bool show_tooltips = !xset_get_b_panel(1, "tool_l");

    // destroy
    if (file_browser->toolbar)
    {
        if (GTK_IS_WIDGET(file_browser->toolbar))
            gtk_widget_destroy(file_browser->toolbar);
        file_browser->toolbar = nullptr;
        file_browser->path_bar = nullptr;
    }

    if (!file_browser->path_bar)
    {
        file_browser->path_bar = ptk_path_entry_new(file_browser);
        g_signal_connect(file_browser->path_bar,
                         "activate",
                         G_CALLBACK(on_address_bar_activate),
                         file_browser);
        g_signal_connect(file_browser->path_bar,
                         "focus-in-event",
                         G_CALLBACK(on_address_bar_focus_in),
                         file_browser);
    }

    // create toolbar
    file_browser->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(file_browser->toolbox), file_browser->toolbar, true, true, 0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->toolbar), GTK_TOOLBAR_ICONS);
    if (app_settings.tool_icon_size > 0 && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG)
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->toolbar),
                                  (GtkIconSize)app_settings.tool_icon_size);

    // fill left toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, "tool_l"),
                      show_tooltips);

    // add pathbar
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, true);
    gtk_toolbar_insert(GTK_TOOLBAR(file_browser->toolbar), toolitem, -1);
    gtk_container_add(GTK_CONTAINER(toolitem), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(file_browser->path_bar), true, true, 5);

    // fill right toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, "tool_r"),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, "show_toolbox", mode))
        gtk_widget_show_all(file_browser->toolbox);
}

static void
rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p - 1] : 0;

    bool show_tooltips = !xset_get_b_panel(1, "tool_l");

    // destroy
    if (file_browser->side_toolbar)
        gtk_widget_destroy(file_browser->side_toolbar);

    // create side toolbar
    file_browser->side_toolbar = gtk_toolbar_new();

    gtk_box_pack_start(GTK_BOX(file_browser->side_toolbox),
                       file_browser->side_toolbar,
                       true,
                       true,
                       0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->side_toolbar), GTK_TOOLBAR_ICONS);
    if (app_settings.tool_icon_size > 0 && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG)
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->side_toolbar),
                                  (GtkIconSize)app_settings.tool_icon_size);
    // fill side toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->side_toolbar,
                      xset_get_panel(p, "tool_s"),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, "show_sidebar", mode))
        gtk_widget_show_all(file_browser->side_toolbox);
}

void
ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser)
{
    for (unsigned int i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
    {
        g_slist_free(file_browser->toolbar_widgets[i]);
        file_browser->toolbar_widgets[i] = nullptr;
    }
    if (file_browser->toolbar)
    {
        rebuild_toolbox(nullptr, file_browser);
        const std::string disp_path =
            Glib::filename_display_name(ptk_file_browser_get_cwd(file_browser));
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), disp_path.c_str());
    }
    if (file_browser->side_toolbar)
        rebuild_side_toolbox(nullptr, file_browser);

    enable_toolbar(file_browser);
}

static bool
on_status_bar_button_press(GtkWidget* widget, GdkEventButton* event, PtkFileBrowser* file_browser)
{
    (void)widget;
    focus_folder_view(file_browser);
    if (event->type == GDK_BUTTON_PRESS)
    {
        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              XSetName::EVT_WIN_CLICK,
                              0,
                              0,
                              "statusbar",
                              0,
                              event->button,
                              event->state,
                              true))
            return true;
        if (event->button == 2)
        {
            const std::array<XSetName, 4> setnames{
                XSetName::STATUS_NAME,
                XSetName::STATUS_PATH,
                XSetName::STATUS_INFO,
                XSetName::STATUS_HIDE,
            };

            for (std::size_t i = 0; i < setnames.size(); i++)
            {
                if (!xset_get_b(setnames.at(i)))
                    continue;

                if (i < 2)
                {
                    std::vector<VFSFileInfo*> sel_files;

                    sel_files = ptk_file_browser_get_selected_files(file_browser);
                    if (sel_files.empty())
                        return true;

                    if (i == 0)
                        ptk_clipboard_copy_name(ptk_file_browser_get_cwd(file_browser), sel_files);
                    else
                        ptk_clipboard_copy_as_text(ptk_file_browser_get_cwd(file_browser),
                                                   sel_files);

                    vfs_file_info_list_free(sel_files);
                }
                else if (i == 2)
                    ptk_file_browser_file_properties(file_browser, 0);
                else if (i == 3)
                    focus_panel(nullptr, file_browser->main_window, -3);
            }
            return true;
        }
    }
    return false;
}

static void
on_status_effect_change(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    set_panel_focus(nullptr, file_browser);
}

static void
on_status_middle_click_config(GtkMenuItem* menuitem, XSet* set)
{
    (void)menuitem;

    const std::array<XSetName, 4> setnames{
        XSetName::STATUS_NAME,
        XSetName::STATUS_PATH,
        XSetName::STATUS_INFO,
        XSetName::STATUS_HIDE,
    };

    for (XSetName setname: setnames)
    {
        if (set->xset_name == setname)
            set->b = XSetB::XSET_B_TRUE;
        else
            xset_set_b(setname, false);
    }
}

static void
on_status_bar_popup(GtkWidget* widget, GtkWidget* menu, PtkFileBrowser* file_browser)
{
    (void)widget;
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    const std::string desc =
        fmt::format("separator panel{}_icon_status status_middle", file_browser->mypanel);

    xset_set_cb_panel(file_browser->mypanel,
                      "icon_status",
                      (GFunc)on_status_effect_change,
                      file_browser);
    XSet* set = xset_get(XSetName::STATUS_NAME);
    xset_set_cb(XSetName::STATUS_NAME, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, nullptr);
    XSet* set_radio = set;
    set = xset_get(XSetName::STATUS_PATH);
    xset_set_cb(XSetName::STATUS_PATH, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_get(XSetName::STATUS_INFO);
    xset_set_cb(XSetName::STATUS_INFO, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_get(XSetName::STATUS_HIDE);
    xset_set_cb(XSetName::STATUS_HIDE, (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);

    xset_add_menu(file_browser, menu, accel_group, desc.c_str());
    gtk_widget_show_all(menu);
    g_signal_connect(menu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
}

static void
ptk_file_browser_init(PtkFileBrowser* file_browser)
{
    file_browser->mypanel = 0; // do not load font yet in ptk_path_entry_new
    file_browser->path_bar = ptk_path_entry_new(file_browser);
    g_signal_connect(file_browser->path_bar,
                     "activate",
                     G_CALLBACK(on_address_bar_activate),
                     file_browser);
    g_signal_connect(file_browser->path_bar,
                     "focus-in-event",
                     G_CALLBACK(on_address_bar_focus_in),
                     file_browser);

    // toolbox
    file_browser->toolbar = nullptr;
    file_browser->toolbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->toolbox, false, false, 0);

    // lists area
    file_browser->hpane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    file_browser->side_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(file_browser->side_vbox, 140, -1);
    file_browser->folder_view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_paned_pack1(GTK_PANED(file_browser->hpane), file_browser->side_vbox, false, false);
    gtk_paned_pack2(GTK_PANED(file_browser->hpane), file_browser->folder_view_scroll, true, true);

    // fill side
    file_browser->side_toolbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    file_browser->side_toolbar = nullptr;
    file_browser->side_vpane_top = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    file_browser->side_vpane_bottom = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    file_browser->side_dir_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    file_browser->side_book_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    file_browser->side_dev_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_toolbox,
                       false,
                       false,
                       0);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_vpane_top,
                       true,
                       true,
                       0);
    // see https://github.com/BwackNinja/spacefm/issues/21
    gtk_paned_pack1(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_dev_scroll,
                    false,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_vpane_bottom,
                    true,
                    false);
    gtk_paned_pack1(GTK_PANED(file_browser->side_vpane_bottom),
                    file_browser->side_book_scroll,
                    false,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_bottom),
                    file_browser->side_dir_scroll,
                    true,
                    false);

    // status bar
    file_browser->status_bar = gtk_statusbar_new();

    GList* children = gtk_container_get_children(GTK_CONTAINER(file_browser->status_bar));
    file_browser->status_frame = GTK_FRAME(children->data);
    g_list_free(children);
    children = gtk_container_get_children(
        GTK_CONTAINER(gtk_statusbar_get_message_area(GTK_STATUSBAR(file_browser->status_bar))));
    file_browser->status_label = GTK_LABEL(children->data);
    g_list_free(children);
    // do not know panel yet
    file_browser->status_image = xset_get_image("gtk-yes", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(file_browser->status_bar),
                       file_browser->status_image,
                       false,
                       false,
                       0);
    // required for button event
    gtk_label_set_selectable(file_browser->status_label, true);
    gtk_widget_set_can_focus(GTK_WIDGET(file_browser->status_label), false);
    gtk_widget_set_hexpand(GTK_WIDGET(file_browser->status_label), true);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_FILL);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_CENTER);

    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "button-press-event",
                     G_CALLBACK(on_status_bar_button_press),
                     file_browser);
    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "populate-popup",
                     G_CALLBACK(on_status_bar_popup),
                     file_browser);

    // pack fb vbox
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->hpane, true, true, 0);
    // TODO pack task frames
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->status_bar, false, false, 0);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dir_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_book_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dev_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    g_signal_connect(file_browser->hpane,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_top,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_bottom,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
}

static void
ptk_file_browser_finalize(GObject* obj)
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(obj);
    // LOG_INFO("ptk_file_browser_finalize");
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    /* Remove all idle handlers which are not called yet. */
    do
    {
    } while (g_source_remove_by_user_data(file_browser));

    if (file_browser->file_list)
    {
        g_signal_handlers_disconnect_matched(file_browser->file_list,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(G_OBJECT(file_browser->file_list));
    }

    free(file_browser->status_bar_custom);
    free(file_browser->seek_name);
    file_browser->seek_name = nullptr;
    free(file_browser->book_set_name);
    file_browser->book_set_name = nullptr;
    free(file_browser->select_path);
    file_browser->select_path = nullptr;
    for (unsigned int i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
    {
        g_slist_free(file_browser->toolbar_widgets[i]);
        file_browser->toolbar_widgets[i] = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that killing the browser results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

static void
ptk_file_browser_get_property(GObject* obj, unsigned int prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
ptk_file_browser_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                              GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

void
ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    // LOG_INFO("ptk_file_browser_update_views fb={:p}  (panel {})", file_browser,
    // file_browser->mypanel);

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    // hide/show browser widgets based on user settings
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];
    bool need_enable_toolbar = false;

    if (xset_get_b_panel_mode(p, "show_toolbox", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->toolbar || !gtk_widget_get_visible(file_browser->toolbox)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->toolbar)
        {
            rebuild_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->toolbox);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->toolbox && gtk_widget_get_visible(file_browser->toolbox))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->toolbox);
    }

    if (xset_get_b_panel_mode(p, "show_sidebar", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_toolbox || !gtk_widget_get_visible(file_browser->side_toolbox)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_toolbar)
        {
            rebuild_side_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->side_toolbox);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_toolbar && file_browser->side_toolbox &&
            gtk_widget_get_visible(file_browser->side_toolbox))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              false);
        /*  toolboxes must be destroyed together for toolbar_widgets[]
        if ( file_browser->side_toolbar )
        {
            gtk_widget_destroy( file_browser->side_toolbar );
            file_browser->side_toolbar = nullptr;
        }
        */
        gtk_widget_hide(file_browser->side_toolbox);
    }

    if (xset_get_b_panel_mode(p, "show_dirtree", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_dir_scroll ||
             !gtk_widget_get_visible(file_browser->side_dir_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_dir)
        {
            file_browser->side_dir = ptk_file_browser_create_dir_tree(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dir_scroll), file_browser->side_dir);
        }
        gtk_widget_show_all(file_browser->side_dir_scroll);
        if (file_browser->side_dir && file_browser->file_list)
            ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                    ptk_file_browser_get_cwd(file_browser));
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_dir_scroll && gtk_widget_get_visible(file_browser->side_dir_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_dir_scroll);
        if (file_browser->side_dir)
            gtk_widget_destroy(file_browser->side_dir);
        file_browser->side_dir = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_book", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_book_scroll ||
             !gtk_widget_get_visible(file_browser->side_book_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "bookmarks",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_book)
        {
            file_browser->side_book = ptk_bookmark_view_new(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_book_scroll),
                              file_browser->side_book);
        }
        gtk_widget_show_all(file_browser->side_book_scroll);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_book_scroll &&
            gtk_widget_get_visible(file_browser->side_book_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "bookmarks",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_book_scroll);
        if (file_browser->side_book)
            gtk_widget_destroy(file_browser->side_book);
        file_browser->side_book = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_devmon", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_dev_scroll ||
             !gtk_widget_get_visible(file_browser->side_dev_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_dev)
        {
            file_browser->side_dev = ptk_location_view_new(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dev_scroll), file_browser->side_dev);
        }
        gtk_widget_show_all(file_browser->side_dev_scroll);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_dev_scroll && gtk_widget_get_visible(file_browser->side_dev_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              XSetName::EVT_PNL_SHOW,
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_dev_scroll);
        if (file_browser->side_dev)
            gtk_widget_destroy(file_browser->side_dev);
        file_browser->side_dev = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_book", mode) ||
        xset_get_b_panel_mode(p, "show_dirtree", mode))
        gtk_widget_show(file_browser->side_vpane_bottom);
    else
        gtk_widget_hide(file_browser->side_vpane_bottom);

    if (xset_get_b_panel_mode(p, "show_devmon", mode) ||
        xset_get_b_panel_mode(p, "show_dirtree", mode) ||
        xset_get_b_panel_mode(p, "show_book", mode))
        gtk_widget_show(file_browser->side_vbox);
    else
        gtk_widget_hide(file_browser->side_vbox);

    if (need_enable_toolbar)
        enable_toolbar(file_browser);
    else
    {
        // toggle sidepane toolbar buttons
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::DEVICES);
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::BOOKMARKS);
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::TREE);
    }

    // set slider positions
    int pos;
    // hpane
    pos = main_window->panel_slide_x[p - 1];
    if (pos < 100)
        pos = -1;
    // LOG_INFO("    set slide_x = {}", pos);
    if (pos > 0)
        gtk_paned_set_position(GTK_PANED(file_browser->hpane), pos);

    // side_vpane_top
    pos = main_window->panel_slide_y[p - 1];
    if (pos < 20)
        pos = -1;
    // LOG_INFO("    slide_y = {}", pos);
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_top), pos);

    // side_vpane_bottom
    pos = main_window->panel_slide_s[p - 1];
    if (pos < 20)
        pos = -1;
    // LOG_INFO( "slide_s = {}", pos);
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_bottom), pos);

    // Large Icons - option for Detailed and Compact list views
    bool large_icons =
        xset_get_b_panel(p, "list_icons") || xset_get_b_panel_mode(p, "list_large", mode);
    if (large_icons != !!file_browser->large_icons)
    {
        if (file_browser->folder_view)
        {
            // force rebuild of folder_view for icon size change
            gtk_widget_destroy(file_browser->folder_view);
            file_browser->folder_view = nullptr;
        }
        file_browser->large_icons = large_icons;
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::LARGE_ICONS);
    }

    // List Styles
    if (xset_get_b_panel(p, "list_detailed"))
    {
        ptk_file_browser_view_as_list(file_browser);

        // Set column widths for this panel context
        GtkTreeViewColumn* col;
        int j, width;
        const char* title;
        XSet* set;

        if (GTK_IS_TREE_VIEW(file_browser->folder_view))
        {
            // LOG_INFO("    set widths   mode = {}", mode);
            int i;
            for (i = 0; i < 6; i++)
            {
                col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), i);
                if (!col)
                    break;
                title = gtk_tree_view_column_get_title(col);
                for (j = 0; j < 6; j++)
                {
                    if (ztd::same(title, column_titles.at(j)))
                        break;
                }
                if (j != 6)
                {
                    // get column width for this panel context
                    set = xset_get_panel_mode(p, column_names.at(j), mode);
                    width = set->y ? std::stol(set->y) : 100;
                    // LOG_INFO("        {}\t{}", width, title );
                    if (width)
                    {
                        gtk_tree_view_column_set_fixed_width(col, width);
                        // LOG_INFO("upd set_width {} {}", column_names.at(j), width);
                    }
                    // set column visibility
                    gtk_tree_view_column_set_visible(col, set->b == XSetB::XSET_B_TRUE || j == 0);
                }
            }
        }
    }
    else if (xset_get_b_panel(p, "list_icons"))
        ptk_file_browser_view_as_icons(file_browser);
    else if (xset_get_b_panel(p, "list_compact"))
        ptk_file_browser_view_as_compact_list(file_browser);
    else
    {
        xset_set_panel(p, "list_detailed", XSetSetSet::B, "1");
        ptk_file_browser_view_as_list(file_browser);
    }

    // Show Hidden
    ptk_file_browser_show_hidden_files(file_browser, xset_get_b_panel(p, "show_hidden"));

    // LOG_INFO("ptk_file_browser_update_views fb={:p} DONE", file_browser);
}

GtkWidget*
ptk_file_browser_new(int curpanel, GtkWidget* notebook, GtkWidget* task_view, void* main_window)
{
    PtkFBViewMode view_mode;
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_new(PTK_TYPE_FILE_BROWSER, nullptr));

    file_browser->mypanel = curpanel;
    file_browser->mynotebook = notebook;
    file_browser->main_window = main_window;
    file_browser->task_view = task_view;
    file_browser->sel_change_idle = 0;
    file_browser->inhibit_focus = file_browser->busy = false;
    file_browser->seek_name = nullptr;
    file_browser->book_set_name = nullptr;
    for (unsigned int i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
        file_browser->toolbar_widgets[i] = nullptr;

    if (xset_get_b_panel(curpanel, "list_detailed"))
        view_mode = PtkFBViewMode::PTK_FB_LIST_VIEW;
    else if (xset_get_b_panel(curpanel, "list_icons"))
    {
        view_mode = PtkFBViewMode::PTK_FB_ICON_VIEW;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
    }
    else if (xset_get_b_panel(curpanel, "list_compact"))
    {
        view_mode = PtkFBViewMode::PTK_FB_COMPACT_VIEW;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
    }
    else
    {
        xset_set_panel(curpanel, "list_detailed", XSetSetSet::B, "1");
        view_mode = PtkFBViewMode::PTK_FB_LIST_VIEW;
    }

    file_browser->view_mode = view_mode; // sfm was after next line
    // Large Icons - option for Detailed and Compact list views
    file_browser->large_icons =
        view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW ||
        xset_get_b_panel_mode(
            file_browser->mypanel,
            "list_large",
            (static_cast<FMMainWindow*>(main_window))->panel_context[file_browser->mypanel - 1]);
    file_browser->folder_view = create_folder_view(file_browser, view_mode);

    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);

    file_browser->side_dir = nullptr;
    file_browser->side_book = nullptr;
    file_browser->side_dev = nullptr;

    file_browser->select_path = nullptr;
    file_browser->status_bar_custom = nullptr;

    // gtk_widget_show_all( file_browser->folder_view_scroll );

    // set status bar icon
    char* icon_name;
    XSet* set = xset_get_panel(curpanel, "icon_status");
    if (set->icon && set->icon[0] != '\0')
        icon_name = set->icon;
    else
        icon_name = ztd::strdup("gtk-yes");
    gtk_image_set_from_icon_name(GTK_IMAGE(file_browser->status_image),
                                 icon_name,
                                 GTK_ICON_SIZE_MENU);

    gtk_widget_show_all(GTK_WIDGET(file_browser));

    return GTK_IS_WIDGET(file_browser) ? GTK_WIDGET(file_browser) : nullptr;
}

static void
ptk_file_browser_update_tab_label(PtkFileBrowser* file_browser)
{
    GtkWidget* label;
    GtkContainer* hbox;
    // GtkImage* icon;
    GtkLabel* text;
    GList* children;
    char* name;

    label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(file_browser->mynotebook),
                                       GTK_WIDGET(file_browser));
    hbox = GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(label)));
    children = gtk_container_get_children(hbox);
    // icon = GTK_IMAGE(children->data);
    text = GTK_LABEL(children->next->data);
    g_list_free(children);

    /* TODO: Change the icon */

    name = g_path_get_basename(ptk_file_browser_get_cwd(file_browser));
    gtk_label_set_text(text, name);
    gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_MIDDLE);
    if (std::strlen(name) < 30)
    {
        gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(text, -1);
    }
    else
        gtk_label_set_width_chars(text, 30);
    free(name);
}

void
ptk_file_browser_select_last(PtkFileBrowser* file_browser) // MOD added
{
    // LOG_INFO("ptk_file_browser_select_last");
    // select one file?
    if (file_browser->select_path)
    {
        ptk_file_browser_select_file(file_browser, file_browser->select_path);
        free(file_browser->select_path);
        file_browser->select_path = nullptr;
        return;
    }

    // select previously selected files
    int elementn = -1;
    GList* l;
    GList* element = nullptr;
    // LOG_INFO("    search for {}", (char*)file_browser->curHistory->data);

    if (file_browser->history && file_browser->histsel && file_browser->curHistory &&
        (l = g_list_last(file_browser->history)))
    {
        if (l->data && !strcmp((char*)l->data, (char*)file_browser->curHistory->data))
        {
            elementn = g_list_position(file_browser->history, l);
            if (elementn != -1)
            {
                element = g_list_nth(file_browser->histsel, elementn);
                // skip the current history item if sellist empty since it was just created
                if (!element->data)
                {
                    // LOG_INFO("        found current empty");
                    element = nullptr;
                }
                // else LOG_INFO("        found current NON-empty");
            }
        }
        if (!element)
        {
            while ((l = l->prev))
            {
                if (l->data && !strcmp((char*)l->data, (char*)file_browser->curHistory->data))
                {
                    elementn = g_list_position(file_browser->history, l);
                    // printf ("        found elementn=%d\n", elementn );
                    if (elementn != -1)
                        element = g_list_nth(file_browser->histsel, elementn);
                    break;
                }
            }
        }
    }

    /*
        if ( element )
        {
            g_debug ("element OK" );
            if ( element->data )
                g_debug ("element->data OK" );
            else
                g_debug ("element->data nullptr" );
        }
        else
            g_debug ("element nullptr" );
        g_debug ("histsellen=%d", g_list_length( file_browser->histsel ) );
    */
    if (element && element->data)
    {
        // LOG_INFO("    select files");
        PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
        GtkTreeSelection* tree_sel;
        bool firstsel = true;
        if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        for (l = (GList*)element->data; l; l = l->next)
        {
            if (l->data)
            {
                // g_debug ("find a file");
                GtkTreeIter it;
                GtkTreePath* tp;
                VFSFileInfo* file = static_cast<VFSFileInfo*>(l->data);
                if (ptk_file_list_find_iter(list, &it, file))
                {
                    // g_debug ("found file");
                    tp = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW ||
                        file_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), tp);
                        if (firstsel)
                        {
                            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                         tp,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    else if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
                    {
                        gtk_tree_selection_select_path(tree_sel, tp);
                        if (firstsel)
                        {
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                         tp,
                                                         nullptr,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    gtk_tree_path_free(tp);
                }
            }
        }
    }
}

bool
ptk_file_browser_chdir(PtkFileBrowser* file_browser, const char* folder_path, PtkFBChdirMode mode)
{
    bool cancel = false;
    GtkWidget* folder_view = file_browser->folder_view;
    // LOG_INFO("ptk_file_browser_chdir");

    bool inhibit_focus = file_browser->inhibit_focus;
    // file_browser->button_press = false;
    file_browser->is_drag = false;
    file_browser->menu_shown = false;
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW || app_settings.single_click)
        /* sfm 1.0.6 do not reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended
           breakage elsewhere. */
        file_browser->skip_release = false;

    if (!folder_path)
        return false;

    char* path;
    std::string msg;

    if (folder_path)
    {
        path = ztd::strdup(folder_path);
        /* remove redundent '/' */
        if (strcmp(path, "/"))
        {
            char* path_end = path + std::strlen(path) - 1;
            for (; path_end > path; --path_end)
            {
                if (*path_end != '/')
                    break;
                else
                    *path_end = '\0';
            }
        }

        // convert ~ to /home/user for smarter bookmarks
        if (Glib::str_has_prefix(path, "~/") || !g_strcmp0(path, "~"))
        {
            msg = fmt::format("{}{}", vfs_user_home_dir(), path + 1);
            path = ztd::strdup(msg);
        }
    }
    else
        path = nullptr;

    if (!path || !std::filesystem::is_directory(path))
    {
        if (!inhibit_focus)
        {
            msg = fmt::format("Directory does not exist\n\n{}", path);
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
            if (path)
                free(path);
        }
        return false;
    }

    if (!have_x_access(path))
    {
        if (!inhibit_focus)
        {
            std::string errno_msg = Glib::strerror(errno);
            msg = fmt::format("Unable to access {}\n\n{}", path, errno_msg);
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
        }
        return false;
    }

    g_signal_emit(file_browser,
                  signals[PTKFileBrowserSignal::BEFORE_CHDIR_SIGNAL],
                  0,
                  path,
                  &cancel);

    if (cancel)
        return false;

    // MOD remember selected files
    // g_debug ("@@@@@@@@@@@ remember: %s", ptk_file_browser_get_cwd( file_browser ) );
    if (file_browser->curhistsel && file_browser->curhistsel->data)
    {
        // g_debug ("free curhistsel");
        g_list_foreach((GList*)file_browser->curhistsel->data, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free((GList*)file_browser->curhistsel->data);
    }
    if (file_browser->curhistsel)
    {
        file_browser->curhistsel->data =
            vector_to_glist_VFSFileInfo(ptk_file_browser_get_selected_files(file_browser));

        // LOG_DEBUG("set curhistsel {}", g_list_position(file_browser->histsel,
        //                                    file_browser->curhistsel));
        // if ( file_browser->curhistsel->data )
        //    g_debug ("curhistsel->data OK" );
        // else
        //    g_debug ("curhistsel->data nullptr" );
    }

    switch (mode)
    {
        case PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY:
            if (!file_browser->curHistory || strcmp((char*)file_browser->curHistory->data, path))
            {
                /* Has forward history */
                if (file_browser->curHistory && file_browser->curHistory->next)
                {
                    /* clear old forward history */
                    g_list_foreach(file_browser->curHistory->next, (GFunc)free, nullptr);
                    g_list_free(file_browser->curHistory->next);
                    file_browser->curHistory->next = nullptr;
                }
                // MOD added - make histsel shadow file_browser->history
                if (file_browser->curhistsel && file_browser->curhistsel->next)
                {
                    // LOG_DEBUG("@@@@@@@@@@@ free forward");
                    GList* l;
                    for (l = file_browser->curhistsel->next; l; l = l->next)
                    {
                        if (l->data)
                        {
                            // LOG_DEBUG("free forward item");
                            g_list_foreach((GList*)l->data, (GFunc)vfs_file_info_unref, nullptr);
                            g_list_free((GList*)l->data);
                        }
                    }
                    g_list_free(file_browser->curhistsel->next);
                    file_browser->curhistsel->next = nullptr;
                }
                /* Add path to history if there is no forward history */
                file_browser->history = g_list_append(file_browser->history, path);
                file_browser->curHistory = g_list_last(file_browser->history);
                // MOD added - make histsel shadow file_browser->history
                GList* sellist = nullptr;
                file_browser->histsel = g_list_append(file_browser->histsel, sellist);
                file_browser->curhistsel = g_list_last(file_browser->histsel);
            }
            break;
        case PtkFBChdirMode::PTK_FB_CHDIR_BACK:
            file_browser->curHistory = file_browser->curHistory->prev;
            file_browser->curhistsel = file_browser->curhistsel->prev;
            break;
        case PtkFBChdirMode::PTK_FB_CHDIR_FORWARD:
            file_browser->curHistory = file_browser->curHistory->next;
            file_browser->curhistsel = file_browser->curhistsel->next;
            break;
        case PtkFBChdirMode::PTK_FB_CHDIR_NORMAL:
        case PtkFBChdirMode::PTK_FB_CHDIR_NO_HISTORY:
        default:
            break;
    }

    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_model(EXO_ICON_VIEW(folder_view), nullptr);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_set_model(GTK_TREE_VIEW(folder_view), nullptr);
            break;
        default:
            break;
    }

    // load new dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(path);

    if (!file_browser->curHistory || path != (char*)file_browser->curHistory->data)
        free(path);

    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::BEGIN_CHDIR_SIGNAL], 0);

    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser->dir, false, file_browser);
        file_browser->busy = false;
    }
    else
        file_browser->busy = true;

    g_signal_connect(file_browser->dir,
                     "file-listed",
                     G_CALLBACK(on_dir_file_listed),
                     file_browser);

    ptk_file_browser_update_tab_label(file_browser);

    const std::string disp_path =
        Glib::filename_display_name(ptk_file_browser_get_cwd(file_browser));
    if (!inhibit_focus)
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), disp_path.c_str());

    enable_toolbar(file_browser);
    return true;
}

static void
on_history_menu_item_activate(GtkWidget* menu_item, PtkFileBrowser* file_browser)
{
    GList* l = (GList*)g_object_get_data(G_OBJECT(menu_item), "path");
    GList* tmp = file_browser->curHistory;
    file_browser->curHistory = l;

    if (!ptk_file_browser_chdir(file_browser,
                                (char*)l->data,
                                PtkFBChdirMode::PTK_FB_CHDIR_NO_HISTORY))
        file_browser->curHistory = tmp;
    else
    {
        // MOD sync curhistsel
        int elementn = -1;
        elementn = g_list_position(file_browser->history, file_browser->curHistory);
        if (elementn != -1)
            file_browser->curhistsel = g_list_nth(file_browser->histsel, elementn);
        else
            LOG_DEBUG("missing history item - ptk-file-browser.cxx");
    }
}

static GtkWidget*
add_history_menu_item(PtkFileBrowser* file_browser, GtkWidget* menu, GList* l)
{
    GtkWidget* menu_item;
    // GtkWidget* folder_image;
    char* disp_name;
    disp_name = g_filename_display_basename((char*)l->data);
    menu_item = gtk_menu_item_new_with_label(disp_name);
    g_object_set_data(G_OBJECT(menu_item), "path", l);
    // folder_image = gtk_image_new_from_icon_name("gnome-fs-directory", GTK_ICON_SIZE_MENU);
    g_signal_connect(menu_item,
                     "activate",
                     G_CALLBACK(on_history_menu_item_activate),
                     file_browser);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    return menu_item;
}

void
ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                   GdkEventButton* event)
{
    (void)event;
    GtkWidget* menu = gtk_menu_new();
    GList* l;
    bool has_items = false;

    if (is_back_history)
    {
        // back history
        for (l = file_browser->curHistory->prev; l != nullptr; l = l->prev)
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
            if (!has_items)
                has_items = true;
        }
    }
    else
    {
        // forward history
        for (l = file_browser->curHistory->next; l != nullptr; l = l->next)
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
            if (!has_items)
                has_items = true;
        }
    }
    if (has_items)
    {
        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    }
    else
        gtk_widget_destroy(menu);
}

static bool
ptk_file_browser_content_changed(PtkFileBrowser* file_browser)
{
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::CONTENT_CHANGE_SIGNAL], 0);
    return false;
}

static void
on_folder_content_changed(VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser)
{
    (void)dir;
    if (file == nullptr)
    {
        // The current directory itself changed
        if (!std::filesystem::is_directory(ptk_file_browser_get_cwd(file_browser)))
            // current directory does not exist - was renamed
            on_close_notebook_page(nullptr, file_browser);
    }
    else
        g_idle_add((GSourceFunc)ptk_file_browser_content_changed, file_browser);
}

static void
on_file_deleted(VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser)
{
    if (file == nullptr)
    {
        // The directory itself was deleted
        on_close_notebook_page(nullptr, file_browser);
        // ptk_file_browser_chdir( file_browser, vfs_user_home_dir(),
        // PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    }
    else
    {
        on_folder_content_changed(dir, file, file_browser);
    }
}

static void
on_sort_col_changed(GtkTreeSortable* sortable, PtkFileBrowser* file_browser)
{
    int col;

    gtk_tree_sortable_get_sort_column_id(sortable, &col, &file_browser->sort_type);

    switch (col)
    {
        case PTKFileListCol::COL_FILE_NAME:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_NAME;
            break;
        case PTKFileListCol::COL_FILE_SIZE:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_SIZE;
            break;
        case PTKFileListCol::COL_FILE_MTIME:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_MTIME;
            break;
        case PTKFileListCol::COL_FILE_DESC:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_TYPE;
            break;
        case PTKFileListCol::COL_FILE_PERM:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_PERM;
            break;
        case PTKFileListCol::COL_FILE_OWNER:
            col = PtkFBSortOrder::PTK_FB_SORT_BY_OWNER;
            break;
        default:
            break;
    }
    file_browser->sort_order = (PtkFBSortOrder)col;
    // MOD enable following to make column click permanent sort
    //    app_settings.sort_order = col;
    //    if ( file_browser )
    //        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ),
    //        app_settings.sort_order );

    xset_set_panel(file_browser->mypanel, "list_detailed", XSetSetSet::X, std::to_string(col));
    xset_set_panel(file_browser->mypanel,
                   "list_detailed",
                   XSetSetSet::Y,
                   std::to_string(file_browser->sort_type));
}

static void
ptk_file_browser_update_model(PtkFileBrowser* file_browser)
{
    PtkFileList* list = ptk_file_list_new(file_browser->dir, file_browser->show_hidden_files);
    GtkTreeModel* old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL(list);
    if (old_list)
        g_object_unref(G_OBJECT(old_list));

    ptk_file_browser_read_sort_extra(file_browser); // sfm
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                         file_list_order_from_sort_order(file_browser->sort_order),
                                         file_browser->sort_type);

    show_thumbnails(file_browser, list, file_browser->large_icons, file_browser->max_thumbnail);
    g_signal_connect(list, "sort-column-changed", G_CALLBACK(on_sort_col_changed), file_browser);

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
        default:
            break;
    }

    // try to smooth list bounce created by delayed re-appearance of column headers
    // while( gtk_events_pending() )
    //    gtk_main_iteration();
}

static void
on_dir_file_listed(VFSDir* dir, bool is_cancelled, PtkFileBrowser* file_browser)
{
    file_browser->n_sel_files = 0;

    if (!is_cancelled)
    {
        g_signal_connect(dir, "file-created", G_CALLBACK(on_folder_content_changed), file_browser);
        g_signal_connect(dir, "file-deleted", G_CALLBACK(on_file_deleted), file_browser);
        g_signal_connect(dir, "file-changed", G_CALLBACK(on_folder_content_changed), file_browser);
    }

    ptk_file_browser_update_model(file_browser);
    file_browser->busy = false;

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that changing the directory results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::AFTER_CHDIR_SIGNAL], 0);
    // g_signal_emit( file_browser, signals[ PTKFileBrowserSignal::CONTENT_CHANGE_SIGNAL ], 0 );
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::SEL_CHANGE_SIGNAL], 0);

    if (file_browser->side_dir)
        ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                ptk_file_browser_get_cwd(file_browser));

    /*
        if ( file_browser->side_pane )
        if ( ptk_file_browser_is_side_pane_visible( file_browser ) )
        {
            side_pane_chdir( file_browser,
                             ptk_file_browser_get_cwd( file_browser ) );
        }
    */
    if (file_browser->side_dev)
        ptk_location_view_chdir(GTK_TREE_VIEW(file_browser->side_dev),
                                ptk_file_browser_get_cwd(file_browser));
    if (file_browser->side_book)
        ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, true);

    // FIXME:  This is already done in update_model, but is there any better way to
    //            reduce unnecessary code?
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
    { // sfm why is this needed for compact view???
        if (!is_cancelled && file_browser->file_list)
        {
            show_thumbnails(file_browser,
                            PTK_FILE_LIST(file_browser->file_list),
                            file_browser->large_icons,
                            file_browser->max_thumbnail);
        }
    }
}

void
ptk_file_browser_canon(PtkFileBrowser* file_browser, const char* path)
{
    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    char buf[PATH_MAX + 1];
    char* canon = realpath(path, buf);
    if (!canon || !g_strcmp0(canon, cwd) || !g_strcmp0(canon, path))
        return;

    if (std::filesystem::is_directory(canon))
    {
        // open dir
        ptk_file_browser_chdir(file_browser, canon, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
    else if (std::filesystem::exists(canon))
    {
        // open dir and select file
        char* dir_path = g_path_get_dirname(canon);
        if (dir_path && strcmp(dir_path, cwd))
        {
            free(file_browser->select_path);
            file_browser->select_path = ztd::strdup(canon);
            ptk_file_browser_chdir(file_browser,
                                   dir_path,
                                   PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
        }
        else
            ptk_file_browser_select_file(file_browser, canon);
        free(dir_path);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
}

const char*
ptk_file_browser_get_cwd(PtkFileBrowser* file_browser)
{
    if (!file_browser->curHistory)
        return nullptr;
    return (const char*)file_browser->curHistory->data;
}

void
ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    /* there is no back history */
    if (!file_browser->curHistory || !file_browser->curHistory->prev)
        return;
    const char* path = (const char*)file_browser->curHistory->prev->data;
    ptk_file_browser_chdir(file_browser, path, PtkFBChdirMode::PTK_FB_CHDIR_BACK);
}

void
ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    /* If there is no forward history */
    if (!file_browser->curHistory || !file_browser->curHistory->next)
        return;
    const char* path = (const char*)file_browser->curHistory->next->data;
    ptk_file_browser_chdir(file_browser, path, PtkFBChdirMode::PTK_FB_CHDIR_FORWARD);
}

void
ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    char* parent_dir = g_path_get_dirname(ptk_file_browser_get_cwd(file_browser));
    if (strcmp(parent_dir, ptk_file_browser_get_cwd(file_browser)))
        ptk_file_browser_chdir(file_browser, parent_dir, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    free(parent_dir);
}

void
ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                           vfs_user_home_dir().c_str(),
                           PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
}

void
ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    const char* path = xset_get_s(XSetName::GO_SET_DEFAULT);
    if (path && path[0] != '\0')
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                               path,
                               PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    else if (geteuid() != 0)
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                               vfs_user_home_dir().c_str(),
                               PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    else
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                               "/",
                               PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
}

void
ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    xset_set(XSetName::GO_SET_DEFAULT, XSetSetSet::S, ptk_file_browser_get_cwd(file_browser));
}

void
ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_select_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_select_all(tree_sel);
            break;
        default:
            break;
    }
}

void
ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_unselect_all(tree_sel);
            break;
        default:
            break;
    }
}

static bool
invert_selection(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* it,
                 PtkFileBrowser* file_browser)
{
    (void)model;
    (void)it;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view), path))
                exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            else
                exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            if (gtk_tree_selection_path_is_selected(tree_sel, path))
                gtk_tree_selection_unselect_path(tree_sel, path);
            else
                gtk_tree_selection_select_path(tree_sel, path);
            break;
        default:
            break;
    }
    return false;
}

void
ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
        default:
            break;
    }
}

void
ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                const char* search_key)
{
    (void)item;
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    VFSFileInfo* file;
    const char* key;

    if (search_key)
        key = search_key;
    else
    {
        // get pattern from user  (store in ob1 so it is not saved)
        XSet* set = xset_get(XSetName::SELECT_PATT);
        if (!xset_text_dialog(
                GTK_WIDGET(file_browser),
                "Select By Pattern",
                "Enter pattern to select files and directories:\n\nIf your pattern contains any "
                "uppercase characters, the matching will be case sensitive.\n\nExample:  "
                "*sp*e?m*\n\nTIP: You can also enter '%% PATTERN' in the path bar.",
                "",
                set->ob1,
                &set->ob1,
                "",
                false) ||
            !set->ob1)
            return;
        key = set->ob1;
    }

    // case insensitive search ?
    bool icase = false;
    char* lower_key = g_utf8_strdown(key, -1);
    if (!strcmp(lower_key, key))
    {
        // key is all lowercase so do icase search
        icase = true;
    }
    free(lower_key);

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    // test rows
    bool first_select = true;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            if (icase)
                name = g_utf8_strdown(name, -1);

            bool select = fnmatch(key, name, 0) == 0;

            if (icase)
                free(name);

            // do selection and scroll to first selected
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)),
                                           &it);

            switch (file_browser->view_mode)
            {
                case PtkFBViewMode::PTK_FB_ICON_VIEW:
                case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                    }
                    else if (select)
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case PtkFBViewMode::PTK_FB_LIST_VIEW:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                            gtk_tree_selection_unselect_path(tree_sel, path);
                    }
                    else if (select)
                        gtk_tree_selection_select_path(tree_sel, path);

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                default:
                    break;
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
        default:
            break;
    }
    focus_folder_view(file_browser);
}

void
ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename, bool do_select)
{
    // If do_select, select all filenames, unselect others
    // if !do_select, unselect filenames, leave others unchanged
    // If !*filename select or unselect all
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    if (!filename || !*filename)
    {
        if (do_select)
            ptk_file_browser_select_all(nullptr, file_browser);
        else
            ptk_file_browser_unselect_all(nullptr, file_browser);
        return;
    }

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    // test rows
    bool first_select = true;
    GtkTreeIter it;
    VFSFileInfo* file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        bool select;
        do
        {
            // get file
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            char** test_name = filename;
            while (*test_name)
            {
                if (!strcmp(*test_name, name))
                    break;
                test_name++;
            }
            if (*test_name)
                select = do_select;
            else
                select = !do_select;

            // do selection and scroll to first selected
            GtkTreePath* path =
                gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)),
                                        &it);

            switch (file_browser->view_mode)
            {
                case PtkFBViewMode::PTK_FB_ICON_VIEW:
                case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                    }
                    else if (select && do_select)
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case PtkFBViewMode::PTK_FB_LIST_VIEW:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                            gtk_tree_selection_unselect_path(tree_sel, path);
                    }
                    else if (select && do_select)
                        gtk_tree_selection_select_path(tree_sel, path);

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                default:
                    break;
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
        default:
            break;
    }
    focus_folder_view(file_browser);
}

static void
ptk_file_browser_restore_sig(PtkFileBrowser* file_browser, GtkTreeSelection* tree_sel)
{
    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(tree_sel), file_browser);
            break;
        default:
            break;
    }
}

void
ptk_file_browser_seek_path(PtkFileBrowser* file_browser, const char* seek_dir,
                           const char* seek_name)
{
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const char* cwd = ptk_file_browser_get_cwd(file_browser);

    if (seek_dir && g_strcmp0(cwd, seek_dir))
    {
        // change dir
        free(file_browser->seek_name);
        file_browser->seek_name = ztd::strdup(seek_name);
        file_browser->inhibit_focus = true;
        if (!ptk_file_browser_chdir(file_browser,
                                    seek_dir,
                                    PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY))
        {
            file_browser->inhibit_focus = false;
            free(file_browser->seek_name);
            file_browser->seek_name = nullptr;
        }
        // return here to allow dir to load
        // finishes seek in main-window.c on_file_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_file_browser_after_chdir()
    // select seek name
    ptk_file_browser_unselect_all(nullptr, file_browser);

    if (!(seek_name && seek_name[0]))
        return;

    // get model, treesel, and stop signals
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (!GTK_IS_TREE_MODEL(model))
    {
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    // test rows - give preference to matching dir, else match file
    GtkTreeIter it_file;
    GtkTreeIter it_dir;
    it_file.stamp = 0;
    it_dir.stamp = 0;
    VFSFileInfo* file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            if (!g_strcmp0(name, seek_name))
            {
                // exact match (may be file or dir)
                it_dir = it;
                break;
            }
            if (Glib::str_has_prefix(name, seek_name))
            {
                // prefix found
                if (vfs_file_info_is_dir(file))
                {
                    if (!it_dir.stamp)
                        it_dir = it;
                }
                else if (!it_file.stamp)
                    it_file = it;
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (it_dir.stamp)
        it = it_dir;
    else
        it = it_file;
    if (!it.stamp)
    {
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    // do selection and scroll to selected
    GtkTreePath* path;
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)), &it);
    if (!path)
    {
        ptk_file_browser_restore_sig(file_browser, tree_sel);
        return;
    }

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            // select
            exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

            // scroll and set cursor
            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                         path,
                                         true,
                                         .25,
                                         0);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            // select
            gtk_tree_selection_select_path(tree_sel, path);

            // scroll and set cursor
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                         path,
                                         nullptr,
                                         true,
                                         .25,
                                         0);
            break;
        default:
            break;
    }
    gtk_tree_path_free(path);

    ptk_file_browser_restore_sig(file_browser, tree_sel);
}

/* signal handlers */

static void
on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                              PtkFileBrowser* file_browser)
{
    (void)iconview;
    (void)path;
    ptk_file_browser_open_selected_files(file_browser);
}

static void
on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                             PtkFileBrowser* file_browser)
{
    (void)tree_view;
    (void)path;
    (void)col;
    // file_browser->button_press = false;
    ptk_file_browser_open_selected_files(file_browser);
}

static bool
on_folder_view_item_sel_change_idle(PtkFileBrowser* file_browser)
{
    if (!GTK_IS_WIDGET(file_browser))
        return false;

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;

    GtkTreeModel* model;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);

    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        VFSFileInfo* file;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data))
        {
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (file)
            {
                file_browser->sel_size += vfs_file_info_get_size(file);
                vfs_file_info_unref(file);
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);

    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::SEL_CHANGE_SIGNAL], 0);
    file_browser->sel_change_idle = 0;
    return false;
}

static void
on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser)
{
    (void)iconview;
    /* //sfm on_folder_view_item_sel_change fires for each selected file
     * when a file is clicked - causes hang if thousands of files are selected
     * So add only one g_idle_add at a time
     */
    if (file_browser->sel_change_idle)
        return;

    file_browser->sel_change_idle =
        g_idle_add((GSourceFunc)on_folder_view_item_sel_change_idle, file_browser);
}

static void
show_popup_menu(PtkFileBrowser* file_browser, GdkEventButton* event)
{
    (void)event;
    std::string file_path;
    VFSFileInfo* file;

    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    std::vector<VFSFileInfo*> sel_files;

    sel_files = ptk_file_browser_get_selected_files(file_browser);
    if (sel_files.empty())
    {
        file = nullptr;
    }
    else
    {
        file = vfs_file_info_ref(sel_files.front());
        file_path = Glib::build_filename(cwd, vfs_file_info_get_name(file));
    }

    /*
    int button;
    std::uint32_t time;
    if (event)
    {
        button = event->button;
        time = event->time;
    }
    else
    {
        button = 0;
        time = gtk_get_current_event_time();
    }
    */

    char* dir_name = nullptr;
    GtkWidget* popup = ptk_file_menu_new(file_browser,
                                         file_path.c_str(),
                                         file,
                                         dir_name ? dir_name : cwd,
                                         sel_files);
    if (popup)
        gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    if (file)
        vfs_file_info_unref(file);

    if (dir_name)
        free(dir_name);
}

/* invoke popup menu via shortcut key */
static bool
on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    (void)widget;
    show_popup_menu(file_browser, nullptr);
    return true;
}

static bool
on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                  PtkFileBrowser* file_browser)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeSelection* tree_sel;
    bool ret = false;

    if (file_browser->menu_shown)
        file_browser->menu_shown = false;

    if (event->type == GDK_BUTTON_PRESS)
    {
        focus_folder_view(file_browser);
        // file_browser->button_press = true;

        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              XSetName::EVT_WIN_CLICK,
                              0,
                              0,
                              "filelist",
                              0,
                              event->button,
                              event->state,
                              true))
        {
            file_browser->skip_release = true;
            return true;
        }

        if (event->button == 4 || event->button == 5 || event->button == 8 ||
            event->button == 9) // sfm
        {
            if (event->button == 4 || event->button == 8)
                ptk_file_browser_go_back(nullptr, file_browser);
            else
                ptk_file_browser_go_forward(nullptr, file_browser);
            return true;
        }

        // Alt - Left/Right Click
        if (((event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) ==
             GDK_MOD1_MASK) &&
            (event->button == 1 || event->button == 3)) // sfm
        {
            if (event->button == 1)
                ptk_file_browser_go_back(nullptr, file_browser);
            else
                ptk_file_browser_go_forward(nullptr, file_browser);
            return true;
        }

        switch (file_browser->view_mode)
        {
            case PtkFBViewMode::PTK_FB_ICON_VIEW:
            case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
                tree_path =
                    exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
                model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));

                /* deselect selected files when right click on blank area */
                if (!tree_path && event->button == 3)
                    exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                break;
            case PtkFBViewMode::PTK_FB_LIST_VIEW:
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              event->x,
                                              event->y,
                                              &tree_path,
                                              &col,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

                if (col &&
                    gtk_tree_view_column_get_sort_column_id(col) != PTKFileListCol::COL_FILE_NAME &&
                    tree_path) // MOD
                {
                    gtk_tree_path_free(tree_path);
                    tree_path = nullptr;
                }
                break;
            default:
                break;
        }

        /* an item is clicked, get its file path */
        VFSFileInfo* file;
        GtkTreeIter it;
        std::string file_path;
        if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            file_path = Glib::build_filename(ptk_file_browser_get_cwd(file_browser),
                                             vfs_file_info_get_name(file));
        }
        else /* no item is clicked */
        {
            file = nullptr;
        }

        /* middle button */
        if (event->button == 2 && !file_path.empty()) /* middle click on a item */
        {
            /* open in new tab if its a directory */
            if (std::filesystem::is_directory(file_path))
            {
                g_signal_emit(file_browser,
                              signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                              0,
                              file_path.c_str(),
                              PtkOpenAction::PTK_OPEN_NEW_TAB);
            }
            ret = true;
        }
        else if (event->button == 3) /* right click */
        {
            /* cancel all selection, and select the item if it is not selected */
            switch (file_browser->view_mode)
            {
                case PtkFBViewMode::PTK_FB_ICON_VIEW:
                case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
                    if (tree_path &&
                        !exo_icon_view_path_is_selected(EXO_ICON_VIEW(widget), tree_path))
                    {
                        exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                        exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
                    }
                    break;
                case PtkFBViewMode::PTK_FB_LIST_VIEW:
                    if (tree_path && !gtk_tree_selection_path_is_selected(tree_sel, tree_path))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                    }
                    break;
                default:
                    break;
            }

            show_popup_menu(file_browser, event);
            /* FIXME if approx 5000 are selected, right-click sometimes unselects all
             * after this button_press function returns - why?  a gtk or exo bug?
             * Always happens with above show_popup_menu call disabled
             * Only when this occurs, cursor is automatically set to current row and
             * treesel 'changed' signal fires
             * Stopping changed signal had no effect
             * Using connect rather than connect_after had no effect
             * Removing signal connect had no effect
             * FIX: inhibit button release */
            ret = file_browser->menu_shown = true;
        }
        if (file)
            vfs_file_info_unref(file);
        gtk_tree_path_free(tree_path);
    }
    else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        // double click event -  button = 0
        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              XSetName::EVT_WIN_CLICK,
                              0,
                              0,
                              "filelist",
                              0,
                              0,
                              event->state,
                              true))
            return true;

        if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
            /* set ret true to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GDK_2BUTTON_PRESS so use
             * file_browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = true;
        else if (!app_settings.single_click)
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * ptk_file_browser_chdir(). See also
             * on_folder_view_button_release_event() */
            file_browser->skip_release = true;
    }
    return ret;
}

static bool
on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
                                    PtkFileBrowser* file_browser) // sfm
{ // on left-click release on file, if not dnd or rubberbanding, unselect files
    (void)widget;
    GtkTreePath* tree_path = nullptr;

    if (file_browser->is_drag || event->button != 1 || file_browser->skip_release ||
        (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
    {
        if (file_browser->skip_release)
            file_browser->skip_release = false;
        // this fixes bug where right-click shows menu and release unselects files
        bool ret = file_browser->menu_shown && event->button != 1;
        if (file_browser->menu_shown)
            file_browser->menu_shown = false;
        return ret;
    }

#if 0
    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            //if (exo_icon_view_is_rubber_banding_active(EXO_ICON_VIEW(widget)))
            //    return false;
            /* Conditional on single_click below was removed 1.0.2 708f0988 bc it
             * caused a left-click to not unselect other files.  However, this
             * caused file under cursor to be selected when entering directory by
             * double-click in Icon/Compact styles.  To correct this, 1.0.6
             * conditionally sets skip_release on GDK_2BUTTON_PRESS, and does not
             * reset skip_release in ptk_file_browser_chdir(). */
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            if (tree_path)
            {
                // unselect all but one file
                exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
            }
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            if (gtk_tree_view_is_rubber_banding_active(GTK_TREE_VIEW(widget)))
                return false;
            if (app_settings.single_click)
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              event->x,
                                              event->y,
                                              &tree_path,
                                              nullptr,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
                if (tree_path && tree_sel && gtk_tree_selection_count_selected_rows(tree_sel) > 1)
                {
                    // unselect all but one file
                    gtk_tree_selection_unselect_all(tree_sel);
                    gtk_tree_selection_select_path(tree_sel, tree_path);
                }
            }
            break;
        default:
            break;
    }
#endif

    gtk_tree_path_free(tree_path);
    return false;
}

static bool
on_dir_tree_update_sel(PtkFileBrowser* file_browser)
{
    if (!file_browser->side_dir)
        return false;
    char* dir_path = ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(file_browser->side_dir));

    if (dir_path)
    {
        if (strcmp(dir_path, ptk_file_browser_get_cwd(file_browser)))
        {
            if (ptk_file_browser_chdir(file_browser,
                                       dir_path,
                                       PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY))
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), dir_path);
        }
        free(dir_path);
    }
    return false;
}

void
on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                          PtkFileBrowser* file_browser)
{
    (void)view;
    (void)path;
    (void)column;
    g_idle_add((GSourceFunc)on_dir_tree_update_sel, file_browser);
}

void
ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    const char* dir_path;

    focus_folder_view(file_browser);
    if (xset_get_s(XSetName::GO_SET_DEFAULT))
        dir_path = xset_get_s(XSetName::GO_SET_DEFAULT);
    else
        dir_path = vfs_user_home_dir().c_str();

    if (!std::filesystem::is_directory(dir_path))
        g_signal_emit(file_browser,
                      signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                      0,
                      "/",
                      PtkOpenAction::PTK_OPEN_NEW_TAB);
    else
    {
        g_signal_emit(file_browser,
                      signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                      0,
                      dir_path,
                      PtkOpenAction::PTK_OPEN_NEW_TAB);
    }
}

void
ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    focus_folder_view(file_browser);
    const char* dir_path = ptk_file_browser_get_cwd(file_browser);
    if (!std::filesystem::is_directory(dir_path))
    {
        if (xset_get_s(XSetName::GO_SET_DEFAULT))
            dir_path = xset_get_s(XSetName::GO_SET_DEFAULT);
        else
            dir_path = vfs_user_home_dir().c_str();
    }
    if (!std::filesystem::is_directory(dir_path))
        g_signal_emit(file_browser,
                      signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                      0,
                      "/",
                      PtkOpenAction::PTK_OPEN_NEW_TAB);
    else
    {
        g_signal_emit(file_browser,
                      signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                      0,
                      dir_path,
                      PtkOpenAction::PTK_OPEN_NEW_TAB);
    }
}

void
ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
        return;

    if (file_browser->view_mode != PtkFBViewMode::PTK_FB_LIST_VIEW)
        return;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);

    // if the window was opened maximized and stayed maximized, or the window is
    // unmaximized and not fullscreen, save the columns
    if ((!main_window->maximized || main_window->opened_maximized) && !main_window->fullscreen)
    {
        int p = file_browser->mypanel;
        char mode = main_window->panel_context[p - 1];
        // LOG_INFO("save_columns  fb={:p} (panel {})  mode = {}", fmt::ptr(file_browser), p, mode);
        int i;
        for (i = 0; i < 6; i++)
        {
            GtkTreeViewColumn* col = gtk_tree_view_get_column(view, i);
            if (!col)
                return;
            const char* title = gtk_tree_view_column_get_title(col);
            int j;
            for (j = 0; j < 6; j++)
            {
                if (ztd::same(title, column_titles.at(j)))
                    break;
            }
            if (j != 6)
            {
                // save column width for this panel context
                XSet* set = xset_get_panel_mode(p, column_names.at(j), mode);
                int width = gtk_tree_view_column_get_width(col);
                if (width > 0)
                {
                    free(set->y);
                    set->y = ztd::strdup(width);
                    // LOG_INFO("        {}\t{}", width, title);
                }
            }
        }
    }
}

static void
on_folder_view_columns_changed(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    // user dragged a column to a different position - save positions
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
        return;

    if (file_browser->view_mode != PtkFBViewMode::PTK_FB_LIST_VIEW)
        return;

    int i;
    for (i = 0; i < 6; i++)
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, i);
        if (!col)
            return;
        const char* title = gtk_tree_view_column_get_title(col);
        int j;
        for (j = 0; j < 6; j++)
        {
            if (ztd::same(title, column_titles.at(j)))
                break;
        }
        if (j != 6)
        {
            // save column position
            XSet* set = xset_get_panel(file_browser->mypanel, column_names.at(j));
            free(set->x);
            set->x = ztd::strdup(i);
        }
    }
}

static void
on_folder_view_destroy(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    (void)file_browser;
    unsigned int id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        unsigned long hand =
            g_signal_handler_find((void*)view, G_SIGNAL_MATCH_ID, id, 0, nullptr, nullptr, nullptr);
        if (hand)
            g_signal_handler_disconnect((void*)view, hand);
    }
}

static bool
folder_view_search_equal(GtkTreeModel* model, int col, const char* key, GtkTreeIter* it,
                         void* search_data)
{
    (void)search_data;
    char* name;
    char* lower_name = nullptr;
    bool no_match;

    if (col != PTKFileListCol::COL_FILE_NAME)
        return true;

    gtk_tree_model_get(model, it, col, &name, -1);

    if (!name || !key)
        return true;

    char* lower_key = g_utf8_strdown(key, -1);
    if (!strcmp(lower_key, key))
    {
        // key is all lowercase so do icase search
        lower_name = g_utf8_strdown(name, -1);
        name = lower_name;
    }

    if (strchr(key, '*') || strchr(key, '?'))
    {
        std::string key2 = fmt::format("*{}*", key);
        no_match = fnmatch(key2.c_str(), name, 0) != 0;
    }
    else
    {
        bool end = Glib::str_has_suffix(key, "$");
        bool start = !end && (std::strlen(key) < 3);
        char* key2 = ztd::strdup(key);
        char* keyp = key2;
        if (key[0] == '^')
        {
            keyp++;
            start = true;
        }
        if (end)
            key2[std::strlen(key2) - 1] = '\0';
        if (start && end)
            no_match = !strstr(name, keyp);
        else if (start)
            no_match = !Glib::str_has_prefix(name, keyp);
        else if (end)
            no_match = !Glib::str_has_suffix(name, keyp);
        else
            no_match = !strstr(name, key);
        free(key2);
    }
    free(lower_name);
    free(lower_key);
    return no_match; // return false for match
}

static GtkWidget*
create_folder_view(PtkFileBrowser* file_browser, PtkFBViewMode view_mode)
{
    GtkWidget* folder_view = nullptr;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;
    int big_icon_size, small_icon_size, icon_size = 0;

    PangoAttrList* attr_list;
    attr_list = pango_attr_list_new();
#if PANGO_VERSION_CHECK(1, 44, 0)
    pango_attr_list_insert(attr_list, pango_attr_insert_hyphens_new(false));
#endif

    vfs_mime_type_get_icon_size(&big_icon_size, &small_icon_size);

    switch (view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            folder_view = exo_icon_view_new();

            if (view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
            {
                icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

                exo_icon_view_set_layout_mode(EXO_ICON_VIEW(folder_view),
                                              EXO_ICON_VIEW_LAYOUT_COLS);
                exo_icon_view_set_orientation(EXO_ICON_VIEW(folder_view),
                                              GTK_ORIENTATION_HORIZONTAL);
            }
            else
            {
                icon_size = big_icon_size;

                exo_icon_view_set_column_spacing(EXO_ICON_VIEW(folder_view), 4);
                exo_icon_view_set_item_width(EXO_ICON_VIEW(folder_view),
                                             icon_size < 110 ? 110 : icon_size);
            }

            exo_icon_view_set_selection_mode(EXO_ICON_VIEW(folder_view), GTK_SELECTION_MULTIPLE);

            // search
            exo_icon_view_set_enable_search(EXO_ICON_VIEW(folder_view), true);
            exo_icon_view_set_search_column(EXO_ICON_VIEW(folder_view),
                                            PTKFileListCol::COL_FILE_NAME);
            exo_icon_view_set_search_equal_func(
                EXO_ICON_VIEW(folder_view),
                (ExoIconViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            exo_icon_view_set_single_click(EXO_ICON_VIEW(folder_view), file_browser->single_click);
            exo_icon_view_set_single_click_timeout(
                EXO_ICON_VIEW(folder_view),
                app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT);

            gtk_cell_layout_clear(GTK_CELL_LAYOUT(folder_view));

            file_browser->icon_render = renderer = gtk_cell_renderer_pixbuf_new();

            /* add the icon renderer */
            g_object_set(G_OBJECT(renderer), "follow_state", true, nullptr);
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, false);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(folder_view),
                                          renderer,
                                          "pixbuf",
                                          file_browser->large_icons
                                              ? PTKFileListCol::COL_FILE_BIG_ICON
                                              : PTKFileListCol::COL_FILE_SMALL_ICON);

            /* add the name renderer */
            renderer = gtk_cell_renderer_text_new();

            if (view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
            {
                g_object_set(G_OBJECT(renderer),
                             "xalign",
                             0.0,
                             "yalign",
                             0.5,
                             "font",
                             config_settings.font_view_compact,
                             "size-set",
                             true,
                             nullptr);
            }
            else
            {
                g_object_set(G_OBJECT(renderer),
                             "alignment",
                             PANGO_ALIGN_CENTER,
                             "wrap-mode",
                             PANGO_WRAP_WORD_CHAR,
                             "wrap-width",
                             105, // FIXME prob shouldnt hard code this
                                  // icon_size < 105 ? 109 : icon_size, // breaks with cjk text
                             "xalign",
                             0.5,
                             "yalign",
                             0.0,
                             "attributes",
                             attr_list,
                             "font",
                             config_settings.font_view_icon,
                             "size-set",
                             true,
                             nullptr);
            }
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, true);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(folder_view),
                                          renderer,
                                          "text",
                                          PTKFileListCol::COL_FILE_NAME);

            exo_icon_view_enable_model_drag_source(
                EXO_ICON_VIEW(folder_view),
                GdkModifierType(GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                GDK_ACTION_ALL);

            exo_icon_view_enable_model_drag_dest(EXO_ICON_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 GDK_ACTION_ALL);

            g_signal_connect((void*)folder_view,
                             "item-activated",
                             G_CALLBACK(on_folder_view_item_activated),
                             file_browser);

            g_signal_connect_after((void*)folder_view,
                                   "selection-changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);

            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            folder_view = gtk_tree_view_new();

            init_list_view(file_browser, GTK_TREE_VIEW(folder_view));

            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(folder_view));
            gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_MULTIPLE);

            if (xset_get_b(XSetName::RUBBERBAND))
                gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(folder_view), true);

            // Search
            gtk_tree_view_set_enable_search(GTK_TREE_VIEW(folder_view), true);
            gtk_tree_view_set_search_column(GTK_TREE_VIEW(folder_view),
                                            PTKFileListCol::COL_FILE_NAME);
            gtk_tree_view_set_search_equal_func(
                GTK_TREE_VIEW(folder_view),
                (GtkTreeViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            // gtk_tree_view_set_single_click(GTK_TREE_VIEW(folder_view),
            // file_browser->single_click); gtk_tree_view_set_single_click_timeout(
            //    GTK_TREE_VIEW(folder_view),
            //    app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT);

            icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

            gtk_tree_view_enable_model_drag_source(
                GTK_TREE_VIEW(folder_view),
                GdkModifierType(GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                GDK_ACTION_ALL);

            gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 GDK_ACTION_ALL);

            g_signal_connect((void*)folder_view,
                             "row_activated",
                             G_CALLBACK(on_folder_view_row_activated),
                             file_browser);

            g_signal_connect_after((void*)tree_sel,
                                   "changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);
            // MOD
            g_signal_connect((void*)folder_view,
                             "columns-changed",
                             G_CALLBACK(on_folder_view_columns_changed),
                             file_browser);
            g_signal_connect((void*)folder_view,
                             "destroy",
                             G_CALLBACK(on_folder_view_destroy),
                             file_browser);
            break;
        default:
            break;
    }

    gtk_cell_renderer_set_fixed_size(file_browser->icon_render, icon_size, icon_size);

    g_signal_connect((void*)folder_view,
                     "button-press-event",
                     G_CALLBACK(on_folder_view_button_press_event),
                     file_browser);
    g_signal_connect((void*)folder_view,
                     "button-release-event",
                     G_CALLBACK(on_folder_view_button_release_event),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "popup-menu",
                     G_CALLBACK(on_folder_view_popup_menu),
                     file_browser);

    /* init drag & drop support */

    g_signal_connect((void*)folder_view,
                     "drag-data-received",
                     G_CALLBACK(on_folder_view_drag_data_received),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-data-get",
                     G_CALLBACK(on_folder_view_drag_data_get),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-begin",
                     G_CALLBACK(on_folder_view_drag_begin),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-motion",
                     G_CALLBACK(on_folder_view_drag_motion),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-leave",
                     G_CALLBACK(on_folder_view_drag_leave),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-drop",
                     G_CALLBACK(on_folder_view_drag_drop),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-end",
                     G_CALLBACK(on_folder_view_drag_end),
                     file_browser);

    return folder_view;
}

static void
init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* pix_renderer;

    const std::array<int, 6> cols{PTKFileListCol::COL_FILE_NAME,
                                  PTKFileListCol::COL_FILE_SIZE,
                                  PTKFileListCol::COL_FILE_DESC,
                                  PTKFileListCol::COL_FILE_PERM,
                                  PTKFileListCol::COL_FILE_OWNER,
                                  PTKFileListCol::COL_FILE_MTIME};

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];

    for (std::size_t i = 0; i < cols.size(); i++)
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

        // column order
        std::size_t j;
        for (j = 0; j < cols.size(); j++)
        {
            if (xset_get_int_panel(p, column_names.at(j), XSetSetSet::X) == static_cast<int>(i))
                break;
        }
        if (j == cols.size())
            j = i; // failsafe
        else
        {
            // column width
            gtk_tree_view_column_set_min_width(col, 50);
            gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            XSet* set = xset_get_panel_mode(p, column_names.at(j), mode);
            int width = set->y ? std::stol(set->y) : 100;
            if (width)
            {
                if (cols.at(j) == PTKFileListCol::COL_FILE_NAME && !app_settings.always_show_tabs &&
                    file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW &&
                    gtk_notebook_get_n_pages(GTK_NOTEBOOK(file_browser->mynotebook)) == 1)
                {
                    // when tabs are added, the width of the notebook decreases
                    // by a few pixels, meaning there is not enough space for
                    // all columns - this causes a horizontal scrollbar to
                    // appear on new and sometimes first tab
                    // so shave some pixels off first columns
                    gtk_tree_view_column_set_fixed_width(col, width - 6);

                    // below causes increasing reduction of column every time new tab is
                    // added and closed - undesirable
                    PtkFileBrowser* first_fb = PTK_FILE_BROWSER(
                        gtk_notebook_get_nth_page(GTK_NOTEBOOK(file_browser->mynotebook), 0));

                    if (first_fb && first_fb->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW &&
                        GTK_IS_TREE_VIEW(first_fb->folder_view))
                    {
                        GtkTreeViewColumn* first_col =
                            gtk_tree_view_get_column(GTK_TREE_VIEW(first_fb->folder_view), 0);
                        if (first_col)
                        {
                            int first_width = gtk_tree_view_column_get_width(first_col);
                            if (first_width > 10)
                                gtk_tree_view_column_set_fixed_width(first_col, first_width - 6);
                        }
                    }
                }
                else
                {
                    gtk_tree_view_column_set_fixed_width(col, width);
                    // LOG_INFO("init set_width {} {}", column_names.at(j), width);
                }
            }
        }

        if (cols.at(j) == PTKFileListCol::COL_FILE_NAME)
        {
            g_object_set(G_OBJECT(renderer),
                         /* "editable", true, */
                         "ellipsize",
                         PANGO_ELLIPSIZE_END,
                         nullptr);
            /*
            g_signal_connect( renderer, "editing-started",
                              G_CALLBACK( on_filename_editing_started ), nullptr );
            */
            file_browser->icon_render = pix_renderer = gtk_cell_renderer_pixbuf_new();

            gtk_tree_view_column_pack_start(col, pix_renderer, false);
            gtk_tree_view_column_set_attributes(col,
                                                pix_renderer,
                                                "pixbuf",
                                                file_browser->large_icons
                                                    ? PTKFileListCol::COL_FILE_BIG_ICON
                                                    : PTKFileListCol::COL_FILE_SMALL_ICON,
                                                nullptr);

            gtk_tree_view_column_set_expand(col, true);
            gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 150);
            gtk_tree_view_column_set_reorderable(col, false);
            // gtk_tree_view_set_activable_column(GTK_TREE_VIEW(list_view), col);
        }
        else
        {
            gtk_tree_view_column_set_reorderable(col, true);
            gtk_tree_view_column_set_visible(col,
                                             xset_get_b_panel_mode(p, column_names.at(j), mode));
        }

        if (cols.at(j) == PTKFileListCol::COL_FILE_SIZE)
            gtk_cell_renderer_set_alignment(renderer, 1, 0.5);

        gtk_tree_view_column_pack_start(col, renderer, true);
        gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(j), nullptr);
        gtk_tree_view_append_column(list_view, col);
        gtk_tree_view_column_set_title(col, column_titles.at(j));
        gtk_tree_view_column_set_sort_indicator(col, true);
        gtk_tree_view_column_set_sort_column_id(col, cols.at(j));
        gtk_tree_view_column_set_sort_order(col, GTK_SORT_DESCENDING);
    }
}

void
ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    if (file_browser->busy)
        // a dir is already loading
        return;

    if (!std::filesystem::is_directory(ptk_file_browser_get_cwd(file_browser)))
    {
        on_close_notebook_page(nullptr, file_browser);
        return;
    }

    // save cursor's file path for later re-selection
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    VFSFileInfo* file;
    std::string cursor_path;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_get_cursor(EXO_ICON_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_get_cursor(GTK_TREE_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (tree_path && model && gtk_tree_model_get_iter(model, &it, tree_path))
    {
        gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
        if (file)
        {
            cursor_path = Glib::build_filename(ptk_file_browser_get_cwd(file_browser),
                                               vfs_file_info_get_name(file));
        }
    }
    gtk_tree_path_free(tree_path);

    // these steps are similar to chdir
    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
        file_browser->dir = nullptr;
    }

    // destroy file list and create new one
    ptk_file_browser_update_model(file_browser);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    // begin load dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(ptk_file_browser_get_cwd(file_browser));
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::BEGIN_CHDIR_SIGNAL], 0);
    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser->dir, false, file_browser);
        if (std::filesystem::exists(cursor_path))
            ptk_file_browser_select_file(file_browser, cursor_path.c_str());
        file_browser->busy = false;
    }
    else
    {
        file_browser->busy = true;
        free(file_browser->select_path);
        file_browser->select_path = ztd::strdup(cursor_path);
    }
    g_signal_connect(file_browser->dir,
                     "file-listed",
                     G_CALLBACK(on_dir_file_listed),
                     file_browser);
}

unsigned int
ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser)
{
    return file_browser->dir ? file_browser->dir->file_list.size() : 0;
}

unsigned int
ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser)
{
    return file_browser->file_list
               ? gtk_tree_model_iter_n_children(file_browser->file_list, nullptr)
               : 0;
}

GList*
folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            *model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            return exo_icon_view_get_selected_items(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            return gtk_tree_selection_get_selected_rows(tree_sel, model);
            break;
        default:
            break;
    }
    return nullptr;
}

static char*
folder_view_get_drop_dir(PtkFileBrowser* file_browser, int x, int y)
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    std::string dest_path;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(file_browser->folder_view),
                                                x,
                                                y,
                                                &x,
                                                &y);
            tree_path = folder_view_get_tree_path_at_pos(file_browser, x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            // if drag is in progress, get the dest row path
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(file_browser->folder_view),
                                            &tree_path,
                                            nullptr);
            if (!tree_path)
            {
                // no drag in progress, get drop path
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr);
                if (col == gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), 0))
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
                }
            }
            else
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (tree_path)
    {
        if (!gtk_tree_model_get_iter(model, &it, tree_path))
            return nullptr;

        gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
        if (file)
        {
            if (vfs_file_info_is_dir(file))
            {
                dest_path = Glib::build_filename(ptk_file_browser_get_cwd(file_browser),
                                                 vfs_file_info_get_name(file));
            }
            else /* Drop on a file, not directory */
            {
                /* Return current directory */
                dest_path = ptk_file_browser_get_cwd(file_browser);
            }
            vfs_file_info_unref(file);
        }
        gtk_tree_path_free(tree_path);
    }
    else
    {
        dest_path = ptk_file_browser_get_cwd(file_browser);
    }
    return ztd::strdup(dest_path);
}

static void
on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                                  GtkSelectionData* sel_data, unsigned int info, unsigned int time,
                                  void* user_data)
{
    (void)x;
    (void)y;
    (void)info;
    PtkFileBrowser* file_browser = static_cast<PtkFileBrowser*>(user_data);
    char* dest_dir;
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-received");

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        dest_dir =
            folder_view_get_drop_dir(file_browser, file_browser->drag_x, file_browser->drag_y);
        // LOG_INFO("FB dest_dir = {}", dest_dir );
        if (dest_dir)
        {
            char** list;
            char** puri;
            puri = list = gtk_selection_data_get_uris(sel_data);

            if (file_browser->pending_drag_status)
            {
                // We only want to update drag status, not really want to drop
                dev_t dest_dev;
                ino_t dest_inode;
                struct stat statbuf; // skip stat
                if (stat(dest_dir, &statbuf) == 0)
                {
                    dest_dev = statbuf.st_dev;
                    dest_inode = statbuf.st_ino;
                    if (file_browser->drag_source_dev == 0)
                    {
                        file_browser->drag_source_dev = dest_dev;
                        for (; *puri; ++puri)
                        {
                            const std::string file_path = Glib::filename_from_uri(*puri);
                            if (stat(file_path.c_str(), &statbuf) == 0)
                            {
                                if (statbuf.st_dev != dest_dev)
                                {
                                    // different devices - store source device
                                    file_browser->drag_source_dev = statbuf.st_dev;
                                    break;
                                }
                                else if (file_browser->drag_source_inode == 0)
                                {
                                    // same device - store source parent inode
                                    char* src_dir = g_path_get_dirname(file_path.c_str());
                                    if (src_dir && stat(src_dir, &statbuf) == 0)
                                    {
                                        file_browser->drag_source_inode = statbuf.st_ino;
                                    }
                                    free(src_dir);
                                }
                            }
                        }
                    }
                    if (file_browser->drag_source_dev != dest_dev ||
                        file_browser->drag_source_inode == dest_inode)
                        // src and dest are on different devices or same dir
                        gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
                    else
                        gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
                }
                else
                { // stat failed
                    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
                }

                free(dest_dir);
                g_strfreev(list);
                file_browser->pending_drag_status = false;
                return;
            }
            if (puri)
            {
                std::vector<std::string> file_list;
                if ((gdk_drag_context_get_selected_action(drag_context) &
                     (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)) == 0)
                {
                    gdk_drag_status(drag_context, GDK_ACTION_COPY, time); // sfm correct?  was MOVE
                }
                gtk_drag_finish(drag_context, true, false, time);

                for (; *puri; ++puri)
                {
                    std::string file_path;
                    if (**puri == '/')
                        file_path = *puri;
                    else
                        file_path = Glib::filename_from_uri(*puri);

                    file_list.push_back(file_path);
                }
                g_strfreev(list);

                VFSFileTaskType file_action;
                switch (gdk_drag_context_get_selected_action(drag_context))
                {
                    case GDK_ACTION_COPY:
                        file_action = VFSFileTaskType::VFS_FILE_TASK_COPY;
                        break;
                    case GDK_ACTION_MOVE:
                        file_action = VFSFileTaskType::VFS_FILE_TASK_MOVE;
                        break;
                    case GDK_ACTION_LINK:
                        file_action = VFSFileTaskType::VFS_FILE_TASK_LINK;
                        break;
                    case GDK_ACTION_DEFAULT:
                    case GDK_ACTION_PRIVATE:
                    case GDK_ACTION_ASK:
                    default:
                        break;
                }

                if (!file_list.empty())
                {
                    // LOG_INFO("dest_dir = {}", dest_dir);

                    /* We only want to update drag status, not really want to drop */
                    if (file_browser->pending_drag_status)
                    {
                        struct stat statbuf; // skip stat
                        if (stat(dest_dir, &statbuf) == 0)
                            file_browser->pending_drag_status = false;
                        free(dest_dir);
                        return;
                    }
                    else /* Accept the drop and perform file actions */
                    {
                        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
                        PtkFileTask* ptask = new PtkFileTask(file_action,
                                                             file_list,
                                                             dest_dir,
                                                             GTK_WINDOW(parent_win),
                                                             file_browser->task_view);
                        ptk_file_task_run(ptask);
                    }
                }
                free(dest_dir);
                gtk_drag_finish(drag_context, true, false, time);
                return;
            }
        }
        free(dest_dir);
    }

    /* If we are only getting drag status, not finished. */
    if (file_browser->pending_drag_status)
    {
        file_browser->pending_drag_status = false;
        return;
    }
    gtk_drag_finish(drag_context, false, false, time);
}

static void
on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                             GtkSelectionData* sel_data, unsigned int info, unsigned int time,
                             PtkFileBrowser* file_browser)
{
    (void)drag_context;
    (void)info;
    (void)time;
    GdkAtom type = gdk_atom_intern("text/uri-list", false);
    std::string uri_list;
    std::vector<VFSFileInfo*> sel_files = ptk_file_browser_get_selected_files(file_browser);

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-get");

    // drag_context->suggested_action = GDK_ACTION_MOVE;

    for (VFSFileInfo* file: sel_files)
    {
        const std::string full_path = Glib::build_filename(ptk_file_browser_get_cwd(file_browser),
                                                           vfs_file_info_get_name(file));
        const std::string uri = Glib::filename_to_uri(full_path);

        uri_list.append(fmt::format("{}\n", uri));
    }

    vfs_file_info_list_free(sel_files);
    gtk_selection_data_set(sel_data,
                           type,
                           8,
                           (const unsigned char*)uri_list.c_str(),
                           uri_list.size());
}

static void
on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                          PtkFileBrowser* file_browser)
{
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-begin");
    gtk_drag_set_icon_default(drag_context);
    file_browser->is_drag = true;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, int x, int y)
{
    GtkTreePath* tree_path;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            tree_path =
                exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(file_browser->folder_view), x, y);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                          x,
                                          y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr);
            break;
        default:
            break;
    }

    return tree_path;
}

static bool
on_folder_view_auto_scroll(GtkScrolledWindow* scroll)
{
    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    double vpos = gtk_adjustment_get_value(vadj);

    if (folder_view_auto_scroll_direction == GTK_DIR_UP)
    {
        vpos -= gtk_adjustment_get_step_increment(vadj);
        if (vpos > gtk_adjustment_get_lower(vadj))
            gtk_adjustment_set_value(vadj, vpos);
        else
            gtk_adjustment_set_value(vadj, gtk_adjustment_get_lower(vadj));
    }
    else
    {
        vpos += gtk_adjustment_get_step_increment(vadj);
        if ((vpos + gtk_adjustment_get_page_size(vadj)) < gtk_adjustment_get_upper(vadj))
            gtk_adjustment_set_value(vadj, vpos);
        else
            gtk_adjustment_set_value(
                vadj,
                (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)));
    }
    return true;
}

static bool
on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                           unsigned int time, PtkFileBrowser* file_browser)
{
    GtkAllocation allocation;

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-motion");

    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));

    // GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    gtk_widget_get_allocation(widget, &allocation);

    if (y < 32)
    {
        /* Auto scroll up */
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GTK_DIR_UP;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (y > (allocation.height - 32))
    {
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GTK_DIR_DOWN;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    GtkTreeViewColumn* col;
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            // store x and y because exo_icon_view has no get_drag_dest_row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(widget), x, y, &x, &y);
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            // store x and y because == 0 for update drag status when is last row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr))
            {
                if (gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 0) == col)
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                }
            }
            break;
        default:
            break;
    }

    if (tree_path)
    {
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            VFSFileInfo* file;
            gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
            if (!file || !vfs_file_info_is_dir(file))
            {
                gtk_tree_path_free(tree_path);
                tree_path = nullptr;
            }
            vfs_file_info_unref(file);
        }
    }

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             tree_path,
                                             EXO_ICON_VIEW_DROP_INTO);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            tree_path,
                                            GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
            break;
        default:
            break;
    }

    if (tree_path)
        gtk_tree_path_free(tree_path);

    /* FIXME: Creating a new target list everytime is very inefficient,
         but currently gtk_drag_dest_get_target_list always returns nullptr
         due to some strange reason, and cannot be used currently.  */
    GtkTargetList* target_list = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, target_list);
    gtk_target_list_unref(target_list);

    if (target == GDK_NONE)
        gdk_drag_status(drag_context, (GdkDragAction)0, time);
    else
    {
        GdkDragAction suggested_action;
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_MOVE)
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_COPY)
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_LINK)
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            int drag_action = xset_get_int(XSetName::DRAG_ACTION, XSetSetSet::X);

            switch (drag_action)
            {
                case 1:
                    suggested_action = GDK_ACTION_COPY;
                    break;
                case 2:
                    suggested_action = GDK_ACTION_MOVE;
                    break;
                case 3:
                    suggested_action = GDK_ACTION_LINK;
                    break;
                default:
                    // automatic
                    file_browser->pending_drag_status = true;
                    gtk_drag_get_data(widget, drag_context, target, time);
                    suggested_action = gdk_drag_context_get_selected_action(drag_context);
                    break;
            }
        }
        gdk_drag_status(drag_context, suggested_action, time);
    }
    return true;
}

static bool
on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, unsigned int time,
                          PtkFileBrowser* file_browser)
{
    (void)drag_context;
    (void)time;
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-leave");
    file_browser->drag_source_dev = 0;
    file_browser->drag_source_inode = 0;

    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }
    return true;
}

static bool
on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                         unsigned int time, PtkFileBrowser* file_browser)
{
    (void)x;
    (void)y;
    (void)file_browser;
    GdkAtom target = gdk_atom_intern("text/uri-list", false);
    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-drop");

    gtk_drag_get_data(widget, drag_context, target, time);
    return true;
}

static void
on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                        PtkFileBrowser* file_browser)
{
    (void)drag_context;
    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             nullptr,
                                             (ExoIconViewDropPosition)0);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            nullptr,
                                            (GtkTreeViewDropPosition)0);
            break;
        default:
            break;
    }
    file_browser->is_drag = false;
}

void
ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser,
                                       std::vector<VFSFileInfo*>& sel_files, const char* cwd)
{
    if (!file_browser)
        return;

    if (sel_files.empty())
        return;

    gtk_widget_grab_focus(file_browser->folder_view);
    gtk_widget_get_toplevel(GTK_WIDGET(file_browser));

    for (VFSFileInfo* file: sel_files)
    {
        if (!ptk_rename_file(file_browser,
                             cwd,
                             file,
                             nullptr,
                             false,
                             PtkRenameMode::PTK_RENAME,
                             nullptr))
            break;
    }
}

void
ptk_file_browser_paste_link(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_links(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                              ptk_file_browser_get_cwd(file_browser),
                              GTK_TREE_VIEW(file_browser->task_view),
                              nullptr,
                              nullptr);
}

void
ptk_file_browser_paste_target(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_targets(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                ptk_file_browser_get_cwd(file_browser),
                                GTK_TREE_VIEW(file_browser->task_view),
                                nullptr,
                                nullptr);
}

std::vector<VFSFileInfo*>
ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser)
{
    GtkTreeModel* model;
    std::vector<VFSFileInfo*> file_list;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);
    if (!sel_files)
        return file_list;

    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        GtkTreeIter it;
        VFSFileInfo* file;
        gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data);
        gtk_tree_model_get(model, &it, PTKFileListCol::COL_FILE_INFO, &file, -1);
        file_list.push_back(file);
    }
    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);

    return file_list;
}

static void
ptk_file_browser_open_selected_files_with_app(PtkFileBrowser* file_browser, char* app_desktop)
{
    std::vector<VFSFileInfo*> sel_files = ptk_file_browser_get_selected_files(file_browser);

    ptk_open_files_with_app(ptk_file_browser_get_cwd(file_browser),
                            sel_files,
                            app_desktop,
                            file_browser,
                            false,
                            false);

    vfs_file_info_list_free(sel_files);
}

void
ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser)
{
    if (xset_opener(file_browser, 1))
        return;
    ptk_file_browser_open_selected_files_with_app(file_browser, nullptr);
}

void
ptk_file_browser_copycmd(PtkFileBrowser* file_browser, std::vector<VFSFileInfo*>& sel_files,
                         const char* cwd, XSetName setname)
{
    if (!file_browser)
        return;

    XSet* set2;
    char* copy_dest = nullptr;
    char* move_dest = nullptr;

    if (setname == XSetName::COPY_TAB_PREV)
        copy_dest = main_window_get_tab_cwd(file_browser, -1);
    else if (setname == XSetName::COPY_TAB_NEXT)
        copy_dest = main_window_get_tab_cwd(file_browser, -2);
    else if (setname == XSetName::COPY_TAB_1)
        copy_dest = main_window_get_tab_cwd(file_browser, 1);
    else if (setname == XSetName::COPY_TAB_2)
        copy_dest = main_window_get_tab_cwd(file_browser, 2);
    else if (setname == XSetName::COPY_TAB_3)
        copy_dest = main_window_get_tab_cwd(file_browser, 3);
    else if (setname == XSetName::COPY_TAB_4)
        copy_dest = main_window_get_tab_cwd(file_browser, 4);
    else if (setname == XSetName::COPY_TAB_5)
        copy_dest = main_window_get_tab_cwd(file_browser, 5);
    else if (setname == XSetName::COPY_TAB_6)
        copy_dest = main_window_get_tab_cwd(file_browser, 6);
    else if (setname == XSetName::COPY_TAB_7)
        copy_dest = main_window_get_tab_cwd(file_browser, 7);
    else if (setname == XSetName::COPY_TAB_8)
        copy_dest = main_window_get_tab_cwd(file_browser, 8);
    else if (setname == XSetName::COPY_TAB_9)
        copy_dest = main_window_get_tab_cwd(file_browser, 9);
    else if (setname == XSetName::COPY_TAB_10)
        copy_dest = main_window_get_tab_cwd(file_browser, 10);
    else if (setname == XSetName::COPY_PANEL_PREV)
        copy_dest = main_window_get_panel_cwd(file_browser, -1);
    else if (setname == XSetName::COPY_PANEL_NEXT)
        copy_dest = main_window_get_panel_cwd(file_browser, -2);
    else if (setname == XSetName::COPY_PANEL_1)
        copy_dest = main_window_get_panel_cwd(file_browser, 1);
    else if (setname == XSetName::COPY_PANEL_2)
        copy_dest = main_window_get_panel_cwd(file_browser, 2);
    else if (setname == XSetName::COPY_PANEL_3)
        copy_dest = main_window_get_panel_cwd(file_browser, 3);
    else if (setname == XSetName::COPY_PANEL_4)
        copy_dest = main_window_get_panel_cwd(file_browser, 4);
    else if (setname == XSetName::COPY_LOC_LAST)
    {
        set2 = xset_get(XSetName::COPY_LOC_LAST);
        copy_dest = ztd::strdup(set2->desc);
    }
    else if (setname == XSetName::MOVE_TAB_PREV)
        move_dest = main_window_get_tab_cwd(file_browser, -1);
    else if (setname == XSetName::MOVE_TAB_NEXT)
        move_dest = main_window_get_tab_cwd(file_browser, -2);
    else if (setname == XSetName::MOVE_TAB_1)
        move_dest = main_window_get_tab_cwd(file_browser, 1);
    else if (setname == XSetName::MOVE_TAB_2)
        move_dest = main_window_get_tab_cwd(file_browser, 2);
    else if (setname == XSetName::MOVE_TAB_3)
        move_dest = main_window_get_tab_cwd(file_browser, 3);
    else if (setname == XSetName::MOVE_TAB_4)
        move_dest = main_window_get_tab_cwd(file_browser, 4);
    else if (setname == XSetName::MOVE_TAB_5)
        move_dest = main_window_get_tab_cwd(file_browser, 5);
    else if (setname == XSetName::MOVE_TAB_6)
        move_dest = main_window_get_tab_cwd(file_browser, 6);
    else if (setname == XSetName::MOVE_TAB_7)
        move_dest = main_window_get_tab_cwd(file_browser, 7);
    else if (setname == XSetName::MOVE_TAB_8)
        move_dest = main_window_get_tab_cwd(file_browser, 8);
    else if (setname == XSetName::MOVE_TAB_9)
        move_dest = main_window_get_tab_cwd(file_browser, 9);
    else if (setname == XSetName::MOVE_TAB_10)
        move_dest = main_window_get_tab_cwd(file_browser, 10);
    else if (setname == XSetName::MOVE_PANEL_PREV)
        move_dest = main_window_get_panel_cwd(file_browser, -1);
    else if (setname == XSetName::MOVE_PANEL_NEXT)
        move_dest = main_window_get_panel_cwd(file_browser, -2);
    else if (setname == XSetName::MOVE_PANEL_1)
        move_dest = main_window_get_panel_cwd(file_browser, 1);
    else if (setname == XSetName::MOVE_PANEL_2)
        move_dest = main_window_get_panel_cwd(file_browser, 2);
    else if (setname == XSetName::MOVE_PANEL_3)
        move_dest = main_window_get_panel_cwd(file_browser, 3);
    else if (setname == XSetName::MOVE_PANEL_4)
        move_dest = main_window_get_panel_cwd(file_browser, 4);
    else if (setname == XSetName::MOVE_LOC_LAST)
    {
        set2 = xset_get(XSetName::COPY_LOC_LAST);
        move_dest = ztd::strdup(set2->desc);
    }

    if ((setname == XSetName::COPY_LOC || setname == XSetName::COPY_LOC_LAST ||
         setname == XSetName::MOVE_LOC || setname == XSetName::MOVE_LOC_LAST) &&
        !copy_dest && !move_dest)
    {
        const char* folder;
        set2 = xset_get(XSetName::COPY_LOC_LAST);
        if (set2->desc)
            folder = set2->desc;
        else
            folder = cwd;
        char* path = xset_file_dialog(GTK_WIDGET(file_browser),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      "Choose Location",
                                      folder,
                                      nullptr);
        if (path && std::filesystem::is_directory(path))
        {
            if (setname == XSetName::COPY_LOC || setname == XSetName::COPY_LOC_LAST)
                copy_dest = path;
            else
                move_dest = path;
            set2 = xset_get(XSetName::COPY_LOC_LAST);
            xset_set_set(set2, XSetSetSet::DESC, path);
        }
        else
        {
            return;
        }
    }

    if (copy_dest || move_dest)
    {
        VFSFileTaskType file_action;
        char* dest_dir;

        if (copy_dest)
        {
            file_action = VFSFileTaskType::VFS_FILE_TASK_COPY;
            dest_dir = copy_dest;
        }
        else
        {
            file_action = VFSFileTaskType::VFS_FILE_TASK_MOVE;
            dest_dir = move_dest;
        }

        if (!strcmp(dest_dir, cwd))
        {
            xset_msg_dialog(GTK_WIDGET(file_browser),
                            GTK_MESSAGE_ERROR,
                            "Invalid Destination",
                            GTK_BUTTONS_OK,
                            "Destination same as source");
            free(dest_dir);
            return;
        }

        // rebuild sel_files with full paths
        std::vector<std::string> file_list;
        std::string file_path;
        for (VFSFileInfo* file: sel_files)
        {
            file_path = Glib::build_filename(cwd, vfs_file_info_get_name(file));
            file_list.push_back(file_path);
        }

        // task
        PtkFileTask* ptask =
            new PtkFileTask(file_action,
                            file_list,
                            dest_dir,
                            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                            file_browser->task_view);
        ptk_file_task_run(ptask);
        free(dest_dir);
    }
    else
    {
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GTK_MESSAGE_ERROR,
                        "Invalid Destination",
                        GTK_BUTTONS_OK,
                        "Invalid destination");
    }
}

void
ptk_file_browser_hide_selected(PtkFileBrowser* file_browser, std::vector<VFSFileInfo*>& sel_files,
                               const char* cwd)
{
    if (xset_msg_dialog(
            GTK_WIDGET(file_browser),
            GTK_MESSAGE_INFO,
            "Hide File",
            GTK_BUTTONS_OK_CANCEL,
            "The names of the selected files will be added to the '.hidden' file located in this "
            "directory, which will hide them from view in SpaceFM.  You may need to refresh the "
            "view or restart SpaceFM for the files to disappear.\n\nTo unhide a file, open the "
            ".hidden file in your text editor, remove the name of the file, and refresh.") !=
        GTK_RESPONSE_OK)
        return;

    if (sel_files.empty())
    {
        ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                       "Error",
                       "No files are selected");
        return;
    }

    for (VFSFileInfo* file: sel_files)
    {
        if (!vfs_dir_add_hidden(cwd, vfs_file_info_get_name(file)))
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           "Error hiding files");
    }

    // refresh from here causes a segfault occasionally
    // ptk_file_browser_refresh( nullptr, file_browser );
}

void
ptk_file_browser_file_properties(PtkFileBrowser* file_browser, int page)
{
    if (!file_browser)
        return;

    char* dir_name = nullptr;
    std::vector<VFSFileInfo*> sel_files = ptk_file_browser_get_selected_files(file_browser);
    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    if (sel_files.empty())
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get(file, ptk_file_browser_get_cwd(file_browser), nullptr);
        sel_files.push_back(file);
        dir_name = g_path_get_dirname(cwd);
    }
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
    ptk_show_file_properties(GTK_WINDOW(parent), dir_name ? dir_name : cwd, sel_files, page);
    vfs_file_info_list_free(sel_files);
    free(dir_name);
}

void
on_popup_file_properties_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    GObject* popup = G_OBJECT(user_data);
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(popup, "PtkFileBrowser"));
    ptk_file_browser_file_properties(file_browser, 0);
}

void
ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show)
{
    if (!!file_browser->show_hidden_files == show)
        return;
    file_browser->show_hidden_files = show;

    if (file_browser->file_list)
    {
        ptk_file_browser_update_model(file_browser);
        g_signal_emit(file_browser, signals[PTKFileBrowserSignal::SEL_CHANGE_SIGNAL], 0);
    }

    if (file_browser->side_dir)
    {
        ptk_dir_tree_view_show_hidden_files(GTK_TREE_VIEW(file_browser->side_dir),
                                            file_browser->show_hidden_files);
    }

    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::SHOW_HIDDEN);
}

static bool
on_dir_tree_button_press(GtkWidget* view, GdkEventButton* evt, PtkFileBrowser* file_browser)
{
    ptk_file_browser_focus_me(file_browser);

    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler.win_click,
                          XSetName::EVT_WIN_CLICK,
                          0,
                          0,
                          "dirtree",
                          0,
                          evt->button,
                          evt->state,
                          true))
        return false;

    if (evt->type == GDK_BUTTON_PRESS && evt->button == 2) /* middle click */
    {
        /* left and right click handled in ptk-dir-tree-view.c
         * on_dir_tree_view_button_press() */
        GtkTreeModel* model;
        GtkTreePath* tree_path;
        GtkTreeIter it;

        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          evt->x,
                                          evt->y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                VFSFileInfo* file;
                gtk_tree_model_get(model, &it, PTKDirTreeCol::COL_DIR_TREE_INFO, &file, -1);
                if (file)
                {
                    char* file_path;
                    file_path = ptk_dir_view_get_dir_path(model, &it);
                    g_signal_emit(file_browser,
                                  signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL],
                                  0,
                                  file_path,
                                  PtkOpenAction::PTK_OPEN_NEW_TAB);
                    free(file_path);
                    vfs_file_info_unref(file);
                }
            }
            gtk_tree_path_free(tree_path);
        }
        return true;
    }
    return false;
}

static GtkWidget*
ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser)
{
    GtkWidget* dir_tree = ptk_dir_tree_view_new(file_browser, file_browser->show_hidden_files);
    // GtkTreeSelection* dir_tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dir_tree));
    g_signal_connect(dir_tree,
                     "row-activated",
                     G_CALLBACK(on_dir_tree_row_activated),
                     file_browser);
    g_signal_connect(dir_tree,
                     "button-press-event",
                     G_CALLBACK(on_dir_tree_button_press),
                     file_browser);

    return dir_tree;
}

static int
file_list_order_from_sort_order(PtkFBSortOrder order)
{
    int col;

    switch (order)
    {
        case PtkFBSortOrder::PTK_FB_SORT_BY_NAME:
            col = PTKFileListCol::COL_FILE_NAME;
            break;
        case PtkFBSortOrder::PTK_FB_SORT_BY_SIZE:
            col = PTKFileListCol::COL_FILE_SIZE;
            break;
        case PtkFBSortOrder::PTK_FB_SORT_BY_MTIME:
            col = PTKFileListCol::COL_FILE_MTIME;
            break;
        case PtkFBSortOrder::PTK_FB_SORT_BY_TYPE:
            col = PTKFileListCol::COL_FILE_DESC;
            break;
        case PtkFBSortOrder::PTK_FB_SORT_BY_PERM:
            col = PTKFileListCol::COL_FILE_PERM;
            break;
        case PtkFBSortOrder::PTK_FB_SORT_BY_OWNER:
            col = PTKFileListCol::COL_FILE_OWNER;
            break;
        default:
            col = PTKFileListCol::COL_FILE_NAME;
    }
    return col;
}

void
ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser)
{
    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (!list)
        return;

    list->sort_alphanum = xset_get_b_panel(file_browser->mypanel, "sort_extra");
#if 0
    list->sort_natural = xset_get_b_panel(file_browser->mypanel, "sort_extra");
#endif
    list->sort_case = xset_get_int_panel(file_browser->mypanel, "sort_extra", XSetSetSet::X) ==
                      XSetB::XSET_B_TRUE;
    list->sort_dir = xset_get_int_panel(file_browser->mypanel, "sort_extra", XSetSetSet::Y);
    list->sort_hidden_first =
        xset_get_int_panel(file_browser->mypanel, "sort_extra", XSetSetSet::Z) ==
        XSetB::XSET_B_TRUE;
}

void
ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, XSetName setname)
{
    if (!file_browser)
        return;

    XSet* set = xset_get(setname);

    if (!Glib::str_has_prefix(set->name, "sortx_"))
        return;

    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (!list)
        return;
    int panel = file_browser->mypanel;

    if (set->xset_name == XSetName::SORTX_ALPHANUM)
    {
        list->sort_alphanum = set->b == XSetB::XSET_B_TRUE;
        xset_set_b_panel(panel, "sort_extra", list->sort_alphanum);
    }
#if 0
    else if (set->xset_name ==  XSetName::SORTX_NATURAL)
    {
        list->sort_natural = set->b == XSetB::XSET_B_TRUE;
        xset_set_b_panel(panel, "sort_extra", list->sort_natural);
    }
#endif
    else if (set->xset_name == XSetName::SORTX_CASE)
    {
        list->sort_case = set->b == XSetB::XSET_B_TRUE;
        xset_set_panel(panel, "sort_extra", XSetSetSet::X, std::to_string(set->b));
    }
    else if (set->xset_name == XSetName::SORTX_DIRECTORIES)
    {
        list->sort_dir = PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST;
        xset_set_panel(panel,
                       "sort_extra",
                       XSetSetSet::Y,
                       std::to_string(PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST));
    }
    else if (set->xset_name == XSetName::SORTX_FILES)
    {
        list->sort_dir = PTKFileListSortDir::PTK_LIST_SORT_DIR_LAST;
        xset_set_panel(panel,
                       "sort_extra",
                       XSetSetSet::Y,
                       std::to_string(PTKFileListSortDir::PTK_LIST_SORT_DIR_LAST));
    }
    else if (set->xset_name == XSetName::SORTX_MIX)
    {
        list->sort_dir = PTKFileListSortDir::PTK_LIST_SORT_DIR_MIXED;
        xset_set_panel(panel,
                       "sort_extra",
                       XSetSetSet::Y,
                       std::to_string(PTKFileListSortDir::PTK_LIST_SORT_DIR_MIXED));
    }
    else if (set->xset_name == XSetName::SORTX_HIDFIRST)
    {
        list->sort_hidden_first = set->b == XSetB::XSET_B_TRUE;
        xset_set_panel(panel, "sort_extra", XSetSetSet::Z, std::to_string(set->b));
    }
    else if (set->xset_name == XSetName::SORTX_HIDLAST)
    {
        list->sort_hidden_first = set->b != XSetB::XSET_B_TRUE;
        xset_set_panel(panel,
                       "sort_extra",
                       XSetSetSet::Z,
                       std::to_string(set->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_FALSE
                                                                   : XSetB::XSET_B_TRUE));
    }
    ptk_file_list_sort(list);
}

void
ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser, PtkFBSortOrder order)
{
    if (order == file_browser->sort_order)
        return;

    file_browser->sort_order = order;
    int col = file_list_order_from_sort_order(order);

    if (file_browser->file_list)
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                             col,
                                             file_browser->sort_type);
    }
}

void
ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order)
{
    if (order != file_browser->sort_type)
    {
        file_browser->sort_type = order;
        if (file_browser->file_list)
        {
            int col;
            GtkSortType old_order;
            gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 &col,
                                                 &old_order);
            gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 col,
                                                 order);
        }
    }
}

/* FIXME: Do not recreate the view if previous view is compact view */
void
ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    true,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PtkFBViewMode::PTK_FB_ICON_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view = create_folder_view(file_browser, PtkFBViewMode::PTK_FB_ICON_VIEW);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

/* FIXME: Do not recreate the view if previous view is icon view */
void
ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PtkFBViewMode::PTK_FB_COMPACT_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view =
        create_folder_view(file_browser, PtkFBViewMode::PTK_FB_COMPACT_VIEW);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

void
ptk_file_browser_view_as_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PtkFBViewMode::PTK_FB_LIST_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view = create_folder_view(file_browser, PtkFBViewMode::PTK_FB_LIST_VIEW);
    gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

unsigned int
ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, std::uint64_t* sel_size)
{
    if (sel_size)
        *sel_size = file_browser->sel_size;
    return file_browser->n_sel_files;
}

static void
ptk_file_browser_before_chdir(PtkFileBrowser* file_browser, const char* path)
{
    (void)file_browser;
    (void)path;
}

static void
ptk_file_browser_after_chdir(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_content_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_sel_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser)
{
    (void)file_browser;
}

static void
ptk_file_browser_open_item(PtkFileBrowser* file_browser, const char* path, int action)
{
    (void)file_browser;
    (void)path;
    (void)action;
}

static void
show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big, int max_file_size)
{
    /* This function collects all calls to ptk_file_list_show_thumbnails()
     * and disables them if change detection is blacklisted on current device */
    if (!(file_browser && file_browser->dir))
        max_file_size = 0;
    else if (file_browser->dir->avoid_changes)
        max_file_size = 0;
    ptk_file_list_show_thumbnails(list, is_big, max_file_size);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSetTool::SHOW_THUMB);
}

void
ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, int max_file_size)
{
    file_browser->max_thumbnail = max_file_size;
    if (file_browser->file_list)
    {
        show_thumbnails(file_browser,
                        PTK_FILE_LIST(file_browser->file_list),
                        file_browser->large_icons,
                        max_file_size);
    }
}

void
ptk_file_browser_emit_open(PtkFileBrowser* file_browser, const char* path, PtkOpenAction action)
{
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::OPEN_ITEM_SIGNAL], 0, path, action);
}

void
ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click)
{
    if (single_click == file_browser->single_click)
        return;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_single_click(EXO_ICON_VIEW(file_browser->folder_view), single_click);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            // gtk_tree_view_set_single_click(GTK_TREE_VIEW(file_browser->folder_view),
            // single_click);
            break;
        default:
            break;
    }

    file_browser->single_click = single_click;
}

void
ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, unsigned int timeout)
{
    if (timeout == file_browser->single_click_timeout)
        return;

    switch (file_browser->view_mode)
    {
        case PtkFBViewMode::PTK_FB_ICON_VIEW:
        case PtkFBViewMode::PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_single_click_timeout(EXO_ICON_VIEW(file_browser->folder_view),
                                                   timeout);
            break;
        case PtkFBViewMode::PTK_FB_LIST_VIEW:
            // gtk_tree_view_set_single_click_timeout(GTK_TREE_VIEW(file_browser->folder_view),
            //                                       timeout);
            break;
        default:
            break;
    }

    file_browser->single_click_timeout = timeout;
}

////////////////////////////////////////////////////////////////////////////

int
ptk_file_browser_no_access(const char* cwd, const char* smode)
{
    int mode;
    if (!smode)
        mode = W_OK;
    else if (!strcmp(smode, "R_OK"))
        mode = R_OK;
    else
        mode = W_OK;

    return faccessat(0, cwd, mode, AT_EACCESS);
}

void
ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, int job2)
{
    GtkWidget* widget;
    int job;
    if (item)
        job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    else
        job = job2;

    FMMainWindow* main_window = static_cast<FMMainWindow*>(file_browser->main_window);
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];
    switch (job)
    {
        case 0:
            // path bar
            if (!xset_get_b_panel_mode(p, "show_toolbox", mode))
            {
                xset_set_b_panel_mode(p, "show_toolbox", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->path_bar;
            break;
        case 1:
            if (!xset_get_b_panel_mode(p, "show_dirtree", mode))
            {
                xset_set_b_panel_mode(p, "show_dirtree", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dir;
            break;
        case 2:
            if (!xset_get_b_panel_mode(p, "show_book", mode))
            {
                xset_set_b_panel_mode(p, "show_book", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_book;
            break;
        case 3:
            if (!xset_get_b_panel_mode(p, "show_devmon", mode))
            {
                xset_set_b_panel_mode(p, "show_devmon", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dev;
            break;
        case 4:
            widget = file_browser->folder_view;
            break;
        default:
            return;
    }
    if (gtk_widget_get_visible(widget))
        gtk_widget_grab_focus(GTK_WIDGET(widget));
}

static void
focus_folder_view(PtkFileBrowser* file_browser)
{
    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::PANE_MODE_CHANGE_SIGNAL], 0);
}

void
ptk_file_browser_focus_me(PtkFileBrowser* file_browser)
{
    g_signal_emit(file_browser, signals[PTKFileBrowserSignal::PANE_MODE_CHANGE_SIGNAL], 0);
}

void
ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, int t)
{
    // LOG_INFO("ptk_file_browser_go_tab fb={:p}", fmt::ptr(file_browser));
    GtkWidget* notebook = file_browser->mynotebook;
    int tab_num;
    if (item)
        tab_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab_num"));
    else
        tab_num = t;

    switch (tab_num)
    {
        case -1:
            // prev
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == 0)
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
                                              gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) - 1);
            else
                gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
            break;
        case -2:
            // next
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) + 1 ==
                gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)))
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
            else
                gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
            break;
        case -3:
            // close
            on_close_notebook_page(nullptr, file_browser);
            break;
        case -4:
            // restore
            on_restore_notebook_page(nullptr, file_browser);
            break;
        default:
            // set tab
            if (tab_num <= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) && tab_num > 0)
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), tab_num - 1);
            break;
    }
}

void
ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, int tab_num, const char* file_path)
{
    int page_x;
    GtkWidget* notebook = file_browser->mynotebook;
    int cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    switch (tab_num)
    {
        case -1:
            // prev
            page_x = cur_page - 1;
            break;
        case -2:
            // next
            page_x = cur_page + 1;
            break;
        default:
            page_x = tab_num - 1;
            break;
    }

    if (page_x > -1 && page_x < pages && page_x != cur_page)
    {
        PtkFileBrowser* a_browser =
            PTK_FILE_BROWSER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x));

        ptk_file_browser_chdir(a_browser, file_path, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    }
}

void
ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser,
                               std::vector<VFSFileInfo*>& sel_files, const char* cwd)
{
    if (sel_files.empty())
        return;

    char* name;
    std::string prog;
    bool as_root = false;
    std::string user1 = "1000";
    std::string user2 = "1001";
    std::string myuser = ztd::strdup(fmt::format("{}", geteuid()));

    XSet* set = XSET(g_object_get_data(G_OBJECT(item), "set"));
    if (!set || !file_browser)
        return;

    if (ztd::startswith(set->name, "perm_"))
    {
        name = set->name + 5;
        if (ztd::startswith(name, "go") || ztd::startswith(name, "ugo"))
            prog = "chmod -R";
        else
            prog = "chmod";
    }
    else if (ztd::startswith(set->name, "rperm_"))
    {
        name = set->name + 6;
        if (ztd::startswith(name, "go") || ztd::startswith(name, "ugo"))
            prog = "chmod -R";
        else
            prog = "chmod";
        as_root = true;
    }
    else if (ztd::startswith(set->name, "own_"))
    {
        name = set->name + 4;
        prog = "chown";
        as_root = true;
    }
    else if (ztd::startswith(set->name, "rown_"))
    {
        name = set->name + 5;
        prog = "chown -R";
        as_root = true;
    }
    else
        return;

    std::string cmd;
    if (ztd::same(name, "r"))
        cmd = "u+r-wx,go-rwx";
    else if (ztd::same(name, "rw"))
        cmd = "u+rw-x,go-rwx";
    else if (ztd::same(name, "rwx"))
        cmd = "u+rwx,go-rwx";
    else if (ztd::same(name, "r_r"))
        cmd = "u+r-wx,g+r-wx,o-rwx";
    else if (ztd::same(name, "rw_r"))
        cmd = "u+rw-x,g+r-wx,o-rwx";
    else if (ztd::same(name, "rw_rw"))
        cmd = "u+rw-x,g+rw-x,o-rwx";
    else if (ztd::same(name, "rwxr_x"))
        cmd = "u+rwx,g+rx-w,o-rwx";
    else if (ztd::same(name, "rwxrwx"))
        cmd = "u+rwx,g+rwx,o-rwx";
    else if (ztd::same(name, "r_r_r"))
        cmd = "ugo+r,ugo-wx";
    else if (ztd::same(name, "rw_r_r"))
        cmd = "u+rw-x,go+r-wx";
    else if (ztd::same(name, "rw_rw_rw"))
        cmd = "ugo+rw-x";
    else if (ztd::same(name, "rwxr_r"))
        cmd = "u+rwx,go+r-wx";
    else if (ztd::same(name, "rwxr_xr_x"))
        cmd = "u+rwx,go+rx-w";
    else if (ztd::same(name, "rwxrwxrwx"))
        cmd = "ugo+rwx,-t";
    else if (ztd::same(name, "rwxrwxrwt"))
        cmd = "ugo+rwx,+t";
    else if (ztd::same(name, "unstick"))
        cmd = "-t";
    else if (ztd::same(name, "stick"))
        cmd = "+t";
    else if (ztd::same(name, "go_w"))
        cmd = "go-w";
    else if (ztd::same(name, "go_rwx"))
        cmd = "go-rwx";
    else if (ztd::same(name, "ugo_w"))
        cmd = "ugo+w";
    else if (ztd::same(name, "ugo_rx"))
        cmd = "ugo+rX";
    else if (ztd::same(name, "ugo_rwx"))
        cmd = "ugo+rwX";
    else if (ztd::same(name, "myuser"))
        cmd = fmt::format("{}:{}", myuser, myuser);
    else if (ztd::same(name, "myuser_users"))
        cmd = fmt::format("{}:users", myuser);
    else if (ztd::same(name, "user1"))
        cmd = fmt::format("{}:{}", user1, user1);
    else if (ztd::same(name, "user1_users"))
        cmd = fmt::format("{}:users", user1);
    else if (ztd::same(name, "user2"))
        cmd = fmt::format("{}:{}", user2, user2);
    else if (ztd::same(name, "user2_users"))
        cmd = fmt::format("{}:users", user2);
    else if (ztd::same(name, "root"))
        cmd = "root:root";
    else if (ztd::same(name, "root_users"))
        cmd = "root:users";
    else if (ztd::same(name, "root_myuser"))
        cmd = fmt::format("root:{}", myuser);
    else if (ztd::same(name, "root_user1"))
        cmd = fmt::format("root:{}", user1);
    else if (ztd::same(name, "root_user2"))
        cmd = fmt::format("root:{}", user2);
    else
        return;

    std::string file_paths;
    for (VFSFileInfo* file: sel_files)
    {
        std::string file_path = bash_quote(vfs_file_info_get_name(file));
        file_paths = fmt::format("{} {}", file_paths, file_path);
    }

    // task
    PtkFileTask* ptask =
        ptk_file_exec_new(set->menu_label, cwd, GTK_WIDGET(file_browser), file_browser->task_view);
    ptask->task->exec_command = ztd::strdup(fmt::format("{} {} {}", prog, cmd, file_paths));
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_sync = true;
    ptask->task->exec_show_error = true;
    ptask->task->exec_show_output = false;
    ptask->task->exec_export = false;
    if (as_root)
        ptask->task->exec_as_user = "root";
    ptk_file_task_run(ptask);
}

void
ptk_file_browser_on_action(PtkFileBrowser* browser, XSetName setname)
{
    int i = 0;
    XSet* set = xset_get(setname);
    FMMainWindow* main_window = static_cast<FMMainWindow*>(browser->main_window);
    char mode = main_window->panel_context[browser->mypanel - 1];

    // LOG_INFO("ptk_file_browser_on_action {}", set->name);

    if (Glib::str_has_prefix(set->name, "book_"))
    {
        if (set->xset_name == XSetName::BOOK_ICON || set->xset_name == XSetName::BOOK_MENU_ICON)
            ptk_bookmark_view_update_icons(nullptr, browser);
        else if (set->xset_name == XSetName::BOOK_ADD)
        {
            const char* text = browser->path_bar && gtk_widget_has_focus(browser->path_bar)
                                   ? gtk_entry_get_text(GTK_ENTRY(browser->path_bar))
                                   : nullptr;
            if (text && (std::filesystem::exists(text) || strstr(text, ":/") ||
                         Glib::str_has_prefix(text, "//")))
                ptk_bookmark_view_add_bookmark(nullptr, browser, text);
            else
                ptk_bookmark_view_add_bookmark(nullptr, browser, nullptr);
        }
        else if (set->xset_name == XSetName::BOOK_OPEN && browser->side_book)
            ptk_bookmark_view_on_open_reverse(nullptr, browser);
    }
    else if (Glib::str_has_prefix(set->name, "go_"))
    {
        if (set->xset_name == XSetName::GO_BACK)
            ptk_file_browser_go_back(nullptr, browser);
        else if (set->xset_name == XSetName::GO_FORWARD)
            ptk_file_browser_go_forward(nullptr, browser);
        else if (set->xset_name == XSetName::GO_UP)
            ptk_file_browser_go_up(nullptr, browser);
        else if (set->xset_name == XSetName::GO_HOME)
            ptk_file_browser_go_home(nullptr, browser);
        else if (set->xset_name == XSetName::GO_DEFAULT)
            ptk_file_browser_go_default(nullptr, browser);
        else if (set->xset_name == XSetName::GO_SET_DEFAULT)
            ptk_file_browser_set_default_folder(nullptr, browser);
    }
    else if (Glib::str_has_prefix(set->name, "tab_"))
    {
        if (set->xset_name == XSetName::TAB_NEW)
            ptk_file_browser_new_tab(nullptr, browser);
        else if (set->xset_name == XSetName::TAB_NEW_HERE)
            ptk_file_browser_new_tab_here(nullptr, browser);
        else
        {
            if (set->xset_name == XSetName::TAB_PREV)
                i = -1;
            else if (set->xset_name == XSetName::TAB_NEXT)
                i = -2;
            else if (set->xset_name == XSetName::TAB_CLOSE)
                i = -3;
            else if (set->xset_name == XSetName::TAB_RESTORE)
                i = -4;
            else
                i = std::stol(set->name);
            ptk_file_browser_go_tab(nullptr, browser, i);
        }
    }
    else if (Glib::str_has_prefix(set->name, "focus_"))
    {
        if (set->xset_name == XSetName::FOCUS_PATH_BAR)
            i = 0;
        else if (set->xset_name == XSetName::FOCUS_FILELIST)
            i = 4;
        else if (set->xset_name == XSetName::FOCUS_DIRTREE)
            i = 1;
        else if (set->xset_name == XSetName::FOCUS_BOOK)
            i = 2;
        else if (set->xset_name == XSetName::FOCUS_DEVICE)
            i = 3;
        ptk_file_browser_focus(nullptr, browser, i);
    }
    else if (set->xset_name == XSetName::VIEW_REORDER_COL)
        on_reorder(nullptr, GTK_WIDGET(browser));
    else if (set->xset_name == XSetName::VIEW_REFRESH)
        ptk_file_browser_refresh(nullptr, browser);
    else if (set->xset_name == XSetName::VIEW_THUMB)
        main_window_toggle_thumbnails_all_windows();
    else if (Glib::str_has_prefix(set->name, "sortby_"))
    {
        i = -3;
        if (set->xset_name == XSetName::SORTBY_NAME)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_NAME;
        else if (set->xset_name == XSetName::SORTBY_SIZE)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_SIZE;
        else if (set->xset_name == XSetName::SORTBY_TYPE)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_TYPE;
        else if (set->xset_name == XSetName::SORTBY_PERM)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_PERM;
        else if (set->xset_name == XSetName::SORTBY_OWNER)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_OWNER;
        else if (set->xset_name == XSetName::SORTBY_DATE)
            i = PtkFBSortOrder::PTK_FB_SORT_BY_MTIME;
        else if (set->xset_name == XSetName::SORTBY_ASCEND)
        {
            i = -1;
            set->b =
                browser->sort_type == GTK_SORT_ASCENDING ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
        }
        else if (set->xset_name == XSetName::SORTBY_DESCEND)
        {
            i = -2;
            set->b = browser->sort_type == GTK_SORT_DESCENDING ? XSetB::XSET_B_TRUE
                                                               : XSetB::XSET_B_FALSE;
        }
        if (i > 0)
            set->b = browser->sort_order == i ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;
        on_popup_sortby(nullptr, browser, i);
    }
    else if (Glib::str_has_prefix(set->name, "sortx_"))
        ptk_file_browser_set_sort_extra(browser, set->xset_name);
    else if (set->xset_name == XSetName::PATH_HELP)
        ptk_path_entry_help(nullptr, GTK_WIDGET(browser));
    else if (Glib::str_has_prefix(set->name, "panel"))
    {
        int panel_num = set->name[5];

        // LOG_INFO("ACTION panelN={}  {}", panel_num, set->name[5]);

        if (i > 0 && i < 5)
        {
            XSet* set2;
            std::string fullxname = fmt::format("panel{}_", panel_num);
            std::string xname = ztd::removeprefix(set->name, fullxname);
            if (ztd::same(xname, "show_hidden")) // shared key
            {
                ptk_file_browser_show_hidden_files(
                    browser,
                    xset_get_b_panel(browser->mypanel, "show_hidden"));
            }
            else if (ztd::same(xname, "show")) // main View|Panel N
                show_panels_all_windows(nullptr, static_cast<FMMainWindow*>(browser->main_window));
            else if (Glib::str_has_prefix(xname, "show_")) // shared key
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
                update_views_all_windows(nullptr, browser);
                if (ztd::same(xname, "show_book") && browser->side_book)
                {
                    ptk_bookmark_view_chdir(GTK_TREE_VIEW(browser->side_book), browser, true);
                    gtk_widget_grab_focus(GTK_WIDGET(browser->side_book));
                }
            }
            else if (ztd::same(xname, "list_detailed")) // shared key
                on_popup_list_detailed(nullptr, browser);
            else if (ztd::same(xname, "list_icons")) // shared key
                on_popup_list_icons(nullptr, browser);
            else if (ztd::same(xname, "list_compact")) // shared key
                on_popup_list_compact(nullptr, browser);
            else if (ztd::same(xname, "list_large")) // shared key
            {
                if (browser->view_mode != PtkFBViewMode::PTK_FB_ICON_VIEW)
                {
                    xset_set_b_panel(browser->mypanel, "list_large", !browser->large_icons);
                    on_popup_list_large(nullptr, browser);
                }
            }
            else if (Glib::str_has_prefix(xname, "detcol_") // shared key
                     && browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
                update_views_all_windows(nullptr, browser);
            }
        }
    }
    else if (Glib::str_has_prefix(set->name, "status_"))
    {
        if (!strcmp(set->name, "status_border") || !strcmp(set->name, "status_text"))
            on_status_effect_change(nullptr, browser);
        else if (set->xset_name == XSetName::STATUS_NAME ||
                 set->xset_name == XSetName::STATUS_PATH ||
                 set->xset_name == XSetName::STATUS_INFO || set->xset_name == XSetName::STATUS_HIDE)
            on_status_middle_click_config(nullptr, set);
    }
    else if (Glib::str_has_prefix(set->name, "paste_"))
    {
        if (set->xset_name == XSetName::PASTE_LINK)
            ptk_file_browser_paste_link(browser);
        else if (set->xset_name == XSetName::PASTE_TARGET)
            ptk_file_browser_paste_target(browser);
        else if (set->xset_name == XSetName::PASTE_AS)
            ptk_file_misc_paste_as(browser, ptk_file_browser_get_cwd(browser), nullptr);
    }
    else if (Glib::str_has_prefix(set->name, "select_"))
    {
        if (set->xset_name == XSetName::SELECT_ALL)
            ptk_file_browser_select_all(nullptr, browser);
        else if (set->xset_name == XSetName::SELECT_UN)
            ptk_file_browser_unselect_all(nullptr, browser);
        else if (set->xset_name == XSetName::SELECT_INVERT)
            ptk_file_browser_invert_selection(nullptr, browser);
        else if (set->xset_name == XSetName::SELECT_PATT)
            ptk_file_browser_select_pattern(nullptr, browser, nullptr);
    }
    else // all the rest require ptkfilemenu data
    {
        ptk_file_menu_action(browser, set->name);
    }
}
