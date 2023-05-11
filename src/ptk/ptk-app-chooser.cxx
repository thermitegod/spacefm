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
#include <string_view>

#include <format>

#include <filesystem>

#include <glibmm.h>

#include <magic_enum.hpp>

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "ptk/ptk-builder.hxx"
#include "ptk/ptk-handler.hxx"

#include "ptk/ptk-app-chooser.hxx"

enum PTKAppChooser
{
    COL_APP_ICON,
    COL_APP_NAME,
    COL_DESKTOP_FILE,
    COL_FULL_PATH,
};

static void load_all_apps_in_dir(const std::filesystem::path& dir_path, GtkListStore* list,
                                 vfs::async_task task);
static void* load_all_known_apps_thread(vfs::async_task task);

static void
init_list_view(GtkTreeView* view)
{
    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    GtkCellRenderer* renderer;

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        PTKAppChooser::COL_APP_ICON,
                                        nullptr);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        PTKAppChooser::COL_APP_NAME,
                                        nullptr);

    gtk_tree_view_append_column(view, col);

    // add tooltip
    gtk_tree_view_set_tooltip_column(view, PTKAppChooser::COL_FULL_PATH);
}

static i32
sort_by_name(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b, void* user_data)
{
    (void)user_data;
    char* name_a;
    i32 ret = 0;
    gtk_tree_model_get(model, a, PTKAppChooser::COL_APP_NAME, &name_a, -1);
    if (name_a)
    {
        char* name_b;
        gtk_tree_model_get(model, b, PTKAppChooser::COL_APP_NAME, &name_b, -1);
        if (name_b)
        {
            ret = g_ascii_strcasecmp(name_a, name_b);
            std::free(name_b);
        }
        std::free(name_a);
    }
    return ret;
}

static void
add_list_item(GtkListStore* list, const std::string_view path)
{
    GtkTreeIter it;

    // desktop file already in list?
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    {
        do
        {
            char* file = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(list),
                               &it,
                               PTKAppChooser::COL_DESKTOP_FILE,
                               &file,
                               -1);
            if (file)
            {
                const vfs::desktop desktop = vfs_get_desktop(path);
                if (ztd::same(file, desktop->get_name()))
                {
                    // already exists
                    std::free(file);
                    return;
                }
                std::free(file);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it));
    }

    const vfs::desktop desktop = vfs_get_desktop(path);

    // tooltip
    const std::string tooltip = std::format("{}\nName={}\nExec={}{}",
                                            desktop->get_full_path().string(),
                                            desktop->get_disp_name(),
                                            desktop->get_exec(),
                                            desktop->use_terminal() ? "\nTerminal=true" : "");

    GdkPixbuf* icon = desktop->get_icon(20);
    gtk_list_store_append(list, &it);
    gtk_list_store_set(list,
                       &it,
                       PTKAppChooser::COL_APP_ICON,
                       icon,
                       PTKAppChooser::COL_APP_NAME,
                       desktop->get_disp_name().data(),
                       PTKAppChooser::COL_DESKTOP_FILE,
                       desktop->get_name().data(),
                       PTKAppChooser::COL_FULL_PATH,
                       tooltip.data(),
                       -1);
    if (icon)
    {
        g_object_unref(icon);
    }
}

static GtkTreeModel*
create_model_from_mime_type(vfs::mime_type mime_type)
{
    GtkListStore* list = gtk_list_store_new(magic_enum::enum_count<PTKAppChooser>(),
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);
    if (mime_type)
    {
        std::vector<std::string> apps = mime_type->get_actions();
        const std::string type = mime_type->get_type();
        if (apps.empty() && mime_type_is_text_file("", type))
        {
            mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
        }
        if (!apps.empty())
        {
            for (const std::string_view app : apps)
            {
                add_list_item(list, app);
            }
        }
    }
    return GTK_TREE_MODEL(list);
}

