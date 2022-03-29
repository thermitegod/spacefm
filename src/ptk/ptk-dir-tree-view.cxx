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

#include <string>
#include <filesystem>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-dir-tree.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-file-task.hxx"

#include "ptk/ptk-dir-tree-view.hxx"

static GQuark dir_tree_view_data = 0;

static GtkTreeModel* get_dir_tree_model();

static void on_dir_tree_view_row_expanded(GtkTreeView* treeview, GtkTreeIter* iter,
                                          GtkTreePath* path, void* user_data);

static void on_dir_tree_view_row_collapsed(GtkTreeView* treeview, GtkTreeIter* iter,
                                           GtkTreePath* path, void* user_data);

static bool on_dir_tree_view_button_press(GtkWidget* view, GdkEventButton* evt,
                                          PtkFileBrowser* browser);

static bool on_dir_tree_view_key_press(GtkWidget* view, GdkEventKey* evt, PtkFileBrowser* browser);

static bool sel_func(GtkTreeSelection* selection, GtkTreeModel* model, GtkTreePath* path,
                     bool path_currently_selected, void* data);

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

// MOD drag n drop...
static void on_dir_tree_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                                int x, int y, GtkSelectionData* sel_data,
                                                unsigned int info, unsigned int time,
                                                void* user_data);
static bool on_dir_tree_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x,
                                         int y, unsigned int time, PtkFileBrowser* file_browser);

static bool on_dir_tree_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context,
                                        unsigned int time, PtkFileBrowser* file_browser);

static bool on_dir_tree_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x,
                                       int y, unsigned int time, PtkFileBrowser* file_browser);

#define GDK_ACTION_ALL (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)

static bool
filter_func(GtkTreeModel* model, GtkTreeIter* iter, void* data)
{
    VFSFileInfo* file;
    GtkTreeView* view = GTK_TREE_VIEW(data);
    bool show_hidden = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(view), dir_tree_view_data));

    if (show_hidden)
        return true;

    gtk_tree_model_get(model, iter, PTKDirTreeCol::COL_DIR_TREE_INFO, &file, -1);
    if (file)
    {
        const char* name = vfs_file_info_get_name(file);
        if (name && name[0] == '.')
        {
            vfs_file_info_unref(file);
            return false;
        }
        vfs_file_info_unref(file);
    }
    return true;
}
static void
on_destroy(GtkWidget* w)
{
    do
    {
    } while (g_source_remove_by_user_data(w));
}
/* Create a new dir tree view */
GtkWidget*
ptk_dir_tree_view_new(PtkFileBrowser* browser, bool show_hidden)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    GtkTreeModel* filter;

    GtkTreeView* dir_tree_view = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_tree_view_set_headers_visible(dir_tree_view, false);
    gtk_tree_view_set_enable_tree_lines(dir_tree_view, true);

    // MOD enabled DND   FIXME: Temporarily disable drag & drop since it does not work right now.
    /*    exo_icon_view_enable_model_drag_dest (
                EXO_ICON_VIEW( dir_tree_view ),
                drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL ); */
    gtk_tree_view_enable_model_drag_dest(
        dir_tree_view,
        drag_targets,
        sizeof(drag_targets) / sizeof(GtkTargetEntry),
        GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK));
    /*
        gtk_tree_view_enable_model_drag_source ( dir_tree_view,
                                                 ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK |
       GDK_BUTTON3_MASK ), drag_targets, sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                                                 GDK_ACTION_DEFAULT | GDK_ACTION_COPY |
       GDK_ACTION_MOVE | GDK_ACTION_LINK );
      */

    col = gtk_tree_view_column_new();

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        PTKDirTreeCol::COL_DIR_TREE_ICON,
                                        "info",
                                        PTKDirTreeCol::COL_DIR_TREE_INFO,
                                        nullptr);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        PTKDirTreeCol::COL_DIR_TREE_DISP_NAME,
                                        nullptr);

    gtk_tree_view_append_column(dir_tree_view, col);

    tree_sel = gtk_tree_view_get_selection(dir_tree_view);
    gtk_tree_selection_set_select_function(tree_sel,
                                           (GtkTreeSelectionFunc)sel_func,
                                           nullptr,
                                           nullptr);

    if (!dir_tree_view_data)
        dir_tree_view_data = g_quark_from_static_string("show_hidden");
    g_object_set_qdata(G_OBJECT(dir_tree_view), dir_tree_view_data, GINT_TO_POINTER(show_hidden));
    model = get_dir_tree_model();
    filter = gtk_tree_model_filter_new(model, nullptr);
    g_object_unref(G_OBJECT(model));
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter),
                                           (GtkTreeModelFilterVisibleFunc)filter_func,
                                           dir_tree_view,
                                           nullptr);
    gtk_tree_view_set_model(dir_tree_view, filter);
    g_object_unref(G_OBJECT(filter));

    g_signal_connect(dir_tree_view,
                     "row-expanded",
                     G_CALLBACK(on_dir_tree_view_row_expanded),
                     model);

    g_signal_connect_data(dir_tree_view,
                          "row-collapsed",
                          G_CALLBACK(on_dir_tree_view_row_collapsed),
                          model,
                          nullptr,
                          G_CONNECT_AFTER);

    g_signal_connect(dir_tree_view,
                     "button-press-event",
                     G_CALLBACK(on_dir_tree_view_button_press),
                     browser);

    g_signal_connect(dir_tree_view,
                     "key-press-event",
                     G_CALLBACK(on_dir_tree_view_key_press),
                     browser);

    // MOD drag n drop
    g_signal_connect((void*)dir_tree_view,
                     "drag-data-received",
                     G_CALLBACK(on_dir_tree_view_drag_data_received),
                     browser);
    g_signal_connect((void*)dir_tree_view,
                     "drag-motion",
                     G_CALLBACK(on_dir_tree_view_drag_motion),
                     browser);

    g_signal_connect((void*)dir_tree_view,
                     "drag-leave",
                     G_CALLBACK(on_dir_tree_view_drag_leave),
                     browser);

    g_signal_connect((void*)dir_tree_view,
                     "drag-drop",
                     G_CALLBACK(on_dir_tree_view_drag_drop),
                     browser);

    tree_path = gtk_tree_path_new_first();
    gtk_tree_view_expand_row(dir_tree_view, tree_path, false);
    gtk_tree_path_free(tree_path);

    g_signal_connect(dir_tree_view, "destroy", G_CALLBACK(on_destroy), nullptr);
    return GTK_WIDGET(dir_tree_view);
}

