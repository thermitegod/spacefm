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

#include <memory>

#include <optional>

#include <magic_enum.hpp>

#include <glibmm.h>

#include <ztd/ztd.hxx>

// #include "logger.hxx"

#include "compat/gtk4-porting.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "ptk/natsort/strnatcmp.hxx"
#include "ptk/deprecated/async-task.hxx"

#include "ptk/ptk-app-chooser.hxx"

enum class app_chooser_column : std::uint8_t
{
    app_icon,
    app_name,
    desktop_file,
    full_path,
};

static void* load_all_known_apps_thread(async_task* task) noexcept;

static void
init_list_view(GtkTreeView* tree_view) noexcept
{
    GtkCellRenderer* renderer = nullptr;
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
sort_by_name(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b, void* user_data) noexcept
{
    (void)user_data;

    i32 ret = 0;

    g_autofree char* name_a = nullptr;
    gtk_tree_model_get(model, a, app_chooser_column::app_name, &name_a, -1);
    if (name_a)
    {
        g_autofree char* name_b = nullptr;
        gtk_tree_model_get(model, b, app_chooser_column::app_name, &name_b, -1);
        if (name_b)
        {
            ret = strnatcasecmp(name_a, name_b);
        }
    }
    return ret;
}

static void
add_list_item(GtkListStore* list_store, const std::string_view path) noexcept
{
    GtkTreeIter iter;

    // desktop file already in list?
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter))
    {
        do
        {
            g_autofree char* file = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(list_store),
                               &iter,
                               app_chooser_column::desktop_file,
                               &file,
                               -1);
            if (file)
            {
                const auto desktop = vfs::desktop::create(path);
                if (file == desktop->name())
                {
                    // already exists
                    return;
                }
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter));
    }

    const auto desktop = vfs::desktop::create(path);

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
                      GtkWidget* dialog) noexcept
{
    (void)tree_view;
    (void)path;
    (void)column;
    GtkButton* btn = GTK_BUTTON(g_object_get_data(G_OBJECT(dialog), "btn_ok"));
#if (GTK_MAJOR_VERSION == 4)
    // TODO
    (void)btn;
#elif (GTK_MAJOR_VERSION == 3)
    gtk_button_clicked(btn);
#endif
}

static void
on_load_all_apps_finish(async_task* task, bool is_cancelled, GtkWidget* dialog) noexcept
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

#if (GTK_MAJOR_VERSION == 4)
    gtk_widget_set_cursor(GTK_WIDGET(dialog), nullptr);
#elif (GTK_MAJOR_VERSION == 3)
    gdk_window_set_cursor(gtk_widget_get_window(dialog), nullptr);
#endif
}

static GtkWidget*
init_associated_apps_tab(GtkWidget* dialog,
                         const std::shared_ptr<vfs::mime_type>& mime_type) noexcept
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

    g_object_set_data(G_OBJECT(scrolled_window), "view", tree_view);

    init_list_view(GTK_TREE_VIEW(tree_view));

    const std::vector<std::string> actions = mime_type->actions();
    if (!actions.empty())
    {
        for (const std::string_view action : actions)
        {
            add_list_item(list_store, action);
        }
    }

    // clang-format off
    g_signal_connect(G_OBJECT(tree_view), "row_activated", G_CALLBACK(on_view_row_activated), dialog);
    // clang-format on

    // Add the tree view to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(tree_view));

    return GTK_WIDGET(scrolled_window);
}

static GtkWidget*
init_all_apps_tab(GtkWidget* dialog) noexcept
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

    g_object_set_data(G_OBJECT(scrolled_window), "view", tree_view);

    init_list_view(GTK_TREE_VIEW(tree_view));

    gtk_widget_grab_focus(GTK_WIDGET(tree_view));

#if (GTK_MAJOR_VERSION == 3)
    GdkCursor* busy = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(tree_view)),
                                                 GdkCursorType::GDK_WATCH);
#endif

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(tree_view)));
    gtk_widget_set_cursor(GTK_WIDGET(parent), nullptr);
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(tree_view));
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), busy);
#endif

#if (GTK_MAJOR_VERSION == 3)
    g_object_unref(busy);
#endif

    GtkListStore* list = gtk_list_store_new(magic_enum::enum_count<app_chooser_column>(),
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);
    auto* const task = async_task::create((async_task::function_t)load_all_known_apps_thread, list);
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
app_chooser_dialog(GtkWindow* parent, const std::shared_ptr<vfs::mime_type>& mime_type,
                   bool focus_all_apps, bool show_command, bool show_default,
                   bool dir_default) noexcept
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
    GtkButton* btn_cancel = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL));
    GtkButton* btn_ok = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", GtkResponseType::GTK_RESPONSE_OK));

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

    gtk_widget_set_margin_start(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dialog), 5);

    gtk_widget_set_size_request(GTK_WIDGET(dialog), 600, 600);
    // gtk_window_set_resizable(GTK_WINDOW(dialog), true);
    gtk_window_set_resizable(GTK_WINDOW(dialog), false);