static bool
on_cmdline_keypress(GtkWidget* widget, GdkEventKey* event, GtkNotebook* notebook)
{
    (void)event;
    gtk_widget_set_sensitive(GTK_WIDGET(notebook),
                             gtk_entry_get_text_length(GTK_ENTRY(widget)) == 0);
    return false;
}

static void
on_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                      GtkWidget* dlg)
{
    (void)tree_view;
    (void)path;
    (void)col;
    GtkBuilder* builder = GTK_BUILDER(g_object_get_data(G_OBJECT(dlg), "builder"));
    GtkWidget* ok = GTK_WIDGET(gtk_builder_get_object(builder, "okbutton"));
    gtk_button_clicked(GTK_BUTTON(ok));
}

static GtkWidget*
app_chooser_dialog_new(GtkWindow* parent, const vfs::mime_type& mime_type, bool focus_all_apps,
                       bool show_command, bool show_default, bool dir_default)
{
    /*
    focus_all_apps      Focus All Apps tab by default
    show_command        Show custom Command entry
    show_default        Show 'Set as default' checkbox
    dir_default         Show 'Set as default' also for type dir
    */
    GtkBuilder* builder = ptk_gtk_builder_new_from_file(PTK_DLG_APP_CHOOSER);
    GtkWidget* dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
    GtkWidget* file_type = GTK_WIDGET(gtk_builder_get_object(builder, "file_type"));

    g_object_set_data_full(G_OBJECT(dlg), "builder", builder, (GDestroyNotify)g_object_unref);

    xset_set_window_icon(GTK_WINDOW(dlg));

    const i32 width = xset_get_int(xset::name::app_dlg, xset::var::x);
    const i32 height = xset_get_int(xset::name::app_dlg, xset::var::y);
    if (width && height)
    {
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
    }
    else
    {
        gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 600);
    }

    const std::string mime_desc =
        std::format(" {}\n ( {} )", mime_type->get_description(), mime_type->get_type());
    gtk_label_set_text(GTK_LABEL(file_type), mime_desc.data());

    /* Do not set default handler for directories and files with unknown type */
    if (!show_default ||
        /*  ztd::same(mime_type->get_type(), XDG_MIME_TYPE_UNKNOWN) || */
        (ztd::same(mime_type->get_type(), XDG_MIME_TYPE_DIRECTORY) && !dir_default))
    {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "set_default")));
    }
    if (!show_command)
    {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "hbox_command")));
        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "label_command")),
                           "Please choose an application:");
    }

    GtkTreeView* view =
        GTK_TREE_VIEW(GTK_WIDGET(gtk_builder_get_object(builder, "recommended_apps")));
    GtkNotebook* notebook = GTK_NOTEBOOK(GTK_WIDGET(gtk_builder_get_object(builder, "notebook")));
    GtkEntry* entry = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "cmdline")));

    GtkTreeModel* model = create_model_from_mime_type(mime_type);
    gtk_tree_view_set_model(view, model);
    g_object_unref(G_OBJECT(model));
    init_list_view(view);
    gtk_widget_grab_focus(GTK_WIDGET(view));

    g_signal_connect(entry, "key_release_event", G_CALLBACK(on_cmdline_keypress), notebook);
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "notebook")),
                     "switch_page",
                     G_CALLBACK(on_notebook_switch_page),
                     dlg);
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "browse_btn")),
                     "clicked",
                     G_CALLBACK(on_browse_btn_clicked),
                     dlg);
    g_signal_connect(G_OBJECT(view), "row_activated", G_CALLBACK(on_view_row_activated), dlg);

    gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);

    if (focus_all_apps)
    {
        // select All Apps tab
        gtk_widget_show(dlg);
        gtk_notebook_next_page(notebook);
    }
    return dlg;
}

