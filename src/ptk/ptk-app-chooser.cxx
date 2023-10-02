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

#include <optional>

#include <cassert>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-async-task.hxx"

#include "ptk/ptk-app-chooser.hxx"

enum class app_chooser_column
{
    app_icon,
    app_name,
    desktop_file,
    full_path,
};

struct app_chooser_dialog_data
{
    GtkNotebook* notebook{nullptr};

    GtkEntry* entry_command{nullptr};

    GtkWidget* btn_open_in_terminal{nullptr};
    GtkWidget* btn_set_as_default{nullptr};

    GtkButton* btn_ok{nullptr};
    GtkButton* btn_cancel{nullptr};
};

static void* load_all_known_apps_thread(vfs::async_task task);

static void
init_list_view(GtkTreeView* tree_view)
{
    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column = gtk_tree_view_column_new();

    gtk_tree_view_column_set_title(column, "Applications");

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, false);
    gtk_tree_view_column_set_attributes(column,
                                        renderer,
                                        "pixbuf",
                                        app_chooser_column::app_icon,
                                        nullptr);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, true);
    gtk_tree_view_column_set_attributes(column,
                                        renderer,
                                        "text",
                                        app_chooser_column::app_name,
                                        nullptr);

    gtk_tree_view_append_column(tree_view, column);

    // add tooltip
    gtk_tree_view_set_tooltip_column(tree_view,
                                     magic_enum::enum_integer(app_chooser_column::full_path));
}

static i32
sort_by_name(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b, void* user_data)
{
    (void)user_data;
    char* name_a;
    i32 ret = 0;
    gtk_tree_model_get(model, a, app_chooser_column::app_name, &name_a, -1);
    if (name_a)
    {
        char* name_b;
        gtk_tree_model_get(model, b, app_chooser_column::app_name, &name_b, -1);
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
add_list_item(GtkListStore* list_store, const std::string_view path)
{
    GtkTreeIter iter;

    // desktop file already in list?
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter))
    {
        do
        {
            char* file = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(list_store),
                               &iter,
                               app_chooser_column::desktop_file,
                               &file,
                               -1);
            if (file)
            {
                const vfs::desktop desktop = vfs_get_desktop(path);
                if (ztd::same(file, desktop->name()))
                {
                    // already exists
                    std::free(file);
                    return;
                }
                std::free(file);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter));
    }

    const vfs::desktop desktop = vfs_get_desktop(path);

    // tooltip
    const std::string tooltip = std::format("{}\nName={}\nExec={}\nTerminal={}",
                                            desktop->path().string(),
                                            desktop->display_name(),
                                            desktop->exec(),
                                            desktop->use_terminal());

    GdkPixbuf* icon = desktop->icon(20);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store,
                       &iter,
                       app_chooser_column::app_icon,
                       icon,
                       app_chooser_column::app_name,
                       desktop->display_name().data(),
                       app_chooser_column::desktop_file,
                       desktop->name().data(),
                       app_chooser_column::full_path,
                       tooltip.data(),
                       -1);
    if (icon)
    {
        g_object_unref(icon);
    }
}

static void
on_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column,
                      GtkWidget* dialog)
{
    (void)tree_view;
    (void)path;
    (void)column;
    auto data = static_cast<app_chooser_dialog_data*>(g_object_get_data(G_OBJECT(dialog), "data"));
    assert(data != nullptr);
    gtk_button_clicked(data->btn_ok);
}

static void
on_load_all_apps_finish(vfs::async_task task, bool is_cancelled, GtkWidget* dialog)
{
    GtkTreeModel* model = GTK_TREE_MODEL(task->user_data());
    if (is_cancelled)
    {
        g_object_unref(model);
        return;
    }

    GtkTreeView* tree_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(task), "view"));

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                    magic_enum::enum_integer(app_chooser_column::app_name),
                                    sort_by_name,
                                    nullptr,
                                    nullptr);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                         magic_enum::enum_integer(app_chooser_column::app_name),
                                         GtkSortType::GTK_SORT_ASCENDING);

    gtk_tree_view_set_model(tree_view, model);
    g_object_unref(model);

    gdk_window_set_cursor(gtk_widget_get_window(dialog), nullptr);
}

