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

#include <array>

#include <ranges>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "types.hxx"

#include "settings/app.hxx"

#include "terminal-handlers.hxx"

#include "main-window.hxx"

#include "settings.hxx"

#include "xset/xset-lookup.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-location-view.hxx"

#include "preference-dialog.hxx"

class PreferenceSection
{
  private:
    GtkBox* box_{nullptr};
    GtkBox* content_box_{nullptr};

  public:
    PreferenceSection() = default;
    PreferenceSection(const std::string_view header)
    {
        this->content_box_ = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 6));

        GtkLabel* label = GTK_LABEL(gtk_label_new(header.data()));
        PangoAttrList* attr_list = pango_attr_list_new();
        PangoAttribute* attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        pango_attr_list_insert(attr_list, attr);
        gtk_label_set_attributes(label, attr_list);
        gtk_label_set_xalign(label, 0.0);
        gtk_label_set_yalign(label, 0.5);

        // clang-format off
        GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
        gtk_box_pack_start(hbox, gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0), false, false, 6);
        gtk_box_pack_start(hbox, GTK_WIDGET(this->content_box_), true, true, 0);
        // clang-format on

        this->box_ = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0));
        gtk_box_pack_start(this->box_, GTK_WIDGET(label), false, false, 0);
        gtk_box_pack_start(this->box_, GTK_WIDGET(hbox), false, false, 6);
    }

    void
    new_split_vboxes(GtkBox** left_box, GtkBox** right_box)
    {
        *left_box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 6));
        gtk_box_set_homogeneous(*left_box, false);

        *right_box = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 6));
        gtk_box_set_homogeneous(*right_box, false);

        GtkBox* hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 12));
        gtk_box_pack_start(hbox, GTK_WIDGET(*left_box), true, true, 0);
        gtk_box_pack_start(hbox, GTK_WIDGET(*right_box), false, false, 0);
        gtk_box_pack_start(this->content_box_, GTK_WIDGET(hbox), true, true, 0);
    }

    GtkBox*
    box()
    {
        return box_;
    }

    GtkBox*
    contentbox()
    {
        return content_box_;
    }
};

class PreferencePage
{
  private:
    GtkBox* box_ = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 12));
    PreferenceSection section_;

  public:
    PreferencePage()
    {
        gtk_box_set_homogeneous(this->box_, false);
        gtk_box_set_spacing(this->box_, 12);

        gtk_widget_set_margin_start(GTK_WIDGET(this->box_), 12);
        gtk_widget_set_margin_end(GTK_WIDGET(this->box_), 12);
        gtk_widget_set_margin_top(GTK_WIDGET(this->box_), 12);
        gtk_widget_set_margin_bottom(GTK_WIDGET(this->box_), 12);
    }

    void
    new_section(const std::string_view header)
    {
        this->section_ = PreferenceSection(header);

        gtk_box_pack_start(this->box_, GTK_WIDGET(this->section_.box()), false, false, 0);
    }

    void
    add_row(GtkWidget* left_item, GtkWidget* right_item = nullptr)
    {
        if (GTK_IS_LABEL(left_item))
        {
            gtk_label_set_xalign(GTK_LABEL(left_item), 0.0);
            gtk_label_set_yalign(GTK_LABEL(left_item), 0.5);
        }

        if (right_item == nullptr)
        {
            // clang-format off
            gtk_box_pack_start(this->section_.contentbox(), GTK_WIDGET(left_item), true, true, 0);
            // clang-format on
        }
        else
        {
            GtkBox* left_box;
            GtkBox* right_box;
            this->section_.new_split_vboxes(&left_box, &right_box);
            gtk_box_pack_start(left_box, GTK_WIDGET(left_item), true, true, 0);
            gtk_box_pack_start(right_box, GTK_WIDGET(right_item), true, true, 0);
        }
    }

    GtkBox*
    box()
    {
        return this->box_;
    }
};

/**
 * General Tab
 */

namespace preference::large_icons
{
    struct big_icon_sizes_data
    {
        i32 value;
        std::string_view name;
    };

    constexpr std::array<big_icon_sizes_data, 13> big_icon_sizes{{
        {512, "512"},
        {384, "384"},
        {256, "256"},
        {192, "192"},
        {128, "128"},
        {96, "96"},
        {72, "72"},
        {64, "64"},
        {48, "48"},
        {36, "36"},
        {32, "32"},
        {24, "24"},
        {22, "22"},
    }};