static void
on_load_all_apps_finish(vfs::async_task task, bool is_cancelled, GtkWidget* dlg)
{
    GtkTreeModel* model = GTK_TREE_MODEL(task->get_data());
    if (is_cancelled)
    {
        g_object_unref(model);
        return;
    }

    GtkTreeView* view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(task), "view"));

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                    PTKAppChooser::COL_APP_NAME,
                                    sort_by_name,
                                    nullptr,
                                    nullptr);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                         PTKAppChooser::COL_APP_NAME,
                                         GtkSortType::GTK_SORT_ASCENDING);

    gtk_tree_view_set_model(view, model);
    g_object_unref(model);

    gdk_window_set_cursor(gtk_widget_get_window(dlg), nullptr);
}

void
on_notebook_switch_page(GtkNotebook* notebook, GtkWidget* page, u32 page_num, void* user_data)
{
    (void)notebook;
    (void)page;
    GtkWidget* dlg = GTK_WIDGET(user_data);

    GtkBuilder* builder = GTK_BUILDER(g_object_get_data(G_OBJECT(dlg), "builder"));

    /* Load all known apps installed on the system */
    if (page_num == 1)
    {
        GtkTreeView* view = GTK_TREE_VIEW(GTK_WIDGET(gtk_builder_get_object(builder, "all_apps")));
        if (!gtk_tree_view_get_model(view))
        {
            init_list_view(view);
            gtk_widget_grab_focus(GTK_WIDGET(view));
            GdkCursor* busy = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(view)),
                                                         GdkCursorType::GDK_WATCH);
            gdk_window_set_cursor(
                gtk_widget_get_window(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(view)))),
                busy);
            g_object_unref(busy);

            GtkListStore* list = gtk_list_store_new(magic_enum::enum_count<PTKAppChooser>(),
                                                    GDK_TYPE_PIXBUF,
                                                    G_TYPE_STRING,
                                                    G_TYPE_STRING,
                                                    G_TYPE_STRING);
            vfs::async_task task =
                vfs_async_task_new((VFSAsyncFunc)load_all_known_apps_thread, list);
            g_object_set_data(G_OBJECT(task), "view", view);
            g_object_set_data(G_OBJECT(dlg), "task", task);

            task->add_event<EventType::TASK_FINISH>(on_load_all_apps_finish, dlg);

            task->run_thread();
            g_signal_connect(G_OBJECT(view),
                             "row_activated",
                             G_CALLBACK(on_view_row_activated),
                             dlg);
        }
    }
}

/*
 * Return selected application in a ``newly allocated'' string.
 * Returned string is the file name of the *.desktop file or a command line.
 * These two can be separated by check if the returned string is ended
 * with ".desktop" postfix.
 */
static char*
app_chooser_dialog_get_selected_app(GtkWidget* dlg)
{
    GtkBuilder* builder = GTK_BUILDER(g_object_get_data(G_OBJECT(dlg), "builder"));
    GtkEntry* entry = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "cmdline")));

    char* app = (char*)gtk_entry_get_text(entry);
    if (app && *app)
    {
        return ztd::strdup(app);
    }

    GtkNotebook* notebook = GTK_NOTEBOOK(GTK_WIDGET(gtk_builder_get_object(builder, "notebook")));
    const i32 idx = gtk_notebook_get_current_page(notebook);
    GtkBin* scroll = GTK_BIN(gtk_notebook_get_nth_page(notebook, idx));
    GtkTreeView* view = GTK_TREE_VIEW(gtk_bin_get_child(scroll));
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);

    GtkTreeModel* model;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(tree_sel, &model, &it))
    {
        gtk_tree_model_get(model, &it, PTKAppChooser::COL_DESKTOP_FILE, &app, -1);
    }
    else
    {
        app = nullptr;
    }
    return app;
}

/*
 * Check if the user set the selected app default handler.
 */
