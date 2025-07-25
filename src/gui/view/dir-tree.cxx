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

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <cmath>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "utils/strdup.hxx"

#include "gui/dir-tree.hxx"
#include "gui/file-menu.hxx"
#include "gui/file-task.hxx"
#include "gui/utils/utils.hxx"

#include "gui/view/dir-tree.hxx"

#include "logger.hxx"

static GQuark dir_tree_view_data = 0;

static GtkTreeModel* get_dir_tree_model() noexcept;

static void on_dir_tree_view_row_expanded(GtkTreeView* treeview, GtkTreeIter* iter,
                                          GtkTreePath* path, void* user_data) noexcept;

static void on_dir_tree_view_row_collapsed(GtkTreeView* treeview, GtkTreeIter* iter,
                                           GtkTreePath* path, void* user_data) noexcept;

static bool on_dir_tree_view_button_press(GtkWidget* view, GdkEvent* event,
                                          gui::browser* browser) noexcept;

static bool on_dir_tree_view_key_press(GtkWidget* view, GdkEvent* event,
                                       gui::browser* browser) noexcept;

static bool sel_func(GtkTreeSelection* selection, GtkTreeModel* model, GtkTreePath* path,
                     bool path_currently_selected, void* data) noexcept;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{utils::strdup("text/uri-list"), 0, 0}};

// drag n drop...
static void on_dir_tree_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                                i32 x, i32 y, GtkSelectionData* sel_data, u32 info,
                                                std::time_t time, void* user_data) noexcept;
static bool on_dir_tree_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x,
                                         i32 y, std::time_t time, gui::browser* browser) noexcept;

static bool on_dir_tree_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context,
                                        std::time_t time, gui::browser* browser) noexcept;

static bool on_dir_tree_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, i32 x,
                                       i32 y, std::time_t time, gui::browser* browser) noexcept;

static bool
filter_func(GtkTreeModel* model, GtkTreeIter* iter, void* data) noexcept
{
    GtkTreeView* view = GTK_TREE_VIEW(data);
    const bool show_hidden =
        GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(view), dir_tree_view_data));

    if (show_hidden)
    {
        return true;
    }

    std::shared_ptr<vfs::file> file;
    gtk_tree_model_get(model, iter, gui::dir_tree::column::info, &file, -1);

    return !(file && file->is_hidden());
}

static void
on_destroy(GtkWidget* w) noexcept
{
    do
    {
    } while (g_source_remove_by_user_data(w));
}