    static void
    changed_cb(GtkComboBox* combobox, void* user_data)
    {
        (void)user_data;

        GtkTreeIter iter;
        GtkTreeModel* model = gtk_combo_box_get_model(combobox);
        if (gtk_combo_box_get_active_iter(combobox, &iter) == false)
        {
            return;
        }

        i32 value;
        gtk_tree_model_get(model, &iter, 1, &value, -1);

        if (value != app_settings.icon_size_big())
        {
            vfs::dir::global_unload_thumbnails(true);
        }

        app_settings.icon_size_big(value);

        // update all windows/all panels/all browsers
        for (MainWindow* window : main_window_get_all())
        {
            for (const panel_t p : PANELS)
            {
                GtkNotebook* notebook = window->get_panel_notebook(p);
                const i32 total_pages = gtk_notebook_get_n_pages(notebook);
                for (const auto i : std::views::iota(0z, total_pages))
                {
                    PtkFileBrowser* file_browser =
                        PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                    // update views
                    gtk_widget_destroy(file_browser->folder_view());
                    file_browser->folder_view(nullptr);
                    if (file_browser->side_dir)
                    {
                        gtk_widget_destroy(file_browser->side_dir);
                        file_browser->side_dir = nullptr;
                    }
                    file_browser->update_views();
                }
            }
        }
        update_volume_icons();
    }

    GtkComboBox*
    create_combobox()
    {
        GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        for (const auto& action : big_icon_sizes)
        {
            GtkTreeIter iter;
            gtk_list_store_append(model, &iter);
            gtk_list_store_set(model, &iter, 0, action.name.data(), 1, action.value, -1);
        }

        GtkComboBox* box = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(model)));
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer, "text", 0, nullptr);

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            const i32 current_big_icon_size = app_settings.icon_size_big();
            do
            {
                i32 value;
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &value, -1);
                if (value == current_big_icon_size)
                {
                    gtk_combo_box_set_active_iter(box, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        }

        g_signal_connect(G_OBJECT(box), "changed", G_CALLBACK(changed_cb), nullptr);

        return box;
    }
} // namespace preference::large_icons

namespace preference::small_icons
{
    struct small_icon_sizes_data
    {
        i32 value;
        std::string_view name;
    };

    constexpr std::array<small_icon_sizes_data, 15> small_icon_sizes{{
        {512, "512"},
        {384, "384"},
        {256, "256"},
        {192, "192"},
        {128, "128"},
        {96, "96"},
        {72, "72"},
        {64, "64"},
        {48, "48"},
        {36, "36"},
        {32, "32"},
        {24, "24"},
        {22, "22"},
        {16, "16"},
        {12, "12"},
    }};

    static void
    changed_cb(GtkComboBox* combobox, void* user_data)
    {
        (void)user_data;

        GtkTreeIter iter;
        GtkTreeModel* model = gtk_combo_box_get_model(combobox);
        if (gtk_combo_box_get_active_iter(combobox, &iter) == false)
        {
            return;
        }

        i32 value;
        gtk_tree_model_get(model, &iter, 1, &value, -1);

        if (value != app_settings.icon_size_small())
        {
            vfs::dir::global_unload_thumbnails(false);
        }

        app_settings.icon_size_small(value);

        // update all windows/all panels/all browsers
        for (MainWindow* window : main_window_get_all())
        {
            for (const panel_t p : PANELS)
            {
                GtkNotebook* notebook = window->get_panel_notebook(p);
                const i32 total_pages = gtk_notebook_get_n_pages(notebook);
                for (const auto i : std::views::iota(0z, total_pages))
                {
                    PtkFileBrowser* file_browser =
                        PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                    // update views
                    gtk_widget_destroy(file_browser->folder_view());
                    file_browser->folder_view(nullptr);
                    if (file_browser->side_dir)
                    {
                        gtk_widget_destroy(file_browser->side_dir);
                        file_browser->side_dir = nullptr;
                    }
                    file_browser->update_views();
                }
            }
        }
        update_volume_icons();
    }

    GtkComboBox*
    create_combobox()
    {
        GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        for (const auto& action : small_icon_sizes)
        {
            GtkTreeIter iter;
            gtk_list_store_append(model, &iter);
            gtk_list_store_set(model, &iter, 0, action.name.data(), 1, action.value, -1);
        }

        GtkComboBox* box = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(model)));
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer, "text", 0, nullptr);

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            const i32 current_small_icon_size = app_settings.icon_size_small();
            do
            {
                i32 value;
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &value, -1);
                if (value == current_small_icon_size)
                {
                    gtk_combo_box_set_active_iter(box, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        }

        g_signal_connect(G_OBJECT(box), "changed", G_CALLBACK(changed_cb), nullptr);

        return box;
    }
} // namespace preference::small_icons