GtkWidget*
init_associated_apps_tab(GtkWidget* dialog, vfs::mime_type mime_type)
{
    GtkScrolledWindow* scrolled_window =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_scrolled_window_set_policy(scrolled_window,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    // gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GtkShadowType::GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_hexpand(GTK_WIDGET(scrolled_window), true);
    gtk_widget_set_vexpand(GTK_WIDGET(scrolled_window), true);

    GtkListStore* list_store = gtk_list_store_new(magic_enum::enum_count<app_chooser_column>(),
                                                  GDK_TYPE_PIXBUF,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);

    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_view), true);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), true);

    init_list_view(GTK_TREE_VIEW(tree_view));

    if (mime_type)
    {
        std::vector<std::string> apps = mime_type->actions();
        const auto type = mime_type->type();
        if (apps.empty() && mime_type_is_text_file("", type))
        {
            mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
        }
        if (!apps.empty())
        {
            for (const std::string_view app : apps)
            {
                add_list_item(list_store, app);
            }
        }
    }

    // clang-format off
    g_signal_connect(G_OBJECT(tree_view), "row_activated", G_CALLBACK(on_view_row_activated), dialog);
    // clang-format on

    // Add the tree view to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(tree_view));

    return GTK_WIDGET(scrolled_window);
}

GtkWidget*
init_all_apps_tab(GtkWidget* dialog)
{
    GtkScrolledWindow* scrolled_window =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    // gtk_scrolled_window_set_shadow_type(scrolled_window, GtkShadowType::GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_hexpand(GTK_WIDGET(scrolled_window), true);
    gtk_widget_set_vexpand(GTK_WIDGET(scrolled_window), true);

    GtkListStore* list_store = gtk_list_store_new(magic_enum::enum_count<app_chooser_column>(),
                                                  GDK_TYPE_PIXBUF,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);

    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_view), true);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), true);

    init_list_view(GTK_TREE_VIEW(tree_view));

    gtk_widget_grab_focus(GTK_WIDGET(tree_view));
    GdkCursor* busy = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(tree_view)),
                                                 GdkCursorType::GDK_WATCH);

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(tree_view)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(tree_view));
#endif
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), busy);
    g_object_unref(busy);

    GtkListStore* list = gtk_list_store_new(magic_enum::enum_count<app_chooser_column>(),
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);
    vfs::async_task task = vfs_async_task_new((VFSAsyncFunc)load_all_known_apps_thread, list);
    g_object_set_data(G_OBJECT(task), "view", tree_view);
    g_object_set_data(G_OBJECT(dialog), "task", task);

    task->add_event<spacefm::signal::task_finish>(on_load_all_apps_finish, dialog);

    task->run();

    // clang-format off
    g_signal_connect(G_OBJECT(tree_view), "row_activated", G_CALLBACK(on_view_row_activated), dialog);
    // clang-format on

    // Add the tree view to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(tree_view));

    return GTK_WIDGET(scrolled_window);
}

static GtkWidget*
app_chooser_dialog(GtkWindow* parent, const vfs::mime_type& mime_type, bool focus_all_apps,
                   bool show_command, bool show_default, bool dir_default)
{
    // focus_all_apps      Focus All Apps tab by default
    // show_command        Show custom Command entry
    // show_default        Show 'Set as default' checkbox
    // dir_default         Show 'Set as default' also for type dir

    GtkWidget* dialog =
        gtk_dialog_new_with_buttons("App Chooser",
                                    parent,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    // "Cancel",
                                    // GtkResponseType::GTK_RESPONSE_CANCEL,
                                    // "OK",
                                    // GtkResponseType::GTK_RESPONSE_OK,
                                    nullptr,
                                    nullptr);

    // dialog widgets
    const auto data = new app_chooser_dialog_data;
    assert(data != nullptr);
    g_object_set_data(G_OBJECT(dialog), "data", data);

    data->btn_cancel = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL));
    data->btn_ok = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", GtkResponseType::GTK_RESPONSE_OK));

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

    gtk_widget_set_margin_start(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dialog), 5);

    gtk_widget_set_size_request(GTK_WIDGET(dialog), 600, 600);
    // gtk_window_set_resizable(GTK_WINDOW(dialog), true);
    gtk_window_set_resizable(GTK_WINDOW(dialog), false);
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);

    // Create a vertical box to hold the dialog contents
    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    GtkBox* vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