/* Create a new dir tree view */
GtkWidget*
gui::view::dir_tree::create(gui::browser* browser, bool show_hidden) noexcept
{
    GtkTreeViewColumn* col = nullptr;
    GtkCellRenderer* renderer = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeSelection* selection = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* filter = nullptr;

    GtkTreeView* dir_tree_view = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_tree_view_set_headers_visible(dir_tree_view, false);
    gtk_tree_view_set_enable_tree_lines(dir_tree_view, true);

    // FIXME: Temporarily disable drag & drop since it does not work right now.
    /*    exo_icon_view_enable_model_drag_dest (
                EXO_ICON_VIEW( dir_tree_view ),
                drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL ); */
    gtk_tree_view_enable_model_drag_dest(dir_tree_view,
                                         drag_targets,
                                         sizeof(drag_targets) / sizeof(GtkTargetEntry),
                                         GdkDragAction(GdkDragAction::GDK_ACTION_MOVE |
                                                       GdkDragAction::GDK_ACTION_COPY |
                                                       GdkDragAction::GDK_ACTION_LINK));
    /*
        gtk_tree_view_enable_model_drag_source ( dir_tree_view,
                                                 ( GdkModifierType::GDK_CONTROL_MASK |
       GdkModifierType::GDK_BUTTON1_MASK | GdkModifierType::GDK_BUTTON3_MASK ), drag_targets,
       sizeof( drag_targets ) / sizeof( GtkTargetEntry ), GdkDragAction::GDK_ACTION_DEFAULT |
       GdkDragAction::GDK_ACTION_COPY | GdkDragAction::GDK_ACTION_MOVE |
       GdkDragAction::GDK_ACTION_LINK );
      */

    col = gtk_tree_view_column_new();

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        gui::dir_tree::column::icon,
                                        "info",
                                        gui::dir_tree::column::info,
                                        nullptr);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        gui::dir_tree::column::disp_name,
                                        nullptr);

    gtk_tree_view_append_column(dir_tree_view, col);

    selection = gtk_tree_view_get_selection(dir_tree_view);
    gtk_tree_selection_set_select_function(selection,
                                           (GtkTreeSelectionFunc)sel_func,
                                           nullptr,
                                           nullptr);

    if (!dir_tree_view_data)
    {
        dir_tree_view_data = g_quark_from_static_string("show_hidden");
    }
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

    // clang-format off
    g_signal_connect(G_OBJECT(dir_tree_view), "row-expanded", G_CALLBACK(on_dir_tree_view_row_expanded), model);

    g_signal_connect_data(G_OBJECT(dir_tree_view), "row-collapsed", G_CALLBACK(on_dir_tree_view_row_collapsed), model, nullptr, G_CONNECT_AFTER);

    g_signal_connect(G_OBJECT(dir_tree_view), "button-press-event", G_CALLBACK(on_dir_tree_view_button_press), browser);
    g_signal_connect(G_OBJECT(dir_tree_view), "button-press-event", G_CALLBACK(on_dir_tree_view_button_press), browser);
    g_signal_connect(G_OBJECT(dir_tree_view), "key-press-event", G_CALLBACK(on_dir_tree_view_key_press), browser);

    // drag n drop
    g_signal_connect(G_OBJECT(dir_tree_view), "drag-data-received", G_CALLBACK(on_dir_tree_view_drag_data_received), browser);
    g_signal_connect(G_OBJECT(dir_tree_view), "drag-motion", G_CALLBACK(on_dir_tree_view_drag_motion), browser);
    g_signal_connect(G_OBJECT(dir_tree_view), "drag-leave", G_CALLBACK(on_dir_tree_view_drag_leave), browser);
    g_signal_connect(G_OBJECT(dir_tree_view), "drag-drop", G_CALLBACK(on_dir_tree_view_drag_drop), browser);
    // clang-format on

    tree_path = gtk_tree_path_new_first();
    gtk_tree_view_expand_row(dir_tree_view, tree_path, false);
    gtk_tree_path_free(tree_path);

    g_signal_connect(G_OBJECT(dir_tree_view), "destroy", G_CALLBACK(on_destroy), nullptr);
    return GTK_WIDGET(dir_tree_view);
}

bool
gui::view::dir_tree::chdir(GtkTreeView* dir_tree_view, const std::filesystem::path& path) noexcept
{
    GtkTreeIter it;
    GtkTreeIter parent_it;
    GtkTreePath* tree_path = nullptr;

    if (!path.is_absolute())
    {
        return false;
    }

    GtkTreeModel* model = gtk_tree_view_get_model(dir_tree_view);

    if (!gtk_tree_model_iter_children(model, &parent_it, nullptr))
    {
        return false;
    }

    // special case: root dir
    if (std::filesystem::equivalent(path, "/"))
    {
        it = parent_it;
        tree_path = gtk_tree_model_get_path(model, &parent_it);

        gtk_tree_selection_select_path(gtk_tree_view_get_selection(dir_tree_view), tree_path);

        gtk_tree_view_scroll_to_cell(dir_tree_view, tree_path, nullptr, false, 0.5, 0.5);

        gtk_tree_path_free(tree_path);

        return true;
    }

    const std::vector<std::string> dirs = ztd::split(path.string(), "/");
    for (const std::string_view dir : dirs)
    {
        if (dir.empty())
        {
            continue; // first item will be empty because of how ztd::split works
        }

        if (!gtk_tree_model_iter_children(model, &it, &parent_it))
        {
            return false;
        }

        bool found = false;
        do
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, gui::dir_tree::column::info, &file, -1);
            if (!file)
            {
                continue;
            }

            if (file->name() == dir)
            {
                tree_path = gtk_tree_model_get_path(model, &it);

                if (dir[1])
                {
                    gtk_tree_view_expand_row(dir_tree_view, tree_path, false);
                    gtk_tree_model_get_iter(model, &parent_it, tree_path);
                }
                found = true;
                break;
            }
        } while (gtk_tree_model_iter_next(model, &it));

        if (!found)
        {
            return false; /* Error! */
        }

        if (tree_path && dir[1])
        {
            gtk_tree_path_free(tree_path);
            tree_path = nullptr;
        }
    }

    gtk_tree_selection_select_path(gtk_tree_view_get_selection(dir_tree_view), tree_path);

    gtk_tree_view_scroll_to_cell(dir_tree_view, tree_path, nullptr, false, 0.5, 0.5);

    gtk_tree_path_free(tree_path);

    return true;
}