namespace preference::tool_icons
{
    struct tool_icon_sizes_data
    {
        i32 value;
        std::string_view name;
    };

    constexpr std::array<tool_icon_sizes_data, 7> tool_icon_sizes{{
        {0, "GTK Default Size"}, // GtkIconSize::GTK_ICON_SIZE_INVALID,
        {1, "Menu"},             // GtkIconSize::GTK_ICON_SIZE_MENU,
        {2, "Small Toolbar"},    // GtkIconSize::GTK_ICON_SIZE_SMALL_TOOLBAR,
        {3, "Large Toolbar"},    // GtkIconSize::GTK_ICON_SIZE_LARGE_TOOLBAR,
        {4, "Button"},           // GtkIconSize::GTK_ICON_SIZE_BUTTON,
        {5, "DND"},              // GtkIconSize::GTK_ICON_SIZE_DND,
        {6, "Dialog"},           // GtkIconSize::GTK_ICON_SIZE_DIALOG
    }};

    static void
    changed_cb(GtkComboBox* combobox, void* user_data)
    {
        (void)user_data;

        GtkTreeIter iter;
        GtkTreeModel* model = gtk_combo_box_get_model(combobox);
        if (gtk_combo_box_get_active_iter(combobox, &iter) == false)
        {
            return;
        }

        i32 value;
        gtk_tree_model_get(model, &iter, 1, &value, -1);

        if (value != app_settings.icon_size_tool())
        {
            app_settings.icon_size_tool(value);
            main_window_rebuild_all_toolbars(nullptr);
        }
    }

    GtkComboBox*
    create_combobox()
    {
        GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        for (const auto& action : tool_icon_sizes)
        {
            GtkTreeIter iter;
            gtk_list_store_append(model, &iter);
            gtk_list_store_set(model, &iter, 0, action.name.data(), 1, action.value, -1);
        }

        GtkComboBox* box = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(model)));
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer, "text", 0, nullptr);

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            const i32 current_tool_icon_size = app_settings.icon_size_tool();
            do
            {
                i32 value;
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &value, -1);
                if (value == current_tool_icon_size)
                {
                    gtk_combo_box_set_active_iter(box, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        }

        g_signal_connect(G_OBJECT(box), "changed", G_CALLBACK(changed_cb), nullptr);

        return box;
    }
} // namespace preference::tool_icons

namespace preference::single_click
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool single_click = gtk_toggle_button_get_active(button);

        if (single_click != app_settings.single_click())
        {
            app_settings.single_click(single_click);
            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = window->get_panel_notebook(p);
                    const i32 total_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : std::views::iota(0z, total_pages))
                    {
                        PtkFileBrowser* file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        file_browser->set_single_click(app_settings.single_click());
                    }
                }
            }
        }
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.single_click();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::single_click

namespace preference::hover_selects
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool single_hover = gtk_toggle_button_get_active(button);

        if (single_hover != app_settings.single_hover())
        {
            app_settings.single_hover(single_hover);
            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = window->get_panel_notebook(p);
                    const i32 total_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : std::views::iota(0z, total_pages))
                    {
                        PtkFileBrowser* file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        // TODO
                        (void)file_browser;
                        // file_browser->set_single_hover(app_settings.single_hover());
                    }
                }
            }
        }
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.single_hover();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::hover_selects

namespace preference::thumbnail_show
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool show_thumbnail = gtk_toggle_button_get_active(button);

        // thumbnail settings are changed
        if (app_settings.show_thumbnail() != show_thumbnail)
        {
            app_settings.show_thumbnail(show_thumbnail);
            // update all windows/all panels/all browsers
            main_window_reload_thumbnails_all_windows();
        }
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.show_thumbnail();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::thumbnail_show