#if (GTK_MAJOR_VERSION == 4)
    gtk_box_prepend(GTK_BOX(content_area), GTK_WIDGET(vbox));
#elif (GTK_MAJOR_VERSION == 3)
    gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(vbox));
#endif

    // Bold Text
    PangoAttrList* attr_list = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);

    // Create the header label
    GtkLabel* label_title = GTK_LABEL(gtk_label_new("Choose an application or enter a command:"));
    gtk_label_set_xalign(label_title, 0.0);
    gtk_label_set_yalign(label_title, 0.5);
    gtk_box_pack_start(vbox, GTK_WIDGET(label_title), false, false, 0);

    // Create the file type label
    GtkBox* label_file_type_box =
        GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 5));
    GtkLabel* label_file_type = GTK_LABEL(gtk_label_new("File Type:"));
    gtk_label_set_attributes(label_file_type, attr_list);
    gtk_label_set_xalign(label_file_type, 0.0);
    gtk_label_set_yalign(label_file_type, 0.5);

    const auto mime_desc = std::format(" {}\n ( {} )", mime_type->description(), mime_type->type());
    GtkLabel* label_file_type_content = GTK_LABEL(gtk_label_new(mime_desc.data()));
    gtk_label_set_xalign(label_file_type, 0.0);
    gtk_label_set_yalign(label_file_type, 0.5);

    gtk_box_pack_start(label_file_type_box, GTK_WIDGET(label_file_type), false, false, 0);
    gtk_box_pack_start(label_file_type_box, GTK_WIDGET(label_file_type_content), false, false, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(label_file_type_box), false, false, 0);

    // Create the label with an entry box
    GtkBox* label_entry_box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 5));
    GtkLabel* label_entry_label = GTK_LABEL(gtk_label_new("Command:"));
    gtk_label_set_attributes(label_entry_label, attr_list);
    gtk_label_set_xalign(label_entry_label, 0.0);
    gtk_label_set_yalign(label_entry_label, 0.5);

    data->entry_command = GTK_ENTRY(gtk_entry_new());
    // gtk_widget_set_hexpand(GTK_WIDGET(entry), true);
    gtk_box_pack_start(label_entry_box, GTK_WIDGET(label_entry_label), false, false, 0);
    gtk_box_pack_start(label_entry_box, GTK_WIDGET(data->entry_command), true, true, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(label_entry_box), false, false, 0);

    if (!show_command)
    {
        // TODO - hide
        gtk_widget_hide(GTK_WIDGET(label_entry_box));
        gtk_label_set_text(label_title, "Please choose an application:");
    }

    // Create the notebook with two tabs
    data->notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(data->notebook,
                             init_associated_apps_tab(dialog, mime_type),
                             gtk_label_new("Associated Apps"));
    gtk_notebook_append_page(data->notebook, init_all_apps_tab(dialog), gtk_label_new("All Apps"));
    gtk_box_pack_start(vbox, GTK_WIDGET(data->notebook), true, true, 0);

    // Create the first checked button
    data->btn_open_in_terminal = gtk_check_button_new_with_label("Open in a terminal");
    gtk_box_pack_start(vbox, data->btn_open_in_terminal, false, false, 0);

    // Create the second checked button
    data->btn_set_as_default =
        gtk_check_button_new_with_label("Set as the default application for this file type");
    gtk_box_pack_start(vbox, data->btn_set_as_default, false, false, 0);
    // Do not set default handler for directories and files with unknown type
    if (!show_default ||
        /*  ztd::same(mime_type->type(), XDG_MIME_TYPE_UNKNOWN) || */
        (ztd::same(mime_type->type(), XDG_MIME_TYPE_DIRECTORY) && !dir_default))
    {
        gtk_widget_hide(GTK_WIDGET(data->btn_set_as_default));
    }

    gtk_widget_show_all(GTK_WIDGET(dialog));

    if (focus_all_apps)
    {
        // select All Apps tab
        gtk_notebook_next_page(data->notebook);
    }

    gtk_widget_grab_focus(GTK_WIDGET(data->notebook));

    return dialog;
}