bool
ptk_dir_tree_view_chdir(GtkTreeView* dir_tree_view, const char* path)
{
    GtkTreeIter it;
    GtkTreeIter parent_it;
    GtkTreePath* tree_path = nullptr;

    if (!path || *path != '/')
        return false;

    char** dirs = g_strsplit(path + 1, "/", -1);

    if (!dirs)
        return false;

    GtkTreeModel* model = gtk_tree_view_get_model(dir_tree_view);

    if (!gtk_tree_model_iter_children(model, &parent_it, nullptr))
    {
        g_strfreev(dirs);
        return false;
    }

    /* special case: root dir */
    if (!dirs[0])
    {
        it = parent_it;
        tree_path = gtk_tree_model_get_path(model, &parent_it);

        g_strfreev(dirs);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(dir_tree_view), tree_path);

        gtk_tree_view_scroll_to_cell(dir_tree_view, tree_path, nullptr, false, 0.5, 0.5);

        gtk_tree_path_free(tree_path);

        return true;
    }

    char** dir;
    for (dir = dirs; *dir; ++dir)
    {
        if (!gtk_tree_model_iter_children(model, &it, &parent_it))
        {
            g_strfreev(dirs);
            return false;
        }
        bool found = false;
        VFSFileInfo* info;
        do
        {
            gtk_tree_model_get(model, &it, PTKDirTreeCol::COL_DIR_TREE_INFO, &info, -1);
            if (!info)
                continue;
            if (!strcmp(vfs_file_info_get_name(info), *dir))
            {
                tree_path = gtk_tree_model_get_path(model, &it);

                if (dir[1])
                {
                    gtk_tree_view_expand_row(dir_tree_view, tree_path, false);
                    gtk_tree_model_get_iter(model, &parent_it, tree_path);
                }
                found = true;
                vfs_file_info_unref(info);
                break;
            }
            vfs_file_info_unref(info);
        } while (gtk_tree_model_iter_next(model, &it));

        if (!found)
            return false; /* Error! */

        if (tree_path && dir[1])
        {
            gtk_tree_path_free(tree_path);
            tree_path = nullptr;
        }
    }

    g_strfreev(dirs);
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(dir_tree_view), tree_path);

    gtk_tree_view_scroll_to_cell(dir_tree_view, tree_path, nullptr, false, 0.5, 0.5);

    gtk_tree_path_free(tree_path);

    return true;
}