namespace preference::thumbnail_size_limits
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.thumbnail_size_limit(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.thumbnail_size_limit();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::thumbnail_size_limits

namespace preference::thumbnailer_api
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.thumbnailer_use_api(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.thumbnailer_use_api();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::thumbnailer_api

namespace preference::thumbnail_max_size
{
    void
    spinner_cb(GtkSpinButton* spinbutton, void* user_data)
    {
        (void)user_data;
        const double value = gtk_spin_button_get_value(spinbutton);

        // convert size from MiB to B
        const u32 max_thumb_size = static_cast<u32>(value) * 1024 * 1024;

        if (app_settings.max_thumb_size() != max_thumb_size)
        {
            app_settings.max_thumb_size(max_thumb_size);
            // update all windows/all panels/all browsers
            main_window_reload_thumbnails_all_windows();
        }
    }

    GtkSpinButton*
    create_pref_spinner(double scale, double lower, double upper, double step_incr,
                        double page_incr, i32 digits)
    {
        const double value = app_settings.max_thumb_size() / scale;

        GtkAdjustment* adjustment =
            gtk_adjustment_new(value, lower, upper, step_incr, page_incr, 0.0);
        GtkSpinButton* spinner = GTK_SPIN_BUTTON(gtk_spin_button_new(adjustment, 0.0, digits));
        gtk_widget_set_size_request(GTK_WIDGET(spinner), 80, -1);
        g_signal_connect(G_OBJECT(spinner), "value-changed", G_CALLBACK(spinner_cb), nullptr);
        return spinner;
    }
} // namespace preference::thumbnail_max_size

/**
 * Interface Tab
 */

namespace preference::show_tab_bar
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool always_show_tabs = gtk_toggle_button_get_active(button);

        if (always_show_tabs != app_settings.always_show_tabs())
        {
            app_settings.always_show_tabs(always_show_tabs);
            for (MainWindow* window : main_window_get_all())
            { // update all windows/all panels
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = window->get_panel_notebook(p);
                    const i32 n = gtk_notebook_get_n_pages(notebook);
                    if (always_show_tabs)
                    {
                        gtk_notebook_set_show_tabs(notebook, true);
                    }
                    else if (n == 1)
                    {
                        gtk_notebook_set_show_tabs(notebook, false);
                    }
                }
            }
        }
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.always_show_tabs();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::show_tab_bar

namespace preference::hide_close_tab
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool show_close_tab_buttons = gtk_toggle_button_get_active(button);

        if (show_close_tab_buttons != app_settings.show_close_tab_buttons())
        {
            app_settings.show_close_tab_buttons(show_close_tab_buttons);
            for (MainWindow* window : main_window_get_all())
            { // update all windows/all panels/all browsers
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = window->get_panel_notebook(p);
                    const i32 total_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : std::views::iota(0z, total_pages))
                    {
                        PtkFileBrowser* file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        GtkWidget* tab_label = window->create_tab_label(file_browser);
                        gtk_notebook_set_tab_label(notebook,
                                                   GTK_WIDGET(file_browser),
                                                   GTK_WIDGET(tab_label));
                        file_browser->update_tab_label();
                    }
                }
            }
        }
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.show_close_tab_buttons();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::hide_close_tab

namespace preference::confirm
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.confirm(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.confirm();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::confirm

namespace preference::confirm_trash
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.confirm_trash(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.confirm_trash();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::confirm_trash

namespace preference::confirm_delete
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.confirm_delete(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.confirm_delete();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::confirm_delete

namespace preference::si_prefix
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.use_si_prefix(value);

        main_window_refresh_all();
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.use_si_prefix();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::si_prefix

namespace preference::click_executes
{
    void
    check_button_cb(GtkToggleButton* button, void* user_data)
    {
        (void)user_data;
        const bool value = gtk_toggle_button_get_active(button);
        app_settings.click_executes(value);
    }

    GtkCheckButton*
    create_pref_check_button(const std::string_view label)
    {
        const bool value = app_settings.click_executes();
        GtkCheckButton* button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label.data()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
        g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(check_button_cb), nullptr);
        return button;
    }
} // namespace preference::click_executes

namespace preference::drag_actions
{
    struct drag_actions_data
    {
        i32 value;
        std::string_view name;
    };

    constexpr std::array<drag_actions_data, 4> drag_actions_new{{
        {0, "Automatic"},
        {1, "Copy (Ctrl+Drag)"},
        {2, "Move (Shift+Drag)"},
        {3, "Link (Ctrl+Shift+Drag)"},
    }};

    static void
    changed_cb(GtkComboBox* combobox, void* user_data)
    {
        (void)user_data;

        GtkTreeIter iter;
        GtkTreeModel* model = gtk_combo_box_get_model(combobox);
        if (gtk_combo_box_get_active_iter(combobox, &iter) == false)
        {
            return;
        }

        i32 value;
        gtk_tree_model_get(model, &iter, 1, &value, -1);

        const i32 last_drag_action = xset_get_int(xset::name::drag_action, xset::var::x);
        if (value == last_drag_action)
        {
            return;
        }

        xset_set(xset::name::drag_action, xset::var::x, std::to_string(value));
    }

    GtkComboBox*
    create_combobox()
    {
        GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        for (const auto& action : drag_actions_new)
        {
            GtkTreeIter iter;
            gtk_list_store_append(model, &iter);
            gtk_list_store_set(model, &iter, 0, action.name.data(), 1, action.value, -1);
        }

        GtkComboBox* box = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(model)));
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer, "text", 0, nullptr);

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            const i32 current_drag_action = xset_get_int(xset::name::drag_action, xset::var::x);
            do
            {
                i32 value;
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &value, -1);
                if (value == current_drag_action)
                {
                    gtk_combo_box_set_active_iter(box, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        }

        g_signal_connect(G_OBJECT(box), "changed", G_CALLBACK(changed_cb), nullptr);

        return box;
    }
} // namespace preference::drag_actions