/* FIXME: should this API be put here? Maybe it belongs to prk-dir-tree.c */
std::optional<std::filesystem::path>
gui::view::dir_tree::dir_path(GtkTreeModel* model, GtkTreeIter* it) noexcept
{
    GtkTreeIter real_it;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &real_it, it);
    GtkTreeModel* tree_model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    return tree->get_dir_path(&real_it);
}

/* Return a newly allocated string containing path of current selected dir. */
std::optional<std::filesystem::path>
gui::view::dir_tree::selected_dir(GtkTreeView* dir_tree_view) noexcept
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(dir_tree_view);
    if (gtk_tree_selection_get_selected(selection, &model, &it))
    {
        return gui::view::dir_tree::dir_path(model, &it);
    }
    return std::nullopt;
}

static GtkTreeModel*
get_dir_tree_model() noexcept
{
    static gui::dir_tree* dir_tree_model = nullptr;

    if (!dir_tree_model)
    {
        dir_tree_model = gui::dir_tree::create();
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
         bool path_currently_selected, void* data) noexcept
{
    (void)selection;
    (void)path_currently_selected;
    (void)data;

    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(model, &it, path))
    {
        return false;
    }
    std::shared_ptr<vfs::file> file;
    gtk_tree_model_get(model, &it, gui::dir_tree::column::info, &file, -1);

    return file != nullptr;
}

void
gui::view::dir_tree::show_hidden_files(GtkTreeView* dir_tree_view, bool show_hidden) noexcept
{
    g_object_set_qdata(G_OBJECT(dir_tree_view), dir_tree_view_data, GINT_TO_POINTER(show_hidden));
    GtkTreeModel* filter = gtk_tree_view_get_model(dir_tree_view);
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
}

static void
on_dir_tree_view_row_expanded(GtkTreeView* treeview, GtkTreeIter* iter, GtkTreePath* path,
                              void* user_data) noexcept
{
    GtkTreeIter real_it;
    GtkTreeModel* filter = gtk_tree_view_get_model(treeview);
    gui::dir_tree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_it, iter);
    GtkTreePath* real_path =
        gtk_tree_model_filter_convert_path_to_child_path(GTK_TREE_MODEL_FILTER(filter), path);
    tree->expand_row(&real_it, real_path);
    gtk_tree_path_free(real_path);
}

static void
on_dir_tree_view_row_collapsed(GtkTreeView* treeview, GtkTreeIter* iter, GtkTreePath* path,
                               void* user_data) noexcept
{
    GtkTreeIter real_it;
    GtkTreeModel* filter = gtk_tree_view_get_model(treeview);
    gui::dir_tree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &real_it, iter);
    GtkTreePath* real_path =
        gtk_tree_model_filter_convert_path_to_child_path(GTK_TREE_MODEL_FILTER(filter), path);
    tree->collapse_row(&real_it, real_path);
    gtk_tree_path_free(real_path);
}