/* FIXME: should this API be put here? Maybe it belongs to prk-dir-tree.c */
char*
ptk_dir_view_get_dir_path(GtkTreeModel* model, GtkTreeIter* it)
{
    GtkTreeIter real_it;

    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &real_it, it);
    GtkTreeModel* tree = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));
    return ptk_dir_tree_get_dir_path(PTK_DIR_TREE(tree), &real_it);
}

/* Return a newly allocated string containing path of current selected dir. */
char*
ptk_dir_tree_view_get_selected_dir(GtkTreeView* dir_tree_view)
{
    GtkTreeModel* model;
    GtkTreeIter it;

    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(dir_tree_view);
    if (gtk_tree_selection_get_selected(tree_sel, &model, &it))
        return ptk_dir_view_get_dir_path(model, &it);
    return nullptr;
}

static GtkTreeModel*
get_dir_tree_model()
{
    static PtkDirTree* dir_tree_model = nullptr;

    if (!dir_tree_model)
    {
        dir_tree_model = ptk_dir_tree_new();
        g_object_add_weak_pointer(G_OBJECT(dir_tree_model), (void**)GTK_WIDGET(&dir_tree_model));
    }
    else
    {
        g_object_ref(G_OBJECT(dir_tree_model));
    }
    return GTK_TREE_MODEL(dir_tree_model);
}

static bool
sel_func(GtkTreeSelection* selection, GtkTreeModel* model, GtkTreePath* path,
         bool path_currently_selected, void* data)
{
    (void)selection;
    (void)path_currently_selected;
    (void)data;
    GtkTreeIter it;
    VFSFileInfo* file;

    if (!gtk_tree_model_get_iter(model, &it, path))
        return false;
    gtk_tree_model_get(model, &it, PTKDirTreeCol::COL_DIR_TREE_INFO, &file, -1);
    if (!file)
        return false;
    vfs_file_info_unref(file);
    return true;
}

void
ptk_dir_tree_view_show_hidden_files(GtkTreeView* dir_tree_view, bool show_hidden)
{
    g_object_set_qdata(G_OBJECT(dir_tree_view), dir_tree_view_data, GINT_TO_POINTER(show_hidden));
    GtkTreeModel* filter = gtk_tree_view_get_model(dir_tree_view);
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
}

static void
on_dir_tree_view_row_expanded(GtkTreeView* treeview, GtkTreeIter* iter, GtkTreePath* path,
                              void* user_data)
{
    GtkTreeIter real_it;
    GtkTreeModel* filter = gtk_tree_view_get_model(treeview);
    PtkDirTree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_it, iter);
    GtkTreePath* real_path =
        gtk_tree_model_filter_convert_path_to_child_path(GTK_TREE_MODEL_FILTER(filter), path);
    ptk_dir_tree_expand_row(tree, &real_it, real_path);
    gtk_tree_path_free(real_path);
}

static void
on_dir_tree_view_row_collapsed(GtkTreeView* treeview, GtkTreeIter* iter, GtkTreePath* path,
                               void* user_data)
{
    GtkTreeIter real_it;
    GtkTreeModel* filter = gtk_tree_view_get_model(treeview);
    PtkDirTree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_it, iter);
    GtkTreePath* real_path =
        gtk_tree_model_filter_convert_path_to_child_path(GTK_TREE_MODEL_FILTER(filter), path);
    ptk_dir_tree_collapse_row(tree, &real_it, real_path);
    gtk_tree_path_free(real_path);
}