/**
 * Advanced Tab
 */

namespace preference::date_format
{
    static void
    pref_text_box_cb(GtkEntry* entry, const void* user_data)
    {
        (void)user_data;

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

        app_settings.date_format(text);

        // main_window_refresh_all();
    }

    GtkEntry*
    create_pref_text_box()
    {
        GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(entry), app_settings.date_format().data());
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(entry), app_settings.date_format().data());
#endif

        g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(pref_text_box_cb), nullptr);

        return entry;
    }
} // namespace preference::date_format

namespace preference::editor
{
    static void
    pref_text_box_cb(GtkEntry* entry, const void* user_data)
    {
        (void)user_data;

#if (GTK_MAJOR_VERSION == 4)
        const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
        const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

        xset_set(xset::name::editor, xset::var::s, text);
    }

    GtkEntry*
    create_pref_text_box()
    {
        const auto editor = xset_get_s(xset::name::editor).value_or("");

        GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(entry), editor.data());
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(entry), editor.data());
#endif

        g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(pref_text_box_cb), nullptr);

        return entry;
    }
} // namespace preference::editor

namespace preference::terminal
{
    static void
    changed_cb(GtkComboBox* combobox, void* user_data)
    {
        (void)user_data;

        GtkTreeIter iter;
        GtkTreeModel* model = gtk_combo_box_get_model(combobox);
        if (gtk_combo_box_get_active_iter(combobox, &iter) == false)
        {
            return;
        }

        i32 value;
        gtk_tree_model_get(model, &iter, 1, &value, -1);

        const auto new_terminal = terminal_handlers->get_supported_terminal_names()[value];
        const std::filesystem::path terminal = Glib::find_program_in_path(new_terminal.data());
        if (terminal.empty())
        {
            ztd::logger::error("Failed to set new terminal: {}, not installed", new_terminal);
            return;
        }
        xset_set(xset::name::main_terminal, xset::var::s, new_terminal);
        xset_set_b(xset::name::main_terminal, true); // discovery
    }

    GtkComboBox*
    create_combobox()
    {
        GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        const auto terminals = terminal_handlers->get_supported_terminal_names();
        for (const auto [index, terminal] : std::views::enumerate(terminals))
        {
            GtkTreeIter iter;
            gtk_list_store_append(model, &iter);
            gtk_list_store_set(model, &iter, 0, terminal.data(), 1, index, -1);
        }

        GtkComboBox* box = GTK_COMBO_BOX(gtk_combo_box_new_with_model(GTK_TREE_MODEL(model)));
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer, "text", 0, nullptr);

        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            const auto current_terminal = xset_get_s(xset::name::main_terminal).value_or("");
            const i32 current_terminal_index =
                ztd::index(terminal_handlers->get_supported_terminal_names(), current_terminal);
            do
            {
                i32 value;
                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &value, -1);
                if (value == current_terminal_index)
                {
                    gtk_combo_box_set_active_iter(box, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        }

        g_signal_connect(G_OBJECT(box), "changed", G_CALLBACK(changed_cb), nullptr);

        return box;
    }
} // namespace preference::terminal

/////////////////////////////////////////////////////////////////

