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

#include <chrono>

#include <array>
#include <vector>

#include <sstream>

#include <chrono>

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "settings/app.hxx"

#include "terminal-handlers.hxx"

#include "pref-dialog.hxx"
#include "main-window.hxx"

#include "extern.hxx"

#include "ptk/ptk-builder.hxx"
#include "ptk/ptk-error.hxx"
#include "ptk/ptk-location-view.hxx"

struct FMPrefDlg
{
    GtkWidget* dlg;
    GtkWidget* notebook;
    GtkWidget* encoding;
    GtkWidget* bm_open_method;
    GtkWidget* max_thumb_size;
    GtkWidget* show_thumbnail;
    GtkWidget* thumb_label1;
    GtkWidget* thumb_label2;
    GtkWidget* terminal;
    GtkWidget* big_icon_size;
    GtkWidget* small_icon_size;
    GtkWidget* tool_icon_size;
    GtkWidget* single_click;
    GtkWidget* single_hover;
    GtkWidget* use_si_prefix;
    GtkWidget* root_bar;
    GtkWidget* drag_action;

    /* Interface tab */
    GtkWidget* always_show_tabs;
    GtkWidget* hide_close_tab_buttons;

    // MOD
    GtkWidget* confirm_delete;
    GtkWidget* click_exec;
    GtkWidget* su_command;
    GtkWidget* date_format;
    GtkWidget* date_display;
    GtkWidget* editor;
    GtkWidget* editor_terminal;
    GtkWidget* root_editor;
    GtkWidget* root_editor_terminal;
};

static FMPrefDlg* data = nullptr;

inline constexpr std::array<GtkIconSize, 7> tool_icon_sizes{
    GtkIconSize(0),
    GtkIconSize::GTK_ICON_SIZE_MENU,
    GtkIconSize::GTK_ICON_SIZE_SMALL_TOOLBAR,
    GtkIconSize::GTK_ICON_SIZE_LARGE_TOOLBAR,
    GtkIconSize::GTK_ICON_SIZE_BUTTON,
    GtkIconSize::GTK_ICON_SIZE_DND,
    GtkIconSize::GTK_ICON_SIZE_DIALOG,
};
// also change max_icon_size in settings.c & lists in prefdlg.ui prefdlg2.ui
// see create_size in vfs-thumbnail-loader.c:_vfs_thumbnail_load()
inline constexpr std::array<u64, 13> big_icon_sizes{
    512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22};
inline constexpr std::array<u64, 15> small_icon_sizes{
    512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22, 16, 12};
inline constexpr std::array<const std::string_view, 3> date_formats{
    "%Y-%m-%d %H:%M",
    "%Y-%m-%d",
    "%Y-%m-%d %H:%M:%S",
};
inline constexpr std::array<i32, 4> drag_actions{0, 1, 2, 3};

static void
dir_unload_thumbnails(vfs::dir dir, bool user_data)
{
    vfs_dir_unload_thumbnails(dir, user_data);
}