static bool
app_chooser_dialog_get_set_default(GtkWidget* dlg)
{
    GtkBuilder* builder = GTK_BUILDER(g_object_get_data(G_OBJECT(dlg), "builder"));
    GtkWidget* check = GTK_WIDGET(gtk_builder_get_object(builder, "set_default"));
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
}

void
on_browse_btn_clicked(GtkButton* button, void* user_data)
{
    (void)button;
    GtkWidget* parent = GTK_WIDGET(user_data);
    GtkWidget* dlg = gtk_file_chooser_dialog_new(nullptr,
                                                 GTK_WINDOW(parent),
                                                 GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                                 "Cancel",
                                                 GtkResponseType::GTK_RESPONSE_CANCEL,
                                                 "document-open",
                                                 GtkResponseType::GTK_RESPONSE_OK,
                                                 nullptr);

    xset_set_window_icon(GTK_WINDOW(dlg));

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), "/usr/bin");

    const i32 response = gtk_dialog_run(GTK_DIALOG(dlg));

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (filename)
        {
            GtkBuilder* builder = GTK_BUILDER(g_object_get_data(G_OBJECT(parent), "builder"));
            GtkEntry* entry = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "cmdline")));
            GtkNotebook* notebook =
                GTK_NOTEBOOK(GTK_WIDGET(gtk_builder_get_object(builder, "notebook")));
            /* FIXME: path should not be hard-coded */
            if (ztd::startswith(filename, "/usr/share/applications") &&
                ztd::endswith(filename, ".desktop"))
            {
                const auto app_name = std::filesystem::path(filename).filename();
                gtk_entry_set_text(entry, app_name.c_str());
            }
            else
            {
                gtk_entry_set_text(entry, filename);
            }
            std::free(filename);
            gtk_widget_set_sensitive(GTK_WIDGET(notebook), gtk_entry_get_text_length(entry) == 0);
            gtk_widget_grab_focus(GTK_WIDGET(entry));
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        }
    }
    gtk_widget_destroy(dlg);
}

static void
on_dlg_response(GtkDialog* dlg, i32 id, void* user_data)
{
    (void)user_data;
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    const i32 width = allocation.width;
    const i32 height = allocation.height;
    if (width && height)
    {
        xset_set(xset::name::app_dlg, xset::var::x, std::to_string(width));
        xset_set(xset::name::app_dlg, xset::var::y, std::to_string(height));
    }

    if (id == GtkResponseType::GTK_RESPONSE_OK || id == GtkResponseType::GTK_RESPONSE_CANCEL ||
        id == GtkResponseType::GTK_RESPONSE_NONE ||
        id == GtkResponseType::GTK_RESPONSE_DELETE_EVENT)
    {
        vfs::async_task task = VFS_ASYNC_TASK(g_object_get_data(G_OBJECT(dlg), "task"));
        if (task)
        {
            // ztd::logger::info("app-chooser.cxx -> vfs_async_task_cancel");
            // see note in vfs-async-task.c: vfs_async_task_real_cancel()
            task->cancel();
            // The GtkListStore will be freed in
            // EventType::TASK_FINISH handler of task - on_load_all_app_finish()
            g_object_unref(task);
        }
    }
}