static bool
on_dir_tree_view_button_press(GtkWidget* view, GdkEventButton* evt, PtkFileBrowser* browser)
{
    GtkTreePath* tree_path;
    GtkTreeViewColumn* tree_col;
    GtkTreeIter it;

    if (evt->type == GDK_BUTTON_PRESS && (evt->button == 1 || evt->button == 3))
    {
        // middle click 2 handled in ptk-file-browser.c on_dir_tree_button_press
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          evt->x,
                                          evt->y,
                                          &tree_path,
                                          &tree_col,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), tree_path, tree_col, false);

                if (evt->button == 3)
                {
                    // right click
                    char* dir_path = ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(view));
                    if (ptk_file_browser_chdir(browser,
                                               dir_path,
                                               PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY))
                    {
                        /* show right-click menu
                         * This simulates a right-click in the file list when
                         * no files are selected (even if some are) since
                         * actions are to be taken on the dir itself. */
                        GtkWidget* popup =
                            ptk_file_menu_new(browser, nullptr, nullptr, dir_path, nullptr);
                        if (popup)
                            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
                        gtk_tree_path_free(tree_path);
                        return true;
                    }
                }
                else
                    gtk_tree_view_row_activated(GTK_TREE_VIEW(view), tree_path, tree_col);
            }
            gtk_tree_path_free(tree_path);
        }
    }
    else if (evt->type == GDK_2BUTTON_PRESS && evt->button == 1)
    {
        // double click - expand/collapse
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          evt->x,
                                          evt->y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), tree_path))
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), tree_path);
            else
                gtk_tree_view_expand_row(GTK_TREE_VIEW(view), tree_path, false);
            gtk_tree_path_free(tree_path);
            return true;
        }
    }
    return false;
}

static bool
on_dir_tree_view_key_press(GtkWidget* view, GdkEventKey* evt, PtkFileBrowser* browser)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    GtkTreeSelection* select = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

    if (!gtk_tree_selection_get_selected(select, &model, &iter))
        return false;

    int keymod = (evt->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK |
                                GDK_HYPER_MASK | GDK_META_MASK));

    GtkTreePath* path = gtk_tree_model_get_path(model, &iter);

    switch (evt->keyval)
    {
        case GDK_KEY_Left:
            if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
            {
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), path);
            }
            else if (gtk_tree_path_up(path))
            {
                gtk_tree_selection_select_path(select, path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, nullptr, false);
            }
            else
            {
                gtk_tree_path_free(path);
                return false;
            }
            break;
        case GDK_KEY_Right:
            if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
            {
                gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, false);
            }
            else
            {
                gtk_tree_path_down(path);
                gtk_tree_selection_select_path(select, path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, nullptr, false);
            }
            break;
        case GDK_KEY_F10:
        case GDK_KEY_Menu:
            if (evt->keyval == GDK_KEY_F10 && keymod != GDK_SHIFT_MASK)
            {
                gtk_tree_path_free(path);
                return false;
            }

            char* dir_path;
            dir_path = ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(view));
            if (ptk_file_browser_chdir(browser, dir_path, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY))
            {
                /* show right-click menu
                 * This simulates a right-click in the file list when
                 * no files are selected (even if some are) since
                 * actions are to be taken on the dir itself. */
                GtkWidget* popup = ptk_file_menu_new(browser, nullptr, nullptr, dir_path, nullptr);
                if (popup)
                    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            }
            break;
        default:
            gtk_tree_path_free(path);
            return false;
    }
    gtk_tree_path_free(path);
    return true;
}

// MOD drag n drop
static char*
dir_tree_view_get_drop_dir(GtkWidget* view, int x, int y)
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path = nullptr;

    // if drag is in progress, get the dest row path
    gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(view), &tree_path, nullptr);
    if (!tree_path)
    {
        // no drag in progress, get drop path
        if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                           x,
                                           y,
                                           &tree_path,
                                           nullptr,
                                           nullptr,
                                           nullptr))
            tree_path = nullptr;
    }
    if (tree_path)
    {
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_model_get(model, &it, PTKDirTreeCol::COL_DIR_TREE_INFO, &file, -1);
            if (file)
            {
                dest_path = ptk_dir_view_get_dir_path(model, &it);
                vfs_file_info_unref(file);
            }
        }
        gtk_tree_path_free(tree_path);
    }
    return dest_path;
}

