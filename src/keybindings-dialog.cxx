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

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "settings.hxx"

#include "xset/xset.hxx"
#include "xset/xset-keyboard.hxx"

#include "keybindings-dialog.hxx"

// TODO
//  - Search xset name
//  - checkbox to hide xsets with no bindings
//  - reload after key change

static void
on_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column,
                 void* user_data) noexcept
{
    (void)column;
    (void)user_data;

    GtkTreeIter iter;
    GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        g_autofree char* name = nullptr;
        gtk_tree_model_get(model, &iter, 0, &name, -1);
        // ztd::logger::debug("on_row_activated={}", name);
        xset_set_key(nullptr, xset_get(name));
    }
}

static GtkWidget*
init_keybindings_tab(const xset::set::keybinding_type type) noexcept
{
    GtkScrolledWindow* scrolled_window =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));

    GtkWidget* tree_view = gtk_tree_view_new();
    // Name, Keybinding
    GtkListStore* list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(list_store));
    gtk_widget_set_hexpand(GTK_WIDGET(tree_view), true);
    gtk_widget_set_vexpand(GTK_WIDGET(tree_view), true);
    g_signal_connect(G_OBJECT(tree_view), "row-activated", G_CALLBACK(on_row_activated), nullptr);
    gtk_scrolled_window_set_child(scrolled_window, tree_view);

    { // Name
        GtkTreeViewColumn* column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(column, "Name");

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, true);
        gtk_tree_view_column_add_attribute(column, renderer, "text", 0);

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }

    { // Keybinding
        GtkTreeViewColumn* column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(column, "Keybinding");

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, true);
        gtk_tree_view_column_add_attribute(column, renderer, "text", 1);

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }

    for (const auto& set : xsets)
    {
        if (set->keybinding.type != type)
        {
            continue;
        }

        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(
            list_store,
            &iter,
            0,
            set->name.c_str(),
            1,
            xset_get_keyname(set, set->keybinding.key, set->keybinding.modifier).c_str(),
            -1);
    }

    gtk_widget_show_all(GTK_WIDGET(scrolled_window));

    return GTK_WIDGET(scrolled_window);
}

static void
on_response(GtkWidget* widget, void* user_data) noexcept
{
    (void)user_data;
    GtkWidget* dialog = GTK_WIDGET(gtk_widget_get_ancestor(widget, GTK_TYPE_DIALOG));

    save_settings();

    // Close the preference dialog
    gtk_widget_destroy(dialog);
}

void
show_keybindings_dialog(GtkWindow* parent) noexcept
{
    GtkWidget* dialog =
        gtk_dialog_new_with_buttons("Preferences",
                                    parent,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    "Close",
                                    GtkResponseType::GTK_RESPONSE_OK,
                                    nullptr);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_response), nullptr);

    GtkBox* content_area = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_notebook_new());
#if (GTK_MAJOR_VERSION == 4)
    gtk_box_prepend(GTK_BOX(content_area), GTK_WIDGET(notebook));
#elif (GTK_MAJOR_VERSION == 3)
    gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(notebook));
#endif

    gtk_widget_set_margin_start(GTK_WIDGET(notebook), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(notebook), 5);
    gtk_widget_set_margin_top(GTK_WIDGET(notebook), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(notebook), 5);

    GtkWidget* tab_navigation = init_keybindings_tab(xset::set::keybinding_type::navigation);
    GtkWidget* tab_editing = init_keybindings_tab(xset::set::keybinding_type::editing);
    GtkWidget* tab_view = init_keybindings_tab(xset::set::keybinding_type::view);
    GtkWidget* tab_tabs = init_keybindings_tab(xset::set::keybinding_type::tabs);
    GtkWidget* tab_general = init_keybindings_tab(xset::set::keybinding_type::general);
    GtkWidget* tab_opening = init_keybindings_tab(xset::set::keybinding_type::opening);
    // GtkWidget* tab_invalid = init_keybindings_tab(xset::set::keybinding_type::invalid);

    gtk_notebook_append_page(notebook, tab_navigation, gtk_label_new("Navigation"));
    gtk_notebook_append_page(notebook, tab_editing, gtk_label_new("Editing"));
    gtk_notebook_append_page(notebook, tab_view, gtk_label_new("View"));
    gtk_notebook_append_page(notebook, tab_tabs, gtk_label_new("Tabs"));
    gtk_notebook_append_page(notebook, tab_general, gtk_label_new("General"));
    gtk_notebook_append_page(notebook, tab_opening, gtk_label_new("Opening"));
    // gtk_notebook_append_page(notebook, tab_invalid, gtk_label_new("Invalid"));

    gtk_widget_set_size_request(GTK_WIDGET(dialog), 800, 800);
    gtk_window_set_resizable(GTK_WINDOW(dialog), true);

#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

    gtk_widget_show_all(GTK_WIDGET(dialog));
}