void
ptk_app_chooser_has_handler_warn(GtkWidget* parent, const vfs::mime_type& mime_type)
{
    // is file handler set for this type?
    std::vector<xset_t> handlers = ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_FILE,
                                                                 PtkHandlerMount::HANDLER_MOUNT,
                                                                 "",
                                                                 mime_type,
                                                                 false,
                                                                 false,
                                                                 true);
    if (!handlers.empty())
    {
        const std::string msg = std::format(
            "Note:  MIME type '{}' is currently set to open with the '{}' file handler, rather "
            "than with your associated MIME application.\n\nYou may also need to disable this "
            "handler in Open|File Handlers for this type to be opened with your associated "
            "application by default.",
            mime_type->get_type(),
            handlers.front()->menu_label);
        xset_msg_dialog(parent,
                        GtkMessageType::GTK_MESSAGE_INFO,
                        "MIME Type Has Handler",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        msg);
    }
    else if (!xset_get_b(xset::name::arc_def_open))
    {
        // is archive handler set for this type?
        handlers = ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_ARC,
                                                 PtkHandlerArchive::HANDLER_EXTRACT,
                                                 "",
                                                 mime_type,
                                                 false,
                                                 false,
                                                 true);
        if (!handlers.empty())
        {
            const std::string msg = std::format(
                "Note:  MIME type '{}' is currently set to open with the '{}' archive handler, "
                "rather than with your associated MIME application.\n\nYou may also need to "
                "disable this handler in Open|Archive Defaults|Archive Handlers, OR select "
                "global option Open|Archive Defaults|Open With App, for this type to be opened "
                "with your associated application by default.",
                mime_type->get_type(),
                handlers.front()->menu_label);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_INFO,
                            "MIME Type Has Handler",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            msg);
        }
    }
}

char*
ptk_choose_app_for_mime_type(GtkWindow* parent, const vfs::mime_type& mime_type,
                             bool focus_all_apps, bool show_command, bool show_default,
                             bool dir_default)
{
    /*
    focus_all_apps      Focus All Apps tab by default
    show_command        Show custom Command entry
    show_default        Show 'Set as default' checkbox
    dir_default         Show 'Set as default' also for type dir
    */
    char* app = nullptr;

    GtkWidget* dlg = app_chooser_dialog_new(parent,
                                            mime_type,
                                            focus_all_apps,
                                            show_command,
                                            show_default,
                                            dir_default);

    g_signal_connect(dlg, "response", G_CALLBACK(on_dlg_response), nullptr);

    const i32 response = gtk_dialog_run(GTK_DIALOG(dlg));

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        app = app_chooser_dialog_get_selected_app(dlg);
        if (app)
        {
            /* The selected app is set to default action */
            /* TODO: full-featured mime editor??? */
            if (app_chooser_dialog_get_set_default(dlg))
            {
                mime_type->set_default_action(app);
                ptk_app_chooser_has_handler_warn(dlg, mime_type);
            }
            else if (/* !ztd::same(mime_type->get_type(),
                                                    XDG_MIME_TYPE_UNKNOWN) && */
                     (dir_default || !ztd::same(mime_type->get_type(), XDG_MIME_TYPE_DIRECTORY)))
            {
                const std::string custom = mime_type->add_action(app);
                std::free(app);
                app = ztd::strdup(custom);
            }
        }
    }

    gtk_widget_destroy(dlg);
    return app;
}

static void
load_all_apps_in_dir(const std::filesystem::path& dir_path, GtkListStore* list,
                     vfs::async_task task)
{
    if (!std::filesystem::is_directory(dir_path))
    {
        return;
    }

    for (const auto& file : std::filesystem::directory_iterator(dir_path))
    {
        if (task->is_cancelled())
        {
            break;
        }

        const auto file_name = file.path().filename();
        const auto file_path = dir_path / file_name;
        if (std::filesystem::is_directory(file_path))
        {
            /* recursively load sub dirs */
            load_all_apps_in_dir(file_path, list, task);
            continue;
        }
        if (!ztd::endswith(file_name.string(), ".desktop"))
        {
            continue;
        }

        if (task->is_cancelled())
        {
            break;
        }

        /* There are some operations using GTK+, so lock may be needed. */
        add_list_item(list, file_path.string());
    }
}

static void*
load_all_known_apps_thread(vfs::async_task task)
{
    GtkListStore* list = GTK_LIST_STORE(task->get_data());

    const auto dir = vfs::user_dirs->data_dir() / "applications";
    load_all_apps_in_dir(dir, list, task);

    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const auto sdir = std::filesystem::path() / sys_dir / "applications";
        load_all_apps_in_dir(sdir, list, task);
    }

    return nullptr;
}