static bool
on_dir_tree_view_button_press(GtkWidget* view, GdkEvent* event, gui::browser* browser) noexcept
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* tree_col = nullptr;
    GtkTreeIter it;

    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);

    double x = NAN;
    double y = NAN;
    gdk_event_get_position(event, &x, &y);

    if (type == GdkEventType::GDK_BUTTON_PRESS &&
        (button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_SECONDARY))
    {
        // middle click 2 handled in gui/file-browser.cxx on_dir_tree_button_press
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          static_cast<std::int32_t>(x),
                                          static_cast<std::int32_t>(y),
                                          &tree_path,
                                          &tree_col,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), tree_path, tree_col, false);

                if (button == GDK_BUTTON_PRIMARY)
                {
                    gtk_tree_view_row_activated(GTK_TREE_VIEW(view), tree_path, tree_col);
                }
                else
                {
                    // right click
                    const auto path =
                        gui::view::dir_tree::selected_dir(GTK_TREE_VIEW(view)).value_or("");
                    if (path.empty())
                    {
                        // path will be empty if the right click is on
                        // the  "( no subdirectory )" label.
                        return true;
                    }

                    if (browser->chdir(path))
                    {
                        /* show right-click menu
                         * This simulates a right-click in the file list when
                         * no files are selected (even if some are) since
                         * actions are to be taken on the dir itself. */
                        GtkWidget* popup = gui_file_menu_new(browser);
                        if (popup)
                        {
                            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
                        }
                        gtk_tree_path_free(tree_path);
                        return true;
                    }
                }
            }
            gtk_tree_path_free(tree_path);
        }
    }
    else if (type == GdkEventType::GDK_2BUTTON_PRESS && button == GDK_BUTTON_PRIMARY)
    {
        // double click - expand/collapse
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          static_cast<std::int32_t>(x),
                                          static_cast<std::int32_t>(y),
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), tree_path))
            {
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), tree_path);
            }
            else
            {
                gtk_tree_view_expand_row(GTK_TREE_VIEW(view), tree_path, false);
            }
            gtk_tree_path_free(tree_path);
            return true;
        }
    }
    return false;
}

static bool
on_dir_tree_view_key_press(GtkWidget* view, GdkEvent* event, gui::browser* browser) noexcept
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        return false;
    }

    const auto keymod = gui::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto keyval = gdk_key_event_get_keyval(event);

    GtkTreePath* path = gtk_tree_model_get_path(model, &iter);

    switch (keyval)
    {
        case GDK_KEY_Left:
        {
            if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
            {
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), path);
            }
            else if (gtk_tree_path_up(path))
            {
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, nullptr, false);
            }
            else
            {
                gtk_tree_path_free(path);
                return false;
            }
            break;
        }
        case GDK_KEY_Right:
        {
            if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
            {
                gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, false);
            }
            else
            {
                gtk_tree_path_down(path);
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, nullptr, false);
            }
            break;
        }
        case GDK_KEY_F10:
        case GDK_KEY_Menu:
        {
            if (keyval == GDK_KEY_F10 && keymod.data() != GdkModifierType::GDK_SHIFT_MASK)
            {
                gtk_tree_path_free(path);
                return false;
            }

            const auto dir_path = gui::view::dir_tree::selected_dir(GTK_TREE_VIEW(view));
            if (dir_path && browser->chdir(dir_path.value()))
            {
                /* show right-click menu
                 * This simulates a right-click in the file list when
                 * no files are selected (even if some are) since
                 * actions are to be taken on the dir itself. */
                GtkWidget* popup = gui_file_menu_new(browser);
                if (popup)
                {
                    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
                }
            }
            break;
        }
        default:
            gtk_tree_path_free(path);
            return false;
    }
    gtk_tree_path_free(path);
    return true;
}