static void
on_response(GtkDialog* dlg, i32 response, FMPrefDlg* user_data)
{
    (void)user_data;
    i32 ibig_icon = -1;
    i32 ismall_icon = -1;
    i32 itool_icon = -1;

    u64 max_thumb;
    bool show_thumbnail;
    u64 big_icon;
    u64 small_icon;
    u64 tool_icon;
    bool single_click;
    PtkFileBrowser* file_browser;
    bool use_si_prefix;

    GtkWidget* tab_label;
    /* interface settings */
    bool always_show_tabs;
    bool hide_close_tab_buttons;

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        show_thumbnail = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->show_thumbnail));
        max_thumb = static_cast<u64>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->max_thumb_size))) << 10;

        /* interface settings */

        always_show_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->always_show_tabs));
        if (always_show_tabs != app_settings.get_always_show_tabs())
        {
            app_settings.set_always_show_tabs(always_show_tabs);
            // update all windows/all panels
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
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
        hide_close_tab_buttons =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->hide_close_tab_buttons));
        if (hide_close_tab_buttons != app_settings.get_show_close_tab_buttons())
        {
            app_settings.set_show_close_tab_buttons(hide_close_tab_buttons);
            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
                    const i32 num_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : ztd::range(num_pages))
                    {
                        file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        tab_label = main_window_create_tab_label(window, file_browser);
                        gtk_notebook_set_tab_label(notebook, GTK_WIDGET(file_browser), tab_label);
                        main_window_update_tab_label(window, file_browser, file_browser->dir->path);
                    }
                }
            }
        }

        // ===============================================================

        /* thumbnail settings are changed */
        if (app_settings.get_show_thumbnail() != show_thumbnail ||
            app_settings.get_max_thumb_size() != max_thumb)
        {
            app_settings.set_show_thumbnail(!show_thumbnail); // toggle reverses this
            app_settings.set_max_thumb_size(max_thumb);
            // update all windows/all panels/all browsers + desktop
            main_window_toggle_thumbnails_all_windows();
        }

        /* icon sizes are changed? */
        ibig_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->big_icon_size));
        big_icon = ibig_icon >= 0 ? big_icon_sizes[ibig_icon] : app_settings.get_icon_size_big();
        ismall_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->small_icon_size));
        small_icon =
            ismall_icon >= 0 ? small_icon_sizes[ismall_icon] : app_settings.get_icon_size_small();
        itool_icon = gtk_combo_box_get_active(GTK_COMBO_BOX(data->tool_icon_size));
        if (itool_icon >= 0 && itool_icon <= GtkIconSize::GTK_ICON_SIZE_DIALOG)
        {
            tool_icon = tool_icon_sizes[itool_icon];
        }

        if (big_icon != app_settings.get_icon_size_big() ||
            small_icon != app_settings.get_icon_size_small())
        {
            vfs_mime_type_set_icon_size_big(big_icon);
            vfs_mime_type_set_icon_size_small(small_icon);

            vfs_file_info_set_thumbnail_size_big(big_icon);
            vfs_file_info_set_thumbnail_size_small(small_icon);

            /* unload old thumbnails (icons of *.desktop files will be unloaded here, too)  */
            if (big_icon != app_settings.get_icon_size_big())
            {
                vfs_dir_foreach(&dir_unload_thumbnails, true);
            }
            if (small_icon != app_settings.get_icon_size_small())
            {
                vfs_dir_foreach(&dir_unload_thumbnails, false);
            }

            app_settings.set_icon_size_big(big_icon);
            app_settings.set_icon_size_small(small_icon);

            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
                    const i32 num_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : ztd::range(num_pages))
                    {
                        file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        // update views
                        gtk_widget_destroy(file_browser->folder_view);
                        file_browser->folder_view = nullptr;
                        if (file_browser->side_dir)
                        {
                            gtk_widget_destroy(file_browser->side_dir);
                            file_browser->side_dir = nullptr;
                        }
                        ptk_file_browser_update_views(nullptr, file_browser);
                    }
                }
            }
            update_volume_icons();
        }

        if (tool_icon != app_settings.get_icon_size_tool())
        {
            app_settings.set_icon_size_tool(tool_icon);
            main_window_rebuild_all_toolbars(nullptr);
        }

        /* unit settings changed? */
        bool need_refresh = false;
        use_si_prefix = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_si_prefix));
        if (use_si_prefix != app_settings.get_use_si_prefix())
        {
            app_settings.set_use_si_prefix(use_si_prefix);
            need_refresh = true;
        }

        // date format
        const char* etext =
            gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(data->date_format))));
        const auto date_format = xset_get_s(xset::name::date_format);
        if (date_format)
        {
            if (!ztd::same(etext, date_format.value()))
            {
                if (etext)
                {
                    xset_set(xset::name::date_format, xset::var::s, etext);
                }
                else
                {
                    xset_set(xset::name::date_format, xset::var::s, "%Y-%m-%d %H:%M");
                }
                app_settings.set_date_format(date_format.value());
                need_refresh = true;
            }
        }

        if (need_refresh)
        {
            main_window_refresh_all();
        }

        /* single click changed? */
        single_click = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->single_click));
        if (single_click != app_settings.get_single_click())
        {
            app_settings.set_single_click(single_click);
            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
                    const i32 num_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : ztd::range(num_pages))
                    {
                        file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        ptk_file_browser_set_single_click(file_browser,
                                                          app_settings.get_single_click());
                    }
                }
            }
        }

        /* single click - hover selects changed? */
        const bool single_hover =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->single_hover));
        if (single_hover != app_settings.get_single_hover())
        {
            app_settings.set_single_hover(single_hover);
            // update all windows/all panels/all browsers
            for (MainWindow* window : main_window_get_all())
            {
                for (const panel_t p : PANELS)
                {
                    GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
                    const i32 num_pages = gtk_notebook_get_n_pages(notebook);
                    for (const auto i : ztd::range(num_pages))
                    {
                        file_browser =
                            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                        ptk_file_browser_set_single_click_timeout(
                            file_browser,
                            app_settings.get_single_hover() ? SINGLE_CLICK_TIMEOUT : 0);
                    }
                }
            }
        }

        // MOD
        app_settings.set_click_executes(
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->click_exec)));
        app_settings.set_confirm_delete(
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->confirm_delete)));

        const std::string s =
            std::to_string(gtk_combo_box_get_active(GTK_COMBO_BOX(data->drag_action)));
        xset_set(xset::name::drag_action, xset::var::x, s);

        // terminal su command
        const i32 idx = gtk_combo_box_get_active(GTK_COMBO_BOX(data->su_command));
        if (idx > -1)
        {
            xset_set(xset::name::su_command, xset::var::s, su_commands.at(idx));
        }

        // MOD editors
        xset_set(xset::name::editor, xset::var::s, gtk_entry_get_text(GTK_ENTRY(data->editor)));
        xset_set_b(xset::name::editor,
                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->editor_terminal)));
        const char* root_editor = gtk_entry_get_text(GTK_ENTRY(data->root_editor));
        const auto old_root_editor = xset_get_s(xset::name::root_editor);
        if (!old_root_editor)
        {
            if (root_editor)
            {
                xset_set(xset::name::root_editor, xset::var::s, root_editor);
            }
        }
        else if (old_root_editor && !ztd::same(root_editor, old_root_editor.value()))
        {
            xset_set(xset::name::root_editor, xset::var::s, root_editor);
        }
        if (!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal)) !=
            !!xset_get_b(xset::name::root_editor))
        {
            xset_set_b(xset::name::root_editor,
                       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal)));
        }

        // MOD terminal
        const auto old_terminal = xset_get_s(xset::name::main_terminal);
        const std::string new_terminal =
            gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->terminal));
        if (old_terminal && !ztd::same(new_terminal, old_terminal.value()))
        {
            xset_set(xset::name::main_terminal, xset::var::s, new_terminal);
        }
        // report missing terminal
        const std::string terminal = Glib::find_program_in_path(new_terminal);
        if (terminal.empty())
        {
            const std::string msg =
                std::format("Unable to find terminal program '{}'", new_terminal);
            ptk_show_error(GTK_WINDOW(dlg), "Error", msg);
        }

        /* save to config file */
        save_settings(nullptr);

        if (xset_get_b(xset::name::main_terminal))
        {
            xset_set_b(xset::name::main_terminal, false);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dlg));
    std::free(data);
    data = nullptr;
}

