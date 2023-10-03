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

#include <span>
#include <vector>

#include <gtkmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "settings/app.hxx"

#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-file-task.hxx"

#include "ptk/ptk-file-actions-misc.hxx"

static bool
create_file_action_dialog(GtkWindow* parent, const std::string_view header_text,
                          const std::span<const vfs::file_info> selected_files) noexcept
{
    enum class file_action_column
    {
        name,
        size,
    };

    GtkWidget* dialog =
        gtk_dialog_new_with_buttons("Confirm File Action",
                                    parent,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    "Confirm",
                                    GtkResponseType::GTK_RESPONSE_ACCEPT,
                                    "Cancel",
                                    GtkResponseType::GTK_RESPONSE_CANCEL,
                                    nullptr);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

    gtk_widget_set_margin_start(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(dialog), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(dialog), 5);

    gtk_widget_set_size_request(GTK_WIDGET(dialog), 800, 500);
    // gtk_window_set_resizable(GTK_WINDOW(dialog), true);
    gtk_window_set_resizable(GTK_WINDOW(dialog), false);
#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

    // Set "Confirm" as the default button
    // TODO - The first column in the GtkTreeView is the active widget
    // whe the dialog is opened, should be the "Confirm" button
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GtkResponseType::GTK_RESPONSE_ACCEPT);

    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    GtkBox* box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 5));
#if (GTK_MAJOR_VERSION == 4)
    gtk_box_prepend(GTK_BOX(content_area), GTK_WIDGET(box));
#elif (GTK_MAJOR_VERSION == 3)
    gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(box));
#endif

    // header label
    GtkLabel* header_label = GTK_LABEL(gtk_label_new(header_text.data()));
    PangoAttrList* attr_list = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(header_label, attr_list);
    gtk_box_pack_start(box, GTK_WIDGET(header_label), false, false, 0);

    GtkScrolledWindow* scrolled_window =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    // gtk_scrolled_window_set_shadow_type(scrolled_window, GtkShadowType::GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_hexpand(GTK_WIDGET(scrolled_window), true);
    gtk_widget_set_vexpand(GTK_WIDGET(scrolled_window), true);

    GtkListStore* list_store = gtk_list_store_new(magic_enum::enum_count<file_action_column>(),
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);
    usize total_size_bytes = 0;
    for (const vfs::file_info& file : selected_files)
    {
        total_size_bytes += file->size();

        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store,
                           &iter,
                           file_action_column::name,
                           file->path().c_str(),
                           file_action_column::size,
                           file->display_size().data(),
                           -1);
    }

    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_view), true);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), true);

    // tree view is non-selectable
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    gtk_tree_selection_set_mode(selection, GtkSelectionMode::GTK_SELECTION_NONE);

    // add the name and size columns to the tree view
    GtkCellRenderer* cell_renderer = nullptr;
    GtkTreeViewColumn* column = nullptr;

    cell_renderer = gtk_cell_renderer_text_new();
    column =
        gtk_tree_view_column_new_with_attributes("File Name",
                                                 cell_renderer,
                                                 "text",
                                                 magic_enum::enum_integer(file_action_column::name),
                                                 nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_expand(column, true);

    cell_renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_alignment(cell_renderer, 1, 0.5); // right align file sizes
    column =
        gtk_tree_view_column_new_with_attributes("Size",
                                                 cell_renderer,
                                                 "text",
                                                 magic_enum::enum_integer(file_action_column::size),
                                                 nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    // Add the tree view to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(tree_view));

    // Add the scrolled window to the box
    gtk_box_pack_start(box, GTK_WIDGET(scrolled_window), true, true, 0);

    // Create the label for total size
    const auto total_size = std::format("Total Size: {}", vfs_file_size_format(total_size_bytes));
    GtkLabel* total_size_label = GTK_LABEL(gtk_label_new(total_size.c_str()));
    gtk_box_pack_start(box, GTK_WIDGET(total_size_label), false, false, 0);

    gtk_widget_show_all(GTK_WIDGET(dialog));

    const auto response = gtk4_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return response == GtkResponseType::GTK_RESPONSE_ACCEPT;
}

void
ptk_delete_files(GtkWindow* parent_win, const std::filesystem::path& cwd,
                 const std::span<const vfs::file_info> selected_files, GtkTreeView* task_view)
{
    (void)cwd;

    if (selected_files.empty())
    {
        ztd::logger::warn("Trying to delete an empty file list");
        return;
    }

    if (app_settings.confirm_delete())
    {
        const bool confirmed =
            create_file_action_dialog(parent_win, "Delete selected files?", selected_files);
        if (!confirmed)
        {
            return;
        }
    }

    std::vector<std::filesystem::path> file_list;
    file_list.reserve(selected_files.size());
    for (const vfs::file_info& file : selected_files)
    {
        file_list.emplace_back(file->path());
    }

    PtkFileTask* ptask = ptk_file_task_new(vfs::file_task_type::DELETE,
                                           file_list,
                                           parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                           GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}

void
ptk_trash_files(GtkWindow* parent_win, const std::filesystem::path& cwd,
                const std::span<const vfs::file_info> selected_files, GtkTreeView* task_view)
{
    (void)cwd;

    if (selected_files.empty())
    {
        ztd::logger::warn("Trying to trash an empty file list");
        return;
    }

    if (app_settings.confirm_trash())
    {
        const bool confirmed =
            create_file_action_dialog(parent_win, "Trash selected files?", selected_files);
        if (!confirmed)
        {
            return;
        }
    }

    std::vector<std::filesystem::path> file_list;
    file_list.reserve(selected_files.size());
    for (const vfs::file_info& file : selected_files)
    {
        file_list.emplace_back(file->path());
    }

    PtkFileTask* ptask = ptk_file_task_new(vfs::file_task_type::trash,
                                           file_list,
                                           parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                           GTK_WIDGET(task_view));
    ptk_file_task_run(ptask);
}