// drag n drop
static std::optional<std::filesystem::path>
dir_tree_view_get_drop_dir(GtkWidget* view, i32 x, i32 y) noexcept
{
    std::optional<std::filesystem::path> dest_path = std::nullopt;

    // if drag is in progress, get the dest row path
    GtkTreePath* tree_path = nullptr;
    gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(view), &tree_path, nullptr);
    if (!tree_path)
    {
        // no drag in progress, get drop path
        if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                           x.data(),
                                           y.data(),
                                           &tree_path,
                                           nullptr,
                                           nullptr,
                                           nullptr))
        {
            tree_path = nullptr;
        }
    }
    if (tree_path)
    {
        GtkTreeIter it;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            std::shared_ptr<vfs::file> file;
            gtk_tree_model_get(model, &it, gui::dir_tree::column::info, &file, -1);
            if (file)
            {
                dest_path = gui::view::dir_tree::dir_path(model, &it);
            }
        }
        gtk_tree_path_free(tree_path);
    }
    return dest_path;
}

static void
on_dir_tree_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                                    GtkSelectionData* sel_data, u32 info, std::time_t time,
                                    void* user_data) noexcept
{
    (void)info;
    auto* browser = static_cast<gui::browser*>(user_data);

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-received");

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        const auto dest_dir = dir_tree_view_get_drop_dir(widget, x, y);
        if (dest_dir)
        {
            char** list = nullptr;
            char** puri = nullptr;
            puri = list = gtk_selection_data_get_uris(sel_data);
            if (browser->pending_drag_status_tree())
            {
                // We only want to update drag status, not really want to drop
                const auto dest_stat = ztd::stat::create(dest_dir.value());
                if (dest_stat)
                {
                    if (browser->drag_source_dev_tree_ == 0)
                    {
                        browser->drag_source_dev_tree_ = dest_stat->dev().data();
                        for (; *puri; ++puri)
                        {
                            const std::filesystem::path file_path = Glib::filename_from_uri(*puri);

                            const auto file_stat = ztd::stat::create(file_path);
                            if (file_stat && file_stat->dev() != dest_stat->dev())
                            {
                                browser->drag_source_dev_tree_ = file_stat->dev().data();
                                break;
                            }
                        }
                    }
                    if (browser->drag_source_dev_tree_ != dest_stat->dev())
                    { // src and dest are on different devices
                        gdk_drag_status(drag_context,
                                        GdkDragAction::GDK_ACTION_COPY,
                                        static_cast<guint32>(time));
                    }
                    else
                    {
                        gdk_drag_status(drag_context,
                                        GdkDragAction::GDK_ACTION_MOVE,
                                        static_cast<guint32>(time));
                    }
                }
                else
                { // stat failed
                    gdk_drag_status(drag_context,
                                    GdkDragAction::GDK_ACTION_COPY,
                                    static_cast<guint32>(time));
                }

                g_strfreev(list);
                browser->pending_drag_status_tree(false);
                return;
            }

            if (puri)
            {
                std::vector<std::filesystem::path> file_list;
                if ((gdk_drag_context_get_selected_action(drag_context) &
                     (GdkDragAction::GDK_ACTION_MOVE | GdkDragAction::GDK_ACTION_COPY |
                      GdkDragAction::GDK_ACTION_LINK)) == 0)
                {
                    gdk_drag_status(drag_context,
                                    GdkDragAction::GDK_ACTION_MOVE,
                                    static_cast<guint32>(time));
                }
                gtk_drag_finish(drag_context, true, false, static_cast<guint32>(time));

                for (; *puri; ++puri)
                {
                    std::filesystem::path file_path;
                    if (**puri == '/')
                    {
                        file_path = *puri;
                    }
                    else
                    {
                        file_path = Glib::filename_from_uri(*puri);
                    }

                    file_list.push_back(file_path);
                }
                g_strfreev(list);

                vfs::file_task::type file_action;
                switch (gdk_drag_context_get_selected_action(drag_context))
                {
                    case GdkDragAction::GDK_ACTION_COPY:
                        file_action = vfs::file_task::type::copy;
                        break;
                    case GdkDragAction::GDK_ACTION_MOVE:
                        file_action = vfs::file_task::type::move;
                        break;
                    case GdkDragAction::GDK_ACTION_LINK:
                        file_action = vfs::file_task::type::link;
                        break;
                    case GdkDragAction::GDK_ACTION_DEFAULT:
                    case GdkDragAction::GDK_ACTION_PRIVATE:
                    case GdkDragAction::GDK_ACTION_ASK:
                    default:
                        break;
                }
                if (!file_list.empty())
                {
                    /* Accept the drop and perform file actions */
                    {
                        GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));

                        gui::file_task* ptask = gui_file_task_new(file_action,
                                                                  file_list,
                                                                  dest_dir.value(),
                                                                  GTK_WINDOW(parent),
                                                                  browser->task_view());
                        ptask->run();
                    }
                }
                gtk_drag_finish(drag_context, true, false, static_cast<guint32>(time));
                return;
            }
        }
        else
        {
            logger::warn<logger::domain::gui>(
                "bad dest_dir in on_dir_tree_view_drag_data_received");
        }
    }
    /* If we are only getting drag status, not finished. */
    if (browser->pending_drag_status_tree())
    {
        gdk_drag_status(drag_context, GdkDragAction::GDK_ACTION_COPY, static_cast<guint32>(time));
        browser->pending_drag_status_tree(false);
        return;
    }
    gtk_drag_finish(drag_context, false, false, static_cast<guint32>(time));
}