#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

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

    GtkEntry* entry_command = GTK_ENTRY(gtk_entry_new());
    // gtk_widget_set_hexpand(GTK_WIDGET(entry), true);
    gtk_box_pack_start(label_entry_box, GTK_WIDGET(label_entry_label), false, false, 0);
    gtk_box_pack_start(label_entry_box, GTK_WIDGET(entry_command), true, true, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(label_entry_box), false, false, 0);

    if (!show_command)
    {
        // TODO - hide
        gtk_widget_hide(GTK_WIDGET(label_entry_box));
        gtk_label_set_text(label_title, "Please choose an application:");
    }

    // Create the notebook with two tabs
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(notebook,
                             init_associated_apps_tab(dialog, mime_type),
                             gtk_label_new("Associated Apps"));
    gtk_notebook_append_page(notebook, init_all_apps_tab(dialog), gtk_label_new("All Apps"));
    gtk_box_pack_start(vbox, GTK_WIDGET(notebook), true, true, 0);

    // Create the first checked button
    GtkCheckButton* btn_open_in_terminal = GTK_CHECK_BUTTON(gtk_check_button_new());
    gtk_button_set_label(GTK_BUTTON(btn_open_in_terminal), "Open in a terminal");
    gtk_box_pack_start(vbox, GTK_WIDGET(btn_open_in_terminal), false, false, 0);

    // Create the second checked button
    GtkCheckButton* btn_set_as_default = GTK_CHECK_BUTTON(gtk_check_button_new());
    gtk_button_set_label(GTK_BUTTON(btn_set_as_default),
                         "Set as the default application for this file type");
    gtk_box_pack_start(vbox, GTK_WIDGET(btn_set_as_default), false, false, 0);
    // Do not set default handler for directories and files with unknown type
    if (!show_default ||
        /* mime_type->type() == vfs::constants::mime_type::unknown || */
        (mime_type->type() == vfs::constants::mime_type::directory && !dir_default))
    {
        gtk_widget_hide(GTK_WIDGET(btn_set_as_default));
    }

    g_object_set_data(G_OBJECT(dialog), "notebook", notebook);
    g_object_set_data(G_OBJECT(dialog), "entry_command", entry_command);
    g_object_set_data(G_OBJECT(dialog), "btn_open_in_terminal", btn_open_in_terminal);
    g_object_set_data(G_OBJECT(dialog), "btn_set_as_default", btn_set_as_default);
    g_object_set_data(G_OBJECT(dialog), "btn_ok", btn_ok);
    g_object_set_data(G_OBJECT(dialog), "btn_cancel", btn_cancel);

    gtk_widget_show_all(GTK_WIDGET(dialog));

    if (focus_all_apps)
    {
        // select All Apps tab
        gtk_notebook_next_page(notebook);
    }

    gtk_widget_grab_focus(GTK_WIDGET(notebook));

    return dialog;
}

/*
 * Return selected application.
 * Returned string is the file name of the *.desktop file or a command line.
 * These two can be separated by check if the returned string is ended
 * with ".desktop" postfix.
 */
static std::optional<std::string>
app_chooser_dialog_get_selected_app(GtkWidget* dialog) noexcept
{
    GtkEntry* entry = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "entry_command"));

#if (GTK_MAJOR_VERSION == 4)
    std::string app = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
    std::string app = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

    if (!app.empty())
    {
        return app;
    }

    GtkNotebook* notebook = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(dialog), "notebook"));

    const auto page = gtk_notebook_get_current_page(notebook);
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_notebook_get_nth_page(notebook, page));
    GtkTreeView* tree_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(scroll), "view"));
    GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(selection, &model, &it))
    {
        g_autofree char* c_app = nullptr;
        gtk_tree_model_get(model, &it, app_chooser_column::desktop_file, &c_app, -1);
        if (c_app != nullptr)
        {
            return std::string(c_app);
        }
    }

    return std::nullopt;
}

/*
 * Check if the user set the selected app default handler.
 */
static bool
app_chooser_dialog_get_set_default(GtkWidget* dialog) noexcept
{
    GtkToggleButton* btn =
        GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), "btn_set_as_default"));

    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
}

static void
on_dialog_response(GtkDialog* dialog, i32 id, void* user_data) noexcept
{
    (void)user_data;

    if (id == GtkResponseType::GTK_RESPONSE_OK || id == GtkResponseType::GTK_RESPONSE_CANCEL ||
        id == GtkResponseType::GTK_RESPONSE_NONE ||
        id == GtkResponseType::GTK_RESPONSE_DELETE_EVENT)
    {
        auto* const task = ASYNC_TASK(g_object_get_data(G_OBJECT(dialog), "task"));
        if (task)
        {
            // logger::info<logger::domain::ptk>("app-chooser.cxx -> async_task_cancel");
            // see note in vfs-async-task.c: async_task_real_cancel()
            task->cancel();
            // The GtkListStore will be freed in
            // spacefm::signal::task_finish handler of task - on_load_all_app_finish()
            g_object_unref(task);
        }
    }
}

std::optional<std::string>
ptk_choose_app_for_mime_type(GtkWindow* parent, const std::shared_ptr<vfs::mime_type>& mime_type,
                             bool focus_all_apps, bool show_command, bool show_default,
                             bool dir_default) noexcept
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

    const auto response = gtk_dialog_run(GTK_DIALOG(dialog));
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
            else if (/* mime_type->get_type() != mime_type::type::unknown && */
                     (dir_default || mime_type->type() != vfs::constants::mime_type::directory))
            {
                const std::string custom = mime_type->add_action(app.value());
                app = custom;
            }
        }
    }

    gtk_widget_destroy(dialog);

    return app;
}

static void
load_all_apps_in_dir(const std::filesystem::path& dir_path, GtkListStore* list,
                     async_task* task) noexcept
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

        const auto filename = file.path().filename();
        const auto file_path = dir_path / filename;
        if (std::filesystem::is_directory(file_path))
        {
            /* recursively load sub dirs */
            load_all_apps_in_dir(file_path, list, task);
            continue;
        }
        if (!filename.string().ends_with(".desktop"))
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
load_all_known_apps_thread(async_task* task) noexcept
{
    GtkListStore* list = GTK_LIST_STORE(task->user_data());

    const auto dir = vfs::user::data() / "applications";
    load_all_apps_in_dir(dir, list, task);

    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        const auto sdir = sys_dir / "applications";
        load_all_apps_in_dir(sdir, list, task);
    }

    return nullptr;
}