static void
on_date_format_changed(GtkComboBox* widget, FMPrefDlg* fm_data)
{
    (void)widget;

    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const char* etext =
        gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(fm_data->date_format))));

    std::tm* local_time = std::localtime(&now);
    std::ostringstream date;
    date << std::put_time(local_time, etext);

    gtk_label_set_text(GTK_LABEL(fm_data->date_display), date.str().data());
}

static void
on_single_click_toggled(GtkWidget* widget, FMPrefDlg* fm_data)
{
    (void)widget;
    gtk_widget_set_sensitive(
        fm_data->single_hover,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fm_data->single_click)));
}

static void
on_show_thumbnail_toggled(GtkWidget* widget, FMPrefDlg* fm_data)
{
    (void)widget;
    gtk_widget_set_sensitive(
        fm_data->max_thumb_size,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fm_data->show_thumbnail)));
    gtk_widget_set_sensitive(
        fm_data->thumb_label1,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fm_data->show_thumbnail)));
    gtk_widget_set_sensitive(
        fm_data->thumb_label2,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fm_data->show_thumbnail)));
}

bool
edit_preference(GtkWindow* parent, i32 page)
{
    i32 ibig_icon = -1;
    i32 ismall_icon = -1;
    i32 itool_icon = -1;
    GtkWidget* dlg;

    if (!data)
    {
        GtkTreeModel* model;
        // this invokes GVFS-RemoteVolumeMonitor via IsSupported
        GtkBuilder* builder = ptk_gtk_builder_new_from_file(PTK_DLG_PREFERENCES);

        if (!builder)
        {
            return false;
        }

        data = g_new0(FMPrefDlg, 1);
        dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
        if (parent)
        {
            gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
        }
        xset_set_window_icon(GTK_WINDOW(dlg));

        data->dlg = dlg;
        data->notebook = GTK_WIDGET(gtk_builder_get_object(builder, "notebook"));

        /* Setup 'General' tab */

        data->encoding = GTK_WIDGET(gtk_builder_get_object(builder, "filename_encoding"));
        data->bm_open_method = GTK_WIDGET(gtk_builder_get_object(builder, "bm_open_method"));
        data->show_thumbnail = GTK_WIDGET(gtk_builder_get_object(builder, "show_thumbnail"));
        data->thumb_label1 = GTK_WIDGET(gtk_builder_get_object(builder, "thumb_label1"));
        data->thumb_label2 = GTK_WIDGET(gtk_builder_get_object(builder, "thumb_label2"));
        data->max_thumb_size = GTK_WIDGET(gtk_builder_get_object(builder, "max_thumb_size"));
        data->terminal = GTK_WIDGET(gtk_builder_get_object(builder, "terminal"));
        data->big_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "big_icon_size"));
        data->small_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "small_icon_size"));
        data->tool_icon_size = GTK_WIDGET(gtk_builder_get_object(builder, "tool_icon_size"));
        data->single_click = GTK_WIDGET(gtk_builder_get_object(builder, "single_click"));
        data->single_hover = GTK_WIDGET(gtk_builder_get_object(builder, "single_hover"));
        data->use_si_prefix = GTK_WIDGET(gtk_builder_get_object(builder, "use_si_prefix"));
        data->root_bar = GTK_WIDGET(gtk_builder_get_object(builder, "root_bar"));
        data->drag_action = GTK_WIDGET(gtk_builder_get_object(builder, "drag_action"));

        model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_combo_box_set_model(GTK_COMBO_BOX(data->terminal), model);
        gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(data->terminal), 0);
        g_object_unref(model);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->max_thumb_size),
                                  app_settings.get_max_thumb_size() >> 10);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->show_thumbnail),
                                     app_settings.get_show_thumbnail());
        g_signal_connect(data->show_thumbnail,
                         "toggled",
                         G_CALLBACK(on_show_thumbnail_toggled),
                         data);
        gtk_widget_set_sensitive(data->max_thumb_size, app_settings.get_show_thumbnail());
        gtk_widget_set_sensitive(data->thumb_label1, app_settings.get_show_thumbnail());
        gtk_widget_set_sensitive(data->thumb_label2, app_settings.get_show_thumbnail());

        const auto supported_terminals = terminal_handlers->get_supported_terminal_names();
        for (const std::string_view terminal : supported_terminals)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->terminal), terminal.data());
        }

        const auto main_terminal = xset_get_s(xset::name::main_terminal);
        if (main_terminal)
        {
            bool set = false;
            for (const auto [index, value] : ztd::enumerate(supported_terminals))
            {
                if (ztd::same(main_terminal.value(), value))
                {
                    gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(data->terminal),
                                                    main_terminal.value().c_str());
                    gtk_combo_box_set_active(GTK_COMBO_BOX(data->terminal), index);
                    break;
                }
            }
            if (!set)
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(data->terminal), 0);
            }
        }

        for (const u64 big_icon_size : big_icon_sizes)
        {
            if (big_icon_size == app_settings.get_icon_size_big())
            {
                ibig_icon = big_icon_size;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->big_icon_size), ibig_icon);

        for (const u64 small_icon_size : small_icon_sizes)
        {
            if (small_icon_size == app_settings.get_icon_size_small())
            {
                ismall_icon = small_icon_size;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->small_icon_size), ismall_icon);

        itool_icon = 0;
        for (const u64 tool_icon_size : tool_icon_sizes)
        {
            if (tool_icon_size == app_settings.get_icon_size_tool())
            {
                itool_icon = tool_icon_size;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->tool_icon_size), itool_icon);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->single_click),
                                     app_settings.get_single_click());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->single_hover),
                                     app_settings.get_single_hover());
        gtk_widget_set_sensitive(data->single_hover, app_settings.get_single_click());
        g_signal_connect(data->single_click, "toggled", G_CALLBACK(on_single_click_toggled), data);

        /* Setup 'Interface' tab */

        data->always_show_tabs = GTK_WIDGET(gtk_builder_get_object(builder, "always_show_tabs"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->always_show_tabs),
                                     app_settings.get_always_show_tabs());

        data->hide_close_tab_buttons =
            GTK_WIDGET(gtk_builder_get_object(builder, "hide_close_tab_buttons"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->hide_close_tab_buttons),
                                     app_settings.get_show_close_tab_buttons());

        // MOD Interface
        data->confirm_delete = GTK_WIDGET(gtk_builder_get_object(builder, "confirm_delete"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->confirm_delete),
                                     app_settings.get_confirm_delete());
        data->click_exec = GTK_WIDGET(gtk_builder_get_object(builder, "click_exec"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->click_exec),
                                     app_settings.get_click_executes());

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->root_bar),
                                     xset_get_b(xset::name::root_bar));
        gtk_widget_set_sensitive(data->root_bar, geteuid() == 0);

        i32 drag_action_set = 0;
        for (const i32 drag_action : drag_actions)
        {
            if (drag_action == xset_get_int(xset::name::drag_action, xset::var::x))
            {
                drag_action_set = drag_action;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->drag_action), drag_action_set);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->use_si_prefix),
                                     app_settings.get_use_si_prefix());

        // Advanced Tab ==================================================

        // terminal su
        i32 su_index = 0;
        data->su_command = GTK_WIDGET(gtk_builder_get_object(builder, "su_command"));
        const auto use_su = xset_get_s(xset::name::su_command);
        if (use_su)
        {
            for (const auto [index, value] : ztd::enumerate(su_commands))
            {
                if (ztd::same(value, use_su.value()))
                {
                    su_index = static_cast<i32>(index);
                    break;
                }
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->su_command), su_index);

        // date format
        data->date_format = GTK_WIDGET(gtk_builder_get_object(builder, "date_format"));
        data->date_display = GTK_WIDGET(gtk_builder_get_object(builder, "label_date_disp"));
        model = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_STRING));
        gtk_combo_box_set_model(GTK_COMBO_BOX(data->date_format), model);
        gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(data->date_format), 0);
        g_object_unref(model);
        for (const std::string_view date_format : date_formats)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->date_format),
                                           date_format.data());
        }
        const auto date_s = xset_get_s(xset::name::date_format);
        if (date_s)
        {
            bool set = false;
            for (const auto [index, value] : ztd::enumerate(date_formats))
            {
                if (ztd::same(date_s.value(), value))
                {
                    gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(data->date_format),
                                                    date_s.value().c_str());
                    gtk_combo_box_set_active(GTK_COMBO_BOX(data->date_format), index);
                    break;
                }
            }
            if (!set)
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(data->date_format), 0);
            }
        }
        on_date_format_changed(nullptr, data);
        g_signal_connect(data->date_format, "changed", G_CALLBACK(on_date_format_changed), data);

        // editors
        data->editor = GTK_WIDGET(gtk_builder_get_object(builder, "editor"));
        const auto editor_s = xset_get_s(xset::name::editor);
        if (editor_s)
        {
            gtk_entry_set_text(GTK_ENTRY(data->editor), editor_s.value().c_str());
        }
        data->editor_terminal = GTK_WIDGET(gtk_builder_get_object(builder, "editor_terminal"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->editor_terminal),
                                     xset_get_b(xset::name::editor));
        data->root_editor = GTK_WIDGET(gtk_builder_get_object(builder, "root_editor"));
        const auto root_editor_s = xset_get_s(xset::name::editor);
        if (root_editor_s)
        {
            gtk_entry_set_text(GTK_ENTRY(data->root_editor), root_editor_s.value().c_str());
        }
        data->root_editor_terminal =
            GTK_WIDGET(gtk_builder_get_object(builder, "root_editor_terminal"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->root_editor_terminal),
                                     xset_get_b(xset::name::root_editor));

        g_signal_connect(dlg, "response", G_CALLBACK(on_response), data);
        g_object_unref(builder);
    }

    // Set current Preferences page
    const i32 desktop_page_num = 2;
    // notebook page number 3 is permanently hidden Volume Management
    if (page > desktop_page_num)
    {
        page++;
    }
    gtk_notebook_set_current_page(GTK_NOTEBOOK(data->notebook), page);

    gtk_window_present(GTK_WINDOW(data->dlg));
    return true;
}