static void
on_dir_tree_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                                    GtkSelectionData* sel_data, unsigned int info,
                                    unsigned int time,
                                    void* user_data) // MOD added
{
    (void)info;
    PtkFileBrowser* file_browser = static_cast<PtkFileBrowser*>(user_data);

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-received");

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        char* dest_dir = dir_tree_view_get_drop_dir(widget, x, y);
        if (dest_dir)
        {
            char* file_path;
            char** list;
            char** puri;
            puri = list = gtk_selection_data_get_uris(sel_data);
            if (file_browser->pending_drag_status_tree)
            {
                // We only want to update drag status, not really want to drop
                dev_t dest_dev;
                struct stat statbuf; // skip stat
                if (stat(dest_dir, &statbuf) == 0)
                {
                    dest_dev = statbuf.st_dev;
                    if (file_browser->drag_source_dev_tree == 0)
                    {
                        file_browser->drag_source_dev_tree = dest_dev;
                        for (; *puri; ++puri)
                        {
                            file_path = g_filename_from_uri(*puri, nullptr, nullptr);
                            if (stat(file_path, &statbuf) == 0 && statbuf.st_dev != dest_dev)
                            {
                                file_browser->drag_source_dev_tree = statbuf.st_dev;
                                free(file_path);
                                break;
                            }
                            free(file_path);
                        }
                    }
                    if (file_browser->drag_source_dev_tree != dest_dev)
                        // src and dest are on different devices */
                        gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
                    else
                        gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
                }
                else
                    // stat failed
                    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);

                free(dest_dir);
                g_strfreev(list);
                file_browser->pending_drag_status_tree = false;
                return;
            }

            if (puri)
            {
                std::vector<std::string> file_list;
                if ((gdk_drag_context_get_selected_action(drag_context) &
                     (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)) == 0)
                {
                    gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
                }
                gtk_drag_finish(drag_context, true, false, time);

                while (*puri)
                {
                    if (**puri == '/')
                        file_path = ztd::strdup(*puri);
                    else
                        file_path = g_filename_from_uri(*puri, nullptr, nullptr);

                    if (file_path)
                        file_list.push_back(file_path);
                    ++puri;
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
                    /* Accept the drop and perform file actions */
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
            free(dest_dir);
        }
        else
            LOG_WARN("bad dest_dir in on_dir_tree_view_drag_data_received");
    }
    /* If we are only getting drag status, not finished. */
    if (file_browser->pending_drag_status_tree)
    {
        gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
        file_browser->pending_drag_status_tree = false;
        return;
    }
    gtk_drag_finish(drag_context, false, false, time);
}

static bool
on_dir_tree_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                           unsigned int time,
                           PtkFileBrowser* file_browser) // MOD added
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

static bool
on_dir_tree_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                             unsigned int time,
                             PtkFileBrowser* file_browser) // MOD added
{
    (void)x;
    (void)y;
    GdkDragAction suggested_action;

    GtkTargetList* target_list = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, target_list);
    gtk_target_list_unref(target_list);

    if (target == GDK_NONE)
        gdk_drag_status(drag_context, (GdkDragAction)0, time);
    else
    {
        // Need to set suggested_action because default handler assumes copy
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
            if (drag_action == 1)
                suggested_action = GDK_ACTION_COPY;
            else if (drag_action == 2)
                suggested_action = GDK_ACTION_MOVE;
            else if (drag_action == 3)
                suggested_action = GDK_ACTION_LINK;
            else
            {
                // automatic
                file_browser->pending_drag_status_tree = true;
                gtk_drag_get_data(widget, drag_context, target, time);
                suggested_action = gdk_drag_context_get_selected_action(drag_context);
            }
        }
        /* hack to be able to call the default handler with the correct suggested_action */
        struct _GdkDragContext
        {
            GObject parent_instance;

            /*< private >*/
            GdkDragProtocol protocol;

            /* 1.0.6 per Teklad: _GdkDragContext appears to change between
             * different versions of GTK3 which causes the crash. It appears they
             * added/removed some variables from that struct.
             * https://github.com/IgnorantGuru/spacefm/issues/670 */
            GdkDisplay* display;

            bool is_source;
            GdkWindow* source_window;
            GdkWindow* dest_window;

            GList* targets;
            GdkDragAction actions;
            GdkDragAction suggested_action;
            GdkDragAction action;

            uint32_t start_time;

            GdkDevice* device;

            /* 1.0.6 per Teklad: _GdkDragContext appears to change between
             * different versions of GTK3 which causes the crash. It appears they
             * added/removed some variables from that struct.
             * https://github.com/IgnorantGuru/spacefm/issues/670 */
            unsigned int drop_done : 1; /* Whether gdk_drag_drop_done() was performed */
        };
        ((struct _GdkDragContext*)drag_context)->suggested_action = suggested_action;
        gdk_drag_status(drag_context, suggested_action, gtk_get_current_event_time());
    }
    return false;
}

static bool
on_dir_tree_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, unsigned int time,
                            PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    (void)time;
    file_browser->drag_source_dev_tree = 0;
    return false;
}