/*
 * Return selected application.
 * Returned string is the file name of the *.desktop file or a command line.
 * These two can be separated by check if the returned string is ended
 * with ".desktop" postfix.
 */
const std::optional<std::string>
app_chooser_dialog_get_selected_app(GtkWidget* dialog)
{
    auto data = static_cast<app_chooser_dialog_data*>(g_object_get_data(G_OBJECT(dialog), "data"));
    assert(data != nullptr);

    const std::string app = gtk_entry_get_text(data->entry_command);
    if (!app.empty())
    {
        return app;
    }

    const i32 idx = gtk_notebook_get_current_page(data->notebook);
    GtkBin* scroll = GTK_BIN(gtk_notebook_get_nth_page(data->notebook, idx));
    GtkTreeView* tree_view = GTK_TREE_VIEW(gtk_bin_get_child(scroll));
    GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel* model;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(selection, &model, &it))
    {
        char* c_app = nullptr;
        gtk_tree_model_get(model, &it, app_chooser_column::desktop_file, &c_app, -1);
        if (c_app != nullptr)
        {
            const std::string selected_app = c_app;
            std::free(c_app);
            return selected_app;
        }
    }

    return std::nullopt;
}

/*
 * Check if the user set the selected app default handler.
 */
static bool
app_chooser_dialog_get_set_default(GtkWidget* dialog)
{
    auto data = static_cast<app_chooser_dialog_data*>(g_object_get_data(G_OBJECT(dialog), "data"));
    assert(data != nullptr);
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->btn_set_as_default));
}

static void
on_dialog_response(GtkDialog* dialog, i32 id, void* user_data)
{
    (void)user_data;

    if (id == GtkResponseType::GTK_RESPONSE_OK || id == GtkResponseType::GTK_RESPONSE_CANCEL ||
        id == GtkResponseType::GTK_RESPONSE_NONE ||
        id == GtkResponseType::GTK_RESPONSE_DELETE_EVENT)
    {
        vfs::async_task task = VFS_ASYNC_TASK(g_object_get_data(G_OBJECT(dialog), "task"));
        if (task)
        {
            // ztd::logger::info("app-chooser.cxx -> vfs_async_task_cancel");
            // see note in vfs-async-task.c: vfs_async_task_real_cancel()
            task->cancel();
            // The GtkListStore will be freed in
            // spacefm::signal::task_finish handler of task - on_load_all_app_finish()
            g_object_unref(task);
        }
    }
}

const std::optional<std::string>
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

    GtkWidget* dialog = app_chooser_dialog(parent,
                                           mime_type,
                                           focus_all_apps,
                                           show_command,
                                           show_default,
                                           dir_default);

    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_dialog_response), nullptr);

    std::optional<std::string> app = std::nullopt;

    const auto response = gtk4_dialog_run(GTK_DIALOG(dialog));
    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        app = app_chooser_dialog_get_selected_app(dialog);
        if (app && !app.value().empty())
        {
            // The selected app is set to default action
            // TODO: full-featured mime editor?
            if (app_chooser_dialog_get_set_default(dialog))
            {
                mime_type->set_default_action(app.value());
            }
            else if (/* !ztd::same(mime_type->get_type(), XDG_MIME_TYPE_UNKNOWN) && */
                     (dir_default || !ztd::same(mime_type->type(), XDG_MIME_TYPE_DIRECTORY)))
            {
                const std::string custom = mime_type->add_action(app.value());
                app = custom;
            }
        }
    }

    auto data = static_cast<app_chooser_dialog_data*>(g_object_get_data(G_OBJECT(dialog), "data"));
    assert(data != nullptr);
    delete data;

    gtk_widget_destroy(dialog);

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
        if (task->is_canceled())
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

        if (task->is_canceled())
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
    GtkListStore* list = GTK_LIST_STORE(task->user_data());

    const auto dir = vfs::user_dirs->data_dir() / "applications";
    load_all_apps_in_dir(dir, list, task);

    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const auto sdir = std::filesystem::path() / sys_dir / "applications";
        load_all_apps_in_dir(sdir, list, task);
    }

    return nullptr;
}