static bool
on_dir_tree_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                           std::time_t time, gui::browser* browser) noexcept
{
    (void)x;
    (void)y;
    (void)browser;
    GdkAtom target = gdk_atom_intern("text/uri-list", false);

    /*  Do not call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-drop");

    gtk_drag_get_data(widget, drag_context, target, static_cast<guint32>(time));
    return true;
}

static bool
on_dir_tree_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                             std::time_t time, gui::browser* browser) noexcept
{
    (void)x;
    (void)y;
    GdkDragAction suggested_action;

    GtkTargetList* target_list = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, target_list);
    gtk_target_list_unref(target_list);

    if (target == GDK_NONE)
    {
        gdk_drag_status(drag_context, (GdkDragAction)0, static_cast<guint32>(time));
    }
    else
    {
        // Need to set suggested_action because default handler assumes copy
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
            GdkDragAction::GDK_ACTION_MOVE)
        {
            suggested_action = GdkDragAction::GDK_ACTION_MOVE;
            /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        }
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
                 GdkDragAction::GDK_ACTION_COPY)
        {
            suggested_action = GdkDragAction::GDK_ACTION_COPY;
            /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        }
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) ==
                 GdkDragAction::GDK_ACTION_LINK)
        {
            suggested_action = GdkDragAction::GDK_ACTION_LINK;
            /* Several different actions are available. We have to figure out a good default action.
             */
        }
        else
        {
            const i32 drag_action = xset_get_int(xset::name::drag_action, xset::var::x);
            if (drag_action == 1)
            {
                suggested_action = GdkDragAction::GDK_ACTION_COPY;
            }
            else if (drag_action == 2)
            {
                suggested_action = GdkDragAction::GDK_ACTION_MOVE;
            }
            else if (drag_action == 3)
            {
                suggested_action = GdkDragAction::GDK_ACTION_LINK;
            }
            else
            {
                // automatic
                browser->pending_drag_status_tree(true);
                gtk_drag_get_data(widget, drag_context, target, static_cast<guint32>(time));
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

            std::uint32_t start_time;

            GdkDevice* device;

            /* 1.0.6 per Teklad: _GdkDragContext appears to change between
             * different versions of GTK3 which causes the crash. It appears they
             * added/removed some variables from that struct.
             * https://github.com/IgnorantGuru/spacefm/issues/670 */
            std::uint32_t drop_done : 1; /* Whether gdk_drag_drop_done() was performed */
        };
        ((struct _GdkDragContext*)drag_context)->suggested_action = suggested_action;
        gdk_drag_status(drag_context, suggested_action, gtk_get_current_event_time());
    }
    return false;
}

static bool
on_dir_tree_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, std::time_t time,
                            gui::browser* browser) noexcept
{
    (void)widget;
    (void)drag_context;
    (void)time;
    browser->drag_source_dev_tree_ = 0;
    return false;
}