GtkWidget*
init_general_tab()
{
    auto page = PreferencePage();

    page.new_section("Icons");

    page.add_row(GTK_WIDGET(gtk_label_new("Large Icons:")),
                 GTK_WIDGET(preference::large_icons::create_combobox()));

    page.add_row(GTK_WIDGET(gtk_label_new("Small Icons:")),
                 GTK_WIDGET(preference::small_icons::create_combobox()));

    page.add_row(GTK_WIDGET(gtk_label_new("Tool Icons:")),
                 GTK_WIDGET(preference::tool_icons::create_combobox()));

    page.new_section("File List");

    page.add_row(
        GTK_WIDGET(preference::single_click::create_pref_check_button("Single Click Opens Files")));

    page.add_row(
        GTK_WIDGET(preference::hover_selects::create_pref_check_button("Hovering Selects Files")));

    page.new_section("Thumbnails");

    page.add_row(
        GTK_WIDGET(preference::thumbnail_show::create_pref_check_button("Show Thumbnails")));

    page.add_row(GTK_WIDGET(
        preference::thumbnail_size_limits::create_pref_check_button("Thumbnail Size Limits")));

    page.add_row(
        GTK_WIDGET(gtk_label_new("Max Image Size To Thumbnail")),
        GTK_WIDGET(
            preference::thumbnail_max_size::create_pref_spinner(1024 * 1024, 0, 1024, 1, 10, 0)));

    page.add_row(
        GTK_WIDGET(preference::thumbnailer_api::create_pref_check_button("Thumbnailer use API")));

    return GTK_WIDGET(page.box());
}

GtkWidget*
init_interface_tab()
{
    auto page = PreferencePage();

    page.new_section("Tabs");

    page.add_row(
        GTK_WIDGET(preference::show_tab_bar::create_pref_check_button("Always Show The Tab Bar")));

    page.add_row(GTK_WIDGET(
        preference::hide_close_tab::create_pref_check_button("Hide 'Close Tab' Buttons")));

    page.new_section("Confirming");

    page.add_row(GTK_WIDGET(preference::confirm::create_pref_check_button("Confirm Some Actions")));

    page.add_row(
        GTK_WIDGET(preference::confirm_trash::create_pref_check_button("Confirm File Trashing")));

    page.add_row(
        GTK_WIDGET(preference::confirm_delete::create_pref_check_button("Confirm File Deleting")));

    page.new_section("Unit Sizes");

    page.add_row(
        GTK_WIDGET(preference::si_prefix::create_pref_check_button("SI File Sizes (1k = 1000)")));

    page.new_section("Other");

    page.add_row(
        GTK_WIDGET(preference::click_executes::create_pref_check_button("Click Runs Executables")));

    page.add_row(
        GTK_WIDGET(preference::click_executes::create_pref_check_button("Click Runs Executables")));

    page.add_row(GTK_WIDGET(gtk_label_new("Default Drag Action:")),
                 GTK_WIDGET(preference::drag_actions::create_combobox()));

    return GTK_WIDGET(page.box());
}

GtkWidget*
init_advanced_tab()
{
    auto page = PreferencePage();

    page.new_section("Terminal");

    page.add_row(GTK_WIDGET(gtk_label_new("Terminal:")),
                 GTK_WIDGET(preference::terminal::create_combobox()));

    page.new_section("Editor");

    page.add_row(GTK_WIDGET(gtk_label_new("Editor")),
                 GTK_WIDGET(preference::editor::create_pref_text_box()));

    page.new_section("Date Format");

    page.add_row(GTK_WIDGET(gtk_label_new("Date Format")),
                 GTK_WIDGET(preference::date_format::create_pref_text_box()));

    return GTK_WIDGET(page.box());
}

void
on_response(GtkWidget* widget, void* user_data)
{
    (void)user_data;
    GtkWidget* dialog = GTK_WIDGET(gtk_widget_get_ancestor(widget, GTK_TYPE_DIALOG));

    save_settings();

    // Close the preference dialog
    gtk_widget_destroy(dialog);
}

void
show_preference_dialog(GtkWindow* parent) noexcept
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

    // clang-format off
    // Add Setting Pages
    gtk_notebook_append_page(notebook, init_general_tab(), gtk_label_new("General"));
    gtk_notebook_append_page(notebook, init_interface_tab(), gtk_label_new("Interface"));
    gtk_notebook_append_page(notebook, init_advanced_tab(), gtk_label_new("Advanced"));
    // clang-format on

    // gtk_widget_set_size_request(GTK_WIDGET(dialog), 500, 800);
    // gtk_window_set_resizable(GTK_WINDOW(dialog), true);
    gtk_window_set_resizable(GTK_WINDOW(dialog), false);

#if (GTK_MAJOR_VERSION == 3)
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GdkWindowTypeHint::GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

    gtk_widget_show_all(GTK_WIDGET(dialog));
}
