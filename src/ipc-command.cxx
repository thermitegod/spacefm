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

#include <array>
#include <vector>

#include <optional>

#include <malloc.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <nlohmann/json.hpp>

#include "ptk/ptk-file-browser.hxx"
#include "types.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"

#include "settings/app.hxx"

#include "settings.hxx"
#include "terminal-handlers.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-volume.hxx"

#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-task-view.hxx"

#include "main-window.hxx"

static const std::string
unescape(const std::string_view t)
{
    std::string unescaped = t.data();
    unescaped = ztd::replace(unescaped, "\\\n", "\\n");
    unescaped = ztd::replace(unescaped, "\\\t", "\\t");
    unescaped = ztd::replace(unescaped, "\\\r", "\\r");
    unescaped = ztd::replace(unescaped, "\\\"", "\"");

    return unescaped;
}

static bool
delayed_show_menu(GtkWidget* menu)
{
    MainWindow* main_window = main_window_get_last_active();
    if (main_window)
    {
        gtk_window_present(GTK_WINDOW(main_window));
    }
    gtk_widget_show_all(GTK_WIDGET(menu));
    gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    g_signal_connect(G_OBJECT(menu), "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    return false;
}

// These are also the sockets return code
#define SOCKET_SUCCESS 0 // Successful exit status.
#define SOCKET_FAILURE 1 // Failing exit status.
#define SOCKET_INVALID 2 // Invalid request exit status.

const std::tuple<char, std::string>
run_ipc_command(const std::string_view socket_commands_json)
{
    const nlohmann::json json = nlohmann::json::parse(socket_commands_json);

    // socket flags
    panel_t panel = json["panel"];
    tab_t tab = json["tab"];
    std::string window = json["window"];
    // socket commands
    // subproperty and data are only retrived in the properties that need them
    const std::string command = json["command"];
    const std::string property = json["property"];

    // must match file-browser.c
    static constexpr std::array<const std::string_view, 12> column_titles{
        "Name",
        "Size",
        "Size in Bytes",
        "Type",
        "MIME Type",
        "Permissions",
        "Owner",
        "Group",
        "Date Accessed",
        "Date Created",
        "Date Metadata Changed",
        "Date Modified",
    };

    // window
    MainWindow* main_window = nullptr;
    if (window.empty())
    {
        main_window = main_window_get_last_active();
        if (main_window == nullptr)
        {
            return {SOCKET_INVALID, "invalid window"};
        }
    }
    else
    {
        for (MainWindow* window2 : main_window_get_all())
        {
            const std::string str = std::format("{}", fmt::ptr(window2));
            if (ztd::same(str, window))
            {
                main_window = window2;
                break;
            }
        }
        if (main_window == nullptr)
        {
            return {SOCKET_INVALID, std::format("invalid window {}", window)};
        }
    }

    // panel
    if (panel == INVALID_PANEL)
    {
        panel = main_window->curpanel;
    }
    if (!is_valid_panel(panel))
    {
        return {SOCKET_INVALID, std::format("invalid panel {}", panel)};
    }
    if (!xset_get_b_panel(panel, xset::panel::show) ||
        gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) == -1)
    {
        return {SOCKET_INVALID, std::format("panel {} is not visible", panel)};
    }

    // tab
    if (tab == 0)
    {
        tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) + 1;
    }
    if (tab < 1 || tab > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
    {
        return {SOCKET_INVALID, std::format("invalid tab {}", tab)};
    }
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
        gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[panel - 1]), tab - 1));

    // command

    i8 i = 0; // socket commands index

    if (ztd::same(command, "set"))
    {
        const std::vector<std::string> data = json["data"];

        if (ztd::same(property, "window-size") || ztd::same(property, "window-position"))
        {
            const std::string_view value = data[0];

            i32 height = 0;
            i32 width = 0;

            // size format '620x480'
            if (!ztd::contains(value, "x"))
            {
                return {SOCKET_INVALID, std::format("invalid size format {}", value)};
            }
            const auto size = ztd::split(value, "x");
            width = std::stoi(size[0]);
            height = std::stoi(size[1]);

            if (height < 1 || width < 1)
            {
                return {SOCKET_INVALID, std::format("invalid size {}", value)};
            }
            if (ztd::same(property, "window-size"))
            {
                gtk_window_resize(GTK_WINDOW(main_window), width, height);
            }
            else
            {
                gtk_window_move(GTK_WINDOW(main_window), width, height);
            }
        }
        else if (ztd::same(property, "window-maximized"))
        {
            const std::string subproperty = json["subproperty"];

            if (ztd::same(subproperty, "true"))
            {
                gtk_window_maximize(GTK_WINDOW(main_window));
            }
            else
            {
                gtk_window_unmaximize(GTK_WINDOW(main_window));
            }
        }
        else if (ztd::same(property, "window-fullscreen"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::main_full, ztd::same(subproperty, "true"));
            main_window_fullscreen_activate(main_window);
        }
        else if (ztd::same(property, "window-vslider-top") ||
                 ztd::same(property, "window-vslider-bottom") ||
                 ztd::same(property, "window-hslider") || ztd::same(property, "window-tslider"))
        {
            const std::string_view value = data[0];

            const i32 width = std::stoi(value.data());
            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }

            GtkWidget* widget;
            if (ztd::same(property, "window-vslider-top"))
            {
                widget = main_window->hpane_top;
            }
            else if (ztd::same(property, "window-vslider-bottom"))
            {
                widget = main_window->hpane_bottom;
            }
            else if (ztd::same(property, "window-hslider"))
            {
                widget = main_window->vpane;
            }
            else
            {
                widget = main_window->task_vpane;
            }

            gtk_paned_set_position(GTK_PANED(widget), width);
        }
        else if (ztd::same(property, "focused-panel"))
        {
            const std::string subproperty = json["subproperty"];

            i32 width = 0;

            if (ztd::same(subproperty, "prev"))
            {
                width = panel_control_code_prev;
            }
            else if (ztd::same(subproperty, "next"))
            {
                width = panel_control_code_next;
            }
            else if (ztd::same(subproperty, "hide"))
            {
                width = panel_control_code_hide;
            }
            else if (ztd::same(subproperty, "panel1"))
            {
                width = panel_1;
            }
            else if (ztd::same(subproperty, "panel2"))
            {
                width = panel_2;
            }
            else if (ztd::same(subproperty, "panel3"))
            {
                width = panel_3;
            }
            else if (ztd::same(subproperty, "panel4"))
            {
                width = panel_4;
            }

            if (!is_valid_panel(width) || !is_valid_panel_code(width))
            {
                return {SOCKET_INVALID, "invalid panel number"};
            }
            focus_panel(nullptr, (void*)main_window, width);
        }
        else if (ztd::same(property, "focused-pane"))
        {
            const std::string subproperty = json["subproperty"];

            GtkWidget* widget = nullptr;

            if (ztd::same(subproperty, "filelist"))
            {
                widget = file_browser->folder_view();
            }
            else if (ztd::same(subproperty, "devices"))
            {
                widget = file_browser->side_dev;
            }
            else if (ztd::same(subproperty, "dirtree"))
            {
                widget = file_browser->side_dir;
            }
            else if (ztd::same(subproperty, "pathbar"))
            {
                widget = file_browser->path_bar();
            }

            if (GTK_IS_WIDGET(widget))
            {
                gtk_widget_grab_focus(widget);
            }
        }
        else if (ztd::same(property, "current-tab"))
        {
            const std::string subproperty = json["subproperty"];

            tab_t new_tab = INVALID_TAB;

            if (ztd::same(subproperty, "prev"))
            {
                new_tab = tab_control_code_prev;
            }
            else if (ztd::same(subproperty, "next"))
            {
                new_tab = tab_control_code_next;
            }
            else if (ztd::same(subproperty, "close"))
            {
                new_tab = tab_control_code_close;
            }
            else if (ztd::same(subproperty, "restore"))
            {
                new_tab = tab_control_code_restore;
            }
            else if (ztd::same(subproperty, "tab1"))
            {
                new_tab = tab_1;
            }
            else if (ztd::same(subproperty, "tab2"))
            {
                new_tab = tab_2;
            }
            else if (ztd::same(subproperty, "tab3"))
            {
                new_tab = tab_3;
            }
            else if (ztd::same(subproperty, "tab4"))
            {
                new_tab = tab_4;
            }
            else if (ztd::same(subproperty, "tab5"))
            {
                new_tab = tab_5;
            }
            else if (ztd::same(subproperty, "tab6"))
            {
                new_tab = tab_6;
            }
            else if (ztd::same(subproperty, "tab7"))
            {
                new_tab = tab_7;
            }
            else if (ztd::same(subproperty, "tab8"))
            {
                new_tab = tab_8;
            }
            else if (ztd::same(subproperty, "tab9"))
            {
                new_tab = tab_9;
            }
            else if (ztd::same(subproperty, "tab10"))
            {
                new_tab = tab_10;
            }

            if (!(is_valid_tab(new_tab) || is_valid_tab_code(new_tab)) || new_tab == INVALID_TAB ||
                new_tab > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
            {
                return {SOCKET_INVALID, std::format("invalid tab number: {}", new_tab)};
            }
            file_browser->go_tab(new_tab);
        }
        else if (ztd::same(property, "new-tab"))
        {
            const std::string_view value = data[0];

            if (!std::filesystem::is_directory(value))
            {
                return {SOCKET_FAILURE, std::format("not a directory: '{}'", value)};
            }

            focus_panel(nullptr, (void*)main_window, panel);
            main_window_add_new_tab(main_window, value);
        }
        else if (ztd::same(property, "devices-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_devmon,
                                  main_window->panel_context.at(panel),
                                  ztd::same(subproperty, "true"));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "dirtree-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_dirtree,
                                  main_window->panel_context.at(panel),
                                  ztd::same(subproperty, "true"));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "toolbar-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_toolbox,
                                  main_window->panel_context.at(panel),
                                  ztd::same(subproperty, "true"));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "sidetoolbar-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_sidebar,
                                  main_window->panel_context.at(panel),
                                  ztd::same(subproperty, "true"));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "hidden-files-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel, xset::panel::show_hidden, ztd::same(subproperty, "true"));
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "panel1-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_1, xset::panel::show, ztd::same(subproperty, "true"));
            show_panels_all_windows(nullptr, main_window);
        }
        else if (ztd::same(property, "panel2-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_2, xset::panel::show, ztd::same(subproperty, "true"));
            show_panels_all_windows(nullptr, main_window);
        }
        else if (ztd::same(property, "panel3-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_3, xset::panel::show, ztd::same(subproperty, "true"));
            show_panels_all_windows(nullptr, main_window);
        }
        else if (ztd::same(property, "panel4-visible"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_4, xset::panel::show, ztd::same(subproperty, "true"));
            show_panels_all_windows(nullptr, main_window);
        }
        else if (ztd::same(property, "panel-hslider-top") ||
                 ztd::same(property, "panel-hslider-bottom") ||
                 ztd::same(property, "panel-vslider"))
        {
            const std::string_view value = data[0];

            const i32 width = std::stoi(value.data());

            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }
            GtkWidget* widget;
            if (ztd::same(property, "panel-hslider-top"))
            {
                widget = file_browser->side_vpane_top;
            }
            else if (ztd::same(property, "panel-hslider-bottom"))
            {
                widget = file_browser->side_vpane_bottom;
            }
            else
            {
                widget = file_browser->hpane;
            }
            gtk_paned_set_position(GTK_PANED(widget), width);
            file_browser->slider_release(nullptr);
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(property, "column-width"))
        { // COLUMN WIDTH
            const std::string_view value = data[0];
            const std::string subproperty = json["subproperty"];

            const i32 width = std::stoi(value.data());

            if (width < 1)
            {
                return {SOCKET_INVALID, "invalid column width"};
            }
            if (file_browser->is_view_mode(ptk::file_browser::view_mode::list_view))
            {
                bool found = false;
                GtkTreeViewColumn* col;
                for (const auto [index, column_title] : ztd::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view()),
                                                   static_cast<i32>(index));
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (ztd::same(subproperty, title))
                    {
                        found = true;
                        break;
                    }

                    if (ztd::same(title, column_title) &&
                        (ztd::same(subproperty, "name") || ztd::same(subproperty, "size") ||
                         ztd::same(subproperty, "bytes") || ztd::same(subproperty, "type") ||
                         ztd::same(subproperty, "mime") || ztd::same(subproperty, "permission") ||
                         ztd::same(subproperty, "owner") || ztd::same(subproperty, "group") ||
                         ztd::same(subproperty, "accessed") || ztd::same(subproperty, "created") ||
                         ztd::same(subproperty, "metadata") || ztd::same(subproperty, "modified")))
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    gtk_tree_view_column_set_fixed_width(col, width);
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid column name '{}'", value)};
                }
            }
        }
        else if (ztd::same(property, "sort-by"))
        { // COLUMN
            const std::string subproperty = json["subproperty"];

            auto j = ptk::file_browser::sort_order::name;
            if (ztd::same(subproperty, "name"))
            {
                j = ptk::file_browser::sort_order::name;
            }
            else if (ztd::same(subproperty, "size"))
            {
                j = ptk::file_browser::sort_order::size;
            }
            else if (ztd::same(subproperty, "bytes"))
            {
                j = ptk::file_browser::sort_order::bytes;
            }
            else if (ztd::same(subproperty, "type"))
            {
                j = ptk::file_browser::sort_order::type;
            }
            else if (ztd::same(subproperty, "mime"))
            {
                j = ptk::file_browser::sort_order::mime;
            }
            else if (ztd::same(subproperty, "permission"))
            {
                j = ptk::file_browser::sort_order::perm;
            }
            else if (ztd::same(subproperty, "owner"))
            {
                j = ptk::file_browser::sort_order::owner;
            }
            else if (ztd::same(subproperty, "group"))
            {
                j = ptk::file_browser::sort_order::group;
            }
            else if (ztd::same(subproperty, "accessed"))
            {
                j = ptk::file_browser::sort_order::atime;
            }
            else if (ztd::same(subproperty, "created"))
            {
                j = ptk::file_browser::sort_order::btime;
            }
            else if (ztd::same(subproperty, "metadata"))
            {
                j = ptk::file_browser::sort_order::ctime;
            }
            else if (ztd::same(subproperty, "modified"))
            {
                j = ptk::file_browser::sort_order::mtime;
            }

            else
            {
                return {SOCKET_INVALID, std::format("invalid column name '{}'", subproperty)};
            }
            file_browser->set_sort_order(j);
        }
        else if (ztd::same(property, "sort-ascend"))
        {
            const std::string subproperty = json["subproperty"];

            file_browser->set_sort_type(ztd::same(subproperty, "true")
                                            ? GtkSortType::GTK_SORT_ASCENDING
                                            : GtkSortType::GTK_SORT_DESCENDING);
        }
        else if (ztd::same(property, "sort-alphanum"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::sortx_alphanum, ztd::same(subproperty, "true"));
            file_browser->set_sort_extra(xset::name::sortx_alphanum);
        }
        // else if (ztd::same(property, "sort-natural"))
        // {
        //     const std::string subproperty = json["subproperty"];
        //
        //     xset_set_b(xset::name::sortx_alphanum, ztd::same(subproperty, "true"));
        //     file_browser->set_sort_extra(xset::name::sortx_alphanum);
        // }
        else if (ztd::same(property, "sort-case"))
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::sortx_case, ztd::same(subproperty, "true"));
            file_browser->set_sort_extra(xset::name::sortx_case);
        }
        else if (ztd::same(property, "sort-hidden-first"))
        {
            const std::string subproperty = json["subproperty"];

            const xset::name name = ztd::same(subproperty, "true") ? xset::name::sortx_hidfirst
                                                                   : xset::name::sortx_hidlast;
            xset_set_b(name, true);
            file_browser->set_sort_extra(name);
        }
        else if (ztd::same(property, "sort-first"))
        {
            const std::string subproperty = json["subproperty"];

            xset::name name;
            if (ztd::same(subproperty, "files"))
            {
                name = xset::name::sortx_files;
            }
            else if (ztd::same(subproperty, "directories"))
            {
                name = xset::name::sortx_directories;
            }
            else if (ztd::same(subproperty, "mixed"))
            {
                name = xset::name::sortx_mix;
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid {} value", subproperty)};
            }
            file_browser->set_sort_extra(name);
        }
        else if (ztd::same(property, "show-thumbnails"))
        {
            const std::string subproperty = json["subproperty"];

            if (app_settings.show_thumbnail() != ztd::same(subproperty, "true"))
            {
                main_window_toggle_thumbnails_all_windows();
            }
        }
        else if (ztd::same(property, "max-thumbnail-size"))
        {
            const std::string_view value = data[0];

            app_settings.max_thumb_size(std::stoi(value.data()));
        }
        else if (ztd::same(property, "large-icons"))
        {
            const std::string subproperty = json["subproperty"];

            if (!file_browser->is_view_mode(ptk::file_browser::view_mode::icon_view))
            {
                xset_set_b_panel_mode(panel,
                                      xset::panel::list_large,
                                      main_window->panel_context.at(panel),
                                      ztd::same(subproperty, "true"));
                update_views_all_windows(nullptr, file_browser);
            }
        }
        else if (ztd::same(property, "pathbar-text"))
        { // TEXT [[SELSTART] SELEND]
            const std::string_view value = data[0];

            GtkWidget* path_bar = file_browser->path_bar();
            if (!GTK_IS_WIDGET(path_bar))
            {
                return {SOCKET_SUCCESS, ""};
            }

            gtk_entry_set_text(GTK_ENTRY(path_bar), value.data());

            gtk_editable_set_position(GTK_EDITABLE(path_bar), -1);
            // gtk_editable_select_region(GTK_EDITABLE(path_bar), width, height);
            gtk_widget_grab_focus(path_bar);
        }
        else if (ztd::same(property, "clipboard-text") ||
                 ztd::same(property, "clipboard-primary-text"))
        {
            const std::string_view value = data[0];

            if (!g_utf8_validate(value.data(), -1, nullptr))
            {
                return {SOCKET_INVALID, "text is not valid UTF-8"};
            }
            GtkClipboard* clip =
                gtk_clipboard_get(ztd::same(property, "clipboard-text") ? GDK_SELECTION_CLIPBOARD
                                                                        : GDK_SELECTION_PRIMARY);
            const std::string str = unescape(value);
            gtk_clipboard_set_text(clip, str.data(), -1);
        }
        else if (ztd::same(property, "clipboard-from-file") ||
                 ztd::same(property, "clipboard-primary-from-file"))
        {
            const std::string_view value = data[0];

            std::string contents;
            try
            {
                contents = Glib::file_get_contents(value.data());
            }
            catch (const Glib::FileError& e)
            {
                return {SOCKET_INVALID, std::format("error reading file '{}'", value)};
            }
            if (!g_utf8_validate(contents.data(), -1, nullptr))
            {
                return {SOCKET_INVALID,
                        std::format("file '{}' does not contain valid UTF-8 text", value)};
            }
            GtkClipboard* clip = gtk_clipboard_get(ztd::same(property, "clipboard-from-file")
                                                       ? GDK_SELECTION_CLIPBOARD
                                                       : GDK_SELECTION_PRIMARY);
            gtk_clipboard_set_text(clip, contents.data(), -1);
        }
        else if (ztd::same(property, "clipboard-cut-files") ||
                 ztd::same(property, "clipboard-copy-files"))
        {
            ptk_clipboard_cut_or_copy_file_list(data, ztd::same(property, "clipboard_copy_files"));
        }
        else if (ztd::same(property, "selected-filenames") || ztd::same(property, "selected-files"))
        {
            const auto& select_filenames = data;

            if (select_filenames.empty())
            {
                // unselect all
                file_browser->unselect_all();
            }
            else
            {
                for (const std::filesystem::path select_filename : select_filenames)
                {
                    file_browser->select_file(select_filename.filename(), false);
                }
            }
        }
        else if (ztd::same(property, "unselected-filenames") ||
                 ztd::same(property, "unselected-files"))
        {
            const auto& select_filenames = data;

            if (select_filenames.empty())
            {
                // unselect all
                file_browser->unselect_all();
            }
            else
            {
                for (const std::filesystem::path select_filename : select_filenames)
                {
                    file_browser->unselect_file(select_filename.filename(), false);
                }
            }
        }
        else if (ztd::same(property, "selected-pattern"))
        {
            const std::string_view value = data[0];

            if (value.empty())
            {
                // unselect all
                file_browser->unselect_all();
            }
            else
            {
                file_browser->select_pattern(value);
            }
        }
        else if (ztd::same(property, "current-dir"))
        {
            const std::string_view value = data[0];

            if (value.empty())
            {
                return {SOCKET_FAILURE, std::format("{} requires a directory path", property)};
            }
            if (!std::filesystem::is_directory(value))
            {
                return {SOCKET_FAILURE, std::format("directory '{}' does not exist", value)};
            }
            file_browser->chdir(value, ptk::file_browser::chdir_mode::add_history);
        }
        else if (ztd::same(property, "thumbnailer"))
        {
            const std::string subproperty = json["subproperty"];
            if (ztd::same(subproperty, "api"))
            {
                app_settings.thumbnailer_use_api(true);
            }
            else // if (ztd::same(subproperty, "cli"))
            {
                app_settings.thumbnailer_use_api(false);
            }
        }
        else if (ztd::same(property, "editor"))
        {
            const std::string_view value = data[0];

            if (!ztd::endswith(value, ".desktop"))
            {
                return {SOCKET_FAILURE, std::format("Must be a .desktop file '{}'", value)};
            }

            const std::filesystem::path editor = value;
            if (editor.is_absolute())
            {
                xset_set(xset::name::editor, xset::var::s, editor.filename().string());
            }
            else
            {
                xset_set(xset::name::editor, xset::var::s, editor.string());
            }
        }
        else if (ztd::same(property, "terminal"))
        {
            const std::string_view value = data[0];

            std::filesystem::path terminal = value;
            if (terminal.is_absolute())
            {
                terminal = terminal.filename();
            }

            const auto supported_terminals = terminal_handlers->get_supported_terminal_names();
            for (const std::string_view supported_terminal : supported_terminals)
            {
                if (ztd::same(terminal.string(), supported_terminal))
                {
                    xset_set(xset::name::main_terminal, xset::var::s, terminal.string());
                    return {SOCKET_SUCCESS, ""};
                }
            }

            return {SOCKET_FAILURE,
                    std::format("Terminal is not supported '{}'\nSupported List:\n{}",
                                value,
                                ztd::join(supported_terminals, "\n"))};
        }
        else
        {
            return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
        }
    }
    else if (ztd::same(command, "get"))
    {
        // get
        if (ztd::same(property, "window-size"))
        {
            i32 width;
            i32 height;
            gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
        }
        else if (ztd::same(property, "window-position"))
        {
            i32 width;
            i32 height;
            gtk_window_get_position(GTK_WINDOW(main_window), &width, &height);
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
        }
        else if (ztd::same(property, "window-maximized"))
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->maximized)};
        }
        else if (ztd::same(property, "window-fullscreen"))
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->fullscreen)};
        }
        else if (ztd::same(property, "screen-size"))
        {
            GdkRectangle workarea = GdkRectangle();
            gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
                                     &workarea);
            return {SOCKET_SUCCESS, std::format("{}x{}", workarea.width, workarea.height)};
        }
        else if (ztd::same(property, "window-vslider-top") ||
                 ztd::same(property, "window-vslider-bottom") ||
                 ztd::same(property, "window-hslider") || ztd::same(property, "window-tslider"))
        {
            GtkWidget* widget = nullptr;

            if (ztd::same(property, "window-vslider-top"))
            {
                widget = main_window->hpane_top;
            }
            else if (ztd::same(property, "window-vslider-bottom"))
            {
                widget = main_window->hpane_bottom;
            }
            else if (ztd::same(property, "window-hslider"))
            {
                widget = main_window->vpane;
            }
            else if (ztd::same(property, "window-tslider"))
            {
                widget = main_window->task_vpane;
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(GTK_PANED(widget)))};
        }
        else if (ztd::same(property, "focused-panel"))
        {
            return {SOCKET_SUCCESS, std::format("{}", main_window->curpanel)};
        }
        else if (ztd::same(property, "focused-pane"))
        {
            if (file_browser->folder_view() && gtk_widget_is_focus(file_browser->folder_view()))
            {
                return {SOCKET_SUCCESS, "filelist"};
            }
            else if (file_browser->side_dev && gtk_widget_is_focus(file_browser->side_dev))
            {
                return {SOCKET_SUCCESS, "devices"};
            }
            else if (file_browser->side_dir && gtk_widget_is_focus(file_browser->side_dir))
            {
                return {SOCKET_SUCCESS, "dirtree"};
            }
            else if (file_browser->path_bar() && gtk_widget_is_focus(file_browser->path_bar()))
            {
                return {SOCKET_SUCCESS, "pathbar"};
            }
        }
        else if (ztd::same(property, "current-tab"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}",
                                gtk_notebook_page_num(GTK_NOTEBOOK(main_window->panel[panel - 1]),
                                                      GTK_WIDGET(file_browser)) +
                                    1)};
        }
        else if (ztd::same(property, "panel-count"))
        {
            const auto counts = main_window_get_counts(file_browser);
            const panel_t panel_count = counts.panel_count;
            // const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", panel_count)};
        }
        else if (ztd::same(property, "tab-count"))
        {
            const auto counts = main_window_get_counts(file_browser);
            // const panel_t panel_count = counts.panel_count;
            const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", tab_count)};
        }
        else if (ztd::same(property, "devices-visible") || ztd::same(property, "dirtree-visible") ||
                 ztd::same(property, "toolbar-visible") ||
                 ztd::same(property, "sidetoolbar-visible") ||
                 ztd::same(property, "hidden-files-visible") ||
                 ztd::same(property, "panel1-visible") || ztd::same(property, "panel2-visible") ||
                 ztd::same(property, "panel3-visible") || ztd::same(property, "panel4-visible"))
        {
            bool valid = false;
            bool use_mode = false;
            xset::panel xset_panel_var;
            if (ztd::same(property, "devices-visible"))
            {
                xset_panel_var = xset::panel::show_devmon;
                use_mode = true;
                valid = true;
            }
            else if (ztd::same(property, "dirtree-visible"))
            {
                xset_panel_var = xset::panel::show_dirtree;
                use_mode = true;
                valid = true;
            }
            else if (ztd::same(property, "toolbar-visible"))
            {
                xset_panel_var = xset::panel::show_toolbox;
                use_mode = true;
                valid = true;
            }
            else if (ztd::same(property, "sidetoolbar-visible"))
            {
                xset_panel_var = xset::panel::show_sidebar;
                use_mode = true;
                valid = true;
            }
            else if (ztd::same(property, "hidden-files-visible"))
            {
                xset_panel_var = xset::panel::show_hidden;
                valid = true;
            }
            else if (ztd::startswith(property, "panel"))
            {
                const panel_t j = std::stoi(property.substr(5, 1));
                return {SOCKET_SUCCESS, std::format("{}", xset_get_b_panel(j, xset::panel::show))};
            }
            if (!valid)
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            if (use_mode)
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel_mode(panel,
                                                          xset_panel_var,
                                                          main_window->panel_context.at(panel)))};
            }
            else
            {
                return {SOCKET_SUCCESS, std::format("{}", xset_get_b_panel(panel, xset_panel_var))};
            }
        }
        else if (ztd::same(property, "panel-hslider-top") ||
                 ztd::same(property, "panel-hslider-bottom") ||
                 ztd::same(property, "panel-vslider"))
        {
            GtkWidget* widget;
            if (ztd::same(property, "panel-hslider-top"))
            {
                widget = file_browser->side_vpane_top;
            }
            else if (ztd::same(property, "panel-hslider-bottom"))
            {
                widget = file_browser->side_vpane_bottom;
            }
            else if (ztd::same(property, "panel-vslider"))
            {
                widget = file_browser->hpane;
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(GTK_PANED(widget)))};
        }
        else if (ztd::same(property, "column-width"))
        { // COLUMN
            const std::string subproperty = json["subproperty"];

            if (file_browser->is_view_mode(ptk::file_browser::view_mode::list_view))
            {
                bool found = false;
                GtkTreeViewColumn* col = nullptr;
                for (const auto [index, column_title] : ztd::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view()),
                                                   static_cast<i32>(index));
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (ztd::same(subproperty, title))
                    {
                        found = true;
                        break;
                    }
                    if (ztd::same(title, column_title) &&
                        (ztd::same(subproperty, "name") || ztd::same(subproperty, "size") ||
                         ztd::same(subproperty, "bytes") || ztd::same(subproperty, "type") ||
                         ztd::same(subproperty, "mime") || ztd::same(subproperty, "permission") ||
                         ztd::same(subproperty, "owner") || ztd::same(subproperty, "group") ||
                         ztd::same(subproperty, "accessed") || ztd::same(subproperty, "created") ||
                         ztd::same(subproperty, "metadata") || ztd::same(subproperty, "modified")))
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    return {SOCKET_SUCCESS, std::format("{}", gtk_tree_view_column_get_width(col))};
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid column name '{}'", subproperty)};
                }
            }
        }
        else if (ztd::same(property, "sort-by"))
        { // COLUMN
            switch (file_browser->sort_order())
            {
                case ptk::file_browser::sort_order::name:
                    return {SOCKET_SUCCESS, "name"};
                case ptk::file_browser::sort_order::size:
                    return {SOCKET_SUCCESS, "size"};
                case ptk::file_browser::sort_order::bytes:
                    return {SOCKET_SUCCESS, "bytes"};
                case ptk::file_browser::sort_order::type:
                    return {SOCKET_SUCCESS, "type"};
                case ptk::file_browser::sort_order::mime:
                    return {SOCKET_SUCCESS, "mime"};
                case ptk::file_browser::sort_order::perm:
                    return {SOCKET_SUCCESS, "permission"};
                case ptk::file_browser::sort_order::owner:
                    return {SOCKET_SUCCESS, "owner"};
                case ptk::file_browser::sort_order::group:
                    return {SOCKET_SUCCESS, "group"};
                case ptk::file_browser::sort_order::atime:
                    return {SOCKET_SUCCESS, "accessed"};
                case ptk::file_browser::sort_order::btime:
                    return {SOCKET_SUCCESS, "created"};
                case ptk::file_browser::sort_order::ctime:
                    return {SOCKET_SUCCESS, "metadata"};
                case ptk::file_browser::sort_order::mtime:
                    return {SOCKET_SUCCESS, "modified"};
            }
        }
        else if (ztd::same(property, "sort-ascend") || ztd::same(property, "sort-natural") ||
                 ztd::same(property, "sort-alphanum") || ztd::same(property, "sort-case") ||
                 ztd::same(property, "sort-hidden-first") || ztd::same(property, "sort-first") ||
                 ztd::same(property, "panel-hslider-top"))
        {
            if (ztd::same(property, "sort-ascend"))
            {
                return {SOCKET_SUCCESS,
                        std::format(
                            "{}",
                            file_browser->is_sort_type(GtkSortType::GTK_SORT_ASCENDING) ? 1 : 0)};
            }
#if 0
            else if (ztd::same(property, "sort-natural"))
            {

            }
#endif
            else if (ztd::same(property, "sort-alphanum"))
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel(file_browser->panel(), xset::panel::sort_extra)
                                        ? 1
                                        : 0)};
            }
            else if (ztd::same(property, "sort-case"))
            {
                return {
                    SOCKET_SUCCESS,
                    std::format("{}",
                                xset_get_b_panel(file_browser->panel(), xset::panel::sort_extra) &&
                                        xset_get_int_panel(file_browser->panel(),
                                                           xset::panel::sort_extra,
                                                           xset::var::x) == xset::b::xtrue
                                    ? 1
                                    : 0)};
            }
            else if (ztd::same(property, "sort-hidden-first"))
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_int_panel(file_browser->panel(),
                                                       xset::panel::sort_extra,
                                                       xset::var::z) == xset::b::xtrue
                                        ? 1
                                        : 0)};
            }
            else if (ztd::same(property, "sort-first"))
            {
                const i32 result = xset_get_int_panel(file_browser->panel(),
                                                      xset::panel::sort_extra,
                                                      xset::var::y);
                if (result == 0)
                {
                    return {SOCKET_SUCCESS, "mixed"};
                }
                else if (result == 1)
                {
                    return {SOCKET_SUCCESS, "directories"};
                }
                else if (result == 2)
                {
                    return {SOCKET_SUCCESS, "files"};
                }
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
        }
        else if (ztd::same(property, "show-thumbnails"))
        {
            return {SOCKET_SUCCESS, std::format("{}", app_settings.show_thumbnail() ? 1 : 0)};
        }
        else if (ztd::same(property, "max-thumbnail-size"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", vfs_file_size_format(app_settings.max_thumb_size()))};
        }
        else if (ztd::same(property, "large-icons"))
        {
            return {SOCKET_SUCCESS, std::format("{}", file_browser->using_large_icons() ? 1 : 0)};
        }
        else if (ztd::same(property, "statusbar-text"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", gtk_label_get_text(GTK_LABEL(file_browser->status_label)))};
        }
        else if (ztd::same(property, "pathbar-text"))
        {
            const GtkWidget* path_bar = file_browser->path_bar();
            if (GTK_IS_WIDGET(path_bar))
            {
                return {SOCKET_SUCCESS, std::format("{}", gtk_entry_get_text(GTK_ENTRY(path_bar)))};
            }
        }
        else if (ztd::same(property, "clipboard-text") ||
                 ztd::same(property, "clipboard-primary-text"))
        {
            GtkClipboard* clip =
                gtk_clipboard_get(ztd::same(property, "clipboard-text") ? GDK_SELECTION_CLIPBOARD
                                                                        : GDK_SELECTION_PRIMARY);
            return {SOCKET_SUCCESS, gtk_clipboard_wait_for_text(clip)};
        }
        else if (ztd::same(property, "clipboard-cut-files") ||
                 ztd::same(property, "clipboard-copy-files"))
        {
            GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            GdkAtom gnome_target;
            GdkAtom uri_list_target;
            GtkSelectionData* sel_data;

            gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
            sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
            if (!sel_data)
            {
                uri_list_target = gdk_atom_intern("text/uri-list", false);
                sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
                if (!sel_data)
                {
                    return {SOCKET_SUCCESS, ""};
                }
            }
            if (gtk_selection_data_get_length(sel_data) <= 0 ||
                gtk_selection_data_get_format(sel_data) != 8)
            {
                gtk_selection_data_free(sel_data);
                return {SOCKET_SUCCESS, ""};
            }
            if (ztd::startswith((const char*)gtk_selection_data_get_data(sel_data), "cut"))
            {
                if (ztd::same(property, "clipboard-copy-files"))
                {
                    gtk_selection_data_free(sel_data);
                    return {SOCKET_SUCCESS, ""};
                }
            }
            else if (ztd::same(property, "clipboard-cut-files"))
            {
                gtk_selection_data_free(sel_data);
                return {SOCKET_SUCCESS, ""};
            }
            const char* clip_txt = gtk_clipboard_wait_for_text(clip);
            gtk_selection_data_free(sel_data);
            if (!clip_txt)
            {
                return {SOCKET_SUCCESS, ""};
            }
            // build fish array
            const std::vector<std::string> pathv = ztd::split(clip_txt, "");
            std::string str;
            for (const std::string_view path : pathv)
            {
                str.append(std::format("{} ", ztd::shell::quote(path)));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (ztd::same(property, "selected-filenames") || ztd::same(property, "selected-files"))
        {
            const auto selected_files = file_browser->selected_files();
            if (selected_files.empty())
            {
                return {SOCKET_SUCCESS, ""};
            }

            // build fish array
            std::string str;
            for (const vfs::file_info file : selected_files)
            {
                if (!file)
                {
                    continue;
                }
                str.append(std::format("{} ", ztd::shell::quote(file->name())));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (ztd::same(property, "selected-pattern"))
        {
        }
        else if (ztd::same(property, "current-dir"))
        {
            return {SOCKET_SUCCESS, std::format("{}", file_browser->cwd().string())};
        }
        else if (ztd::same(property, "thumbnailer"))
        {
            return {SOCKET_SUCCESS, app_settings.thumbnailer_use_api() ? "api" : "cli"};
        }
        else if (ztd::same(property, "editor"))
        {
            const auto editor = xset_get_s(xset::name::editor);
            if (editor)
            {
                return {SOCKET_SUCCESS, editor.value()};
            }
            else
            {
                return {SOCKET_SUCCESS, "No editor has been set"};
            }
        }
        else if (ztd::same(property, "terminal"))
        {
            const auto terminal = xset_get_s(xset::name::main_terminal);
            if (terminal)
            {
                return {SOCKET_SUCCESS, terminal.value()};
            }
            else
            {
                return {SOCKET_SUCCESS, "No terminal has been set"};
            }
        }
        else
        {
            return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
        }
    }
    else if (ztd::same(command, "set-task"))
    { // TASKNUM PROPERTY [VALUE]
        const std::string subproperty = json["subproperty"];
        const std::vector<std::string> data = json["data"];
        const std::string_view value = data[0];

        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
                const std::string str = std::format("{}", fmt::ptr(ptask));
                if (ztd::same(str, data[i]))
                {
                    break;
                }
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            return {SOCKET_INVALID, std::format("invalid task '{}'", data[i])};
        }
        if (ptask->task->type != vfs::file_task_type::exec)
        {
            return {SOCKET_INVALID, std::format("internal task {} is read-only", data[i])};
        }

        // set model value
        i32 j;
        if (ztd::same(property, "icon"))
        {
            ptk_file_task_lock(ptask);
            ptask->task->exec_icon = value;
            ptask->pause_change_view = ptask->pause_change = true;
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(property, "count"))
        {
            j = task_view_column::count;
        }
        else if (ztd::same(property, "directory") || ztd::same(subproperty, "from"))
        {
            j = task_view_column::path;
        }
        else if (ztd::same(property, "item"))
        {
            j = task_view_column::file;
        }
        else if (ztd::same(property, "to"))
        {
            j = task_view_column::to;
        }
        else if (ztd::same(property, "progress"))
        {
            if (value.empty())
            {
                ptask->task->percent = 50;
            }
            else
            {
                j = std::stoi(value.data());
                if (j < 0)
                {
                    j = 0;
                }
                if (j > 100)
                {
                    j = 100;
                }
                ptask->task->percent = j;
            }
            ptask->task->custom_percent = !ztd::same(value, "0");
            ptask->pause_change_view = ptask->pause_change = true;
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(property, "total"))
        {
            j = task_view_column::total;
        }
        else if (ztd::same(property, "curspeed"))
        {
            j = task_view_column::curspeed;
        }
        else if (ztd::same(property, "curremain"))
        {
            j = task_view_column::curest;
        }
        else if (ztd::same(property, "avgspeed"))
        {
            j = task_view_column::avgspeed;
        }
        else if (ztd::same(property, "avgremain"))
        {
            j = task_view_column::avgest;
        }
        else if (ztd::same(property, "queue_state"))
        {
            if (ztd::same(subproperty, "run"))
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::running);
            }
            else if (ztd::same(subproperty, "pause"))
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::pause);
            }
            else if (ztd::same(subproperty, "queue") || ztd::same(subproperty, "queued"))
            {
                ptk_file_task_pause(ptask, vfs::file_task_state::queue);
            }
            else if (ztd::same(subproperty, "stop"))
            {
                ptk_task_view_task_stop(main_window->task_view,
                                        xset_get(xset::name::task_stop_all),
                                        nullptr);
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid queue_state '{}'", subproperty)};
            }
            main_task_start_queued(main_window->task_view, nullptr);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(property, "popup-handler"))
        {
            std::free(ptask->pop_handler);
            if (value.empty())
            {
                ptask->pop_handler = nullptr;
            }
            else
            {
                ptask->pop_handler = ztd::strdup(value.data());
            }
            return {SOCKET_SUCCESS, ""};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task property '{}'", subproperty)};
        }
        gtk_list_store_set(GTK_LIST_STORE(model), &it, j, value.data(), -1);
    }
    else if (ztd::same(command, "get-task"))
    { // TASKNUM PROPERTY
        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, task_view_column::data, &ptask, -1);
                const std::string str = std::format("{}", fmt::ptr(ptask));
                if (ztd::same(str, property))
                {
                    break;
                }
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            return {SOCKET_INVALID, std::format("invalid task '{}'", property)};
        }

        // get model value
        i32 j;
        if (ztd::same(property, "icon"))
        {
            ptk_file_task_lock(ptask);
            if (!ptask->task->exec_icon.empty())
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->task->exec_icon)};
            }
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(property, "count"))
        {
            j = task_view_column::count;
        }
        else if (ztd::same(property, "directory") || ztd::same(property, "from"))
        {
            j = task_view_column::path;
        }
        else if (ztd::same(property, "item"))
        {
            j = task_view_column::file;
        }
        else if (ztd::same(property, "to"))
        {
            j = task_view_column::to;
        }
        else if (ztd::same(property, "progress"))
        {
            return {SOCKET_SUCCESS, std::format("{}", ptask->task->percent)};
        }
        else if (ztd::same(property, "total"))
        {
            j = task_view_column::total;
        }
        else if (ztd::same(property, "curspeed"))
        {
            j = task_view_column::curspeed;
        }
        else if (ztd::same(property, "curremain"))
        {
            j = task_view_column::curest;
        }
        else if (ztd::same(property, "avgspeed"))
        {
            j = task_view_column::avgspeed;
        }
        else if (ztd::same(property, "avgremain"))
        {
            j = task_view_column::avgest;
        }
        else if (ztd::same(property, "elapsed"))
        {
            j = task_view_column::elapsed;
        }
        else if (ztd::same(property, "started"))
        {
            j = task_view_column::started;
        }
        else if (ztd::same(property, "status"))
        {
            j = task_view_column::status;
        }
        else if (ztd::same(property, "queue_state"))
        {
            if (ptask->task->state_pause == vfs::file_task_state::running)
            {
                return {SOCKET_SUCCESS, "run"};
            }
            else if (ptask->task->state_pause == vfs::file_task_state::pause)
            {
                return {SOCKET_SUCCESS, "pause"};
            }
            else if (ptask->task->state_pause == vfs::file_task_state::queue)
            {
                return {SOCKET_SUCCESS, "queue"};
            }
            else
            { // failsafe
                return {SOCKET_SUCCESS, "stop"};
            }
        }
        else if (ztd::same(property, "popup-handler"))
        {
            if (ptask->pop_handler)
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->pop_handler)};
            }
            return {SOCKET_SUCCESS, ""};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task property '{}'", property)};
        }
        char* str2;
        gtk_tree_model_get(model, &it, j, &str2, -1);
        if (str2)
        {
            return {SOCKET_SUCCESS, std::format("{}", str2)};
        }
        std::free(str2);
    }
    else if (ztd::same(command, "run-task"))
    { // TYPE [OPTIONS] ...
        if (ztd::same(property, "cmd") || ztd::same(property, "command"))
        {
            // custom command task
            // cmd [--task [--popup] [--scroll]] [--terminal]
            //                     [--user USER] [--title TITLE]
            //                     [--icon ICON] [--dir DIR] COMMAND

            const std::vector<std::string> data = json["data"];

            const nlohmann::json cmd_json = nlohmann::json::parse(data[0]);

            // flags
            bool opt_task = json["task"];
            bool opt_popup = json["popup"];
            bool opt_scroll = json["scroll"];
            bool opt_terminal = json["terminal"];
            const std::string opt_user = json["user"];
            const std::string opt_title = json["title"];
            const std::string opt_icon = json["icon"];
            const std::string opt_cwd = json["cwd"];
            // actual command to be run
            const std::vector<std::string> opt_cmd = json["cmd"];

            if (opt_cmd.empty())
            {
                return {SOCKET_FAILURE, std::format("{} requires a command", command)};
            }
            std::string cmd;
            for (const std::string_view c : opt_cmd)
            {
                cmd.append(std::format(" {}", c));
            }

            PtkFileTask* ptask =
                ptk_file_exec_new(!opt_title.empty() ? opt_title : cmd,
                                  !opt_cwd.empty() ? opt_cwd.data() : file_browser->cwd(),
                                  GTK_WIDGET(file_browser),
                                  file_browser->task_view());
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_icon = opt_icon;
            ptask->task->exec_terminal = opt_terminal;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = opt_task;
            ptask->task->exec_popup = opt_popup;
            ptask->task->exec_show_output = opt_popup;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = !opt_scroll;
            ptask->task->exec_export = true;
            if (opt_popup)
            {
                gtk_window_present(GTK_WINDOW(main_window));
            }
            ptk_file_task_run(ptask);
            if (opt_task)
            {
                return {SOCKET_SUCCESS,
                        std::format("Note: $new_task_id not valid until approx one "
                                    "half second after task start\nnew_task_window={}\n"
                                    "new_task_id={}",
                                    fmt::ptr(main_window),
                                    fmt::ptr(ptask))};
            }
        }
        else if (ztd::same(property, "edit"))
        { // edit FILE
            const std::vector<std::string> data = json["data"];
            const std::string_view value = data[0];

            if (!std::filesystem::is_regular_file(value))
            {
                return {SOCKET_INVALID, std::format("no such file '{}'", value)};
            }
            xset_edit(GTK_WIDGET(file_browser), value);
        }
        else if (ztd::same(property, "mount") || ztd::same(property, "umount"))
        { // mount or unmount TARGET
            const std::vector<std::string> data = json["data"];
            const std::string_view value = data[0];

            // Resolve TARGET
            if (!std::filesystem::exists(value))
            {
                return {SOCKET_INVALID, std::format("path does not exist '{}'", value)};
            }

            const auto real_path_stat = ztd::statx(value);
            vfs::volume vol = nullptr;
            if (ztd::same(property, "umount") && std::filesystem::is_directory(value))
            {
                // umount DIR
                if (is_path_mountpoint(value))
                {
                    if (!real_path_stat || !real_path_stat.is_block_file())
                    {
                        // NON-block device - try to find vol by mount point
                        vol = vfs_volume_get_by_device(value);
                        if (!vol)
                        {
                            return {SOCKET_INVALID, std::format("invalid TARGET '{}'", value)};
                        }
                    }
                }
            }
            else if (real_path_stat && real_path_stat.is_block_file())
            {
                // block device eg /dev/sda1
                vol = vfs_volume_get_by_device(value);
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid TARGET '{}'", value)};
            }

            // Create command
            std::string cmd;
            if (vol)
            {
                // mount/unmount vol
                if (ztd::same(property, "mount"))
                {
                    const auto check_mount_command = vol->device_mount_cmd();
                    if (check_mount_command)
                    {
                        cmd = check_mount_command.value();
                    }
                }
                else
                {
                    const auto check_unmount_command = vol->device_unmount_cmd();
                    if (check_unmount_command)
                    {
                        cmd = check_unmount_command.value();
                    }
                }
            }

            if (cmd.empty())
            {
                return {SOCKET_INVALID, std::format("invalid mount TARGET '{}'", value)};
            }
            // Task
            PtkFileTask* ptask = ptk_file_exec_new(property,
                                                   file_browser->cwd(),
                                                   GTK_WIDGET(file_browser),
                                                   file_browser->task_view());
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_terminal = false;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = true;
            ptask->task->exec_export = false;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = false;
            ptk_file_task_run(ptask);
        }
        else if (ztd::same(property, "copy") || ztd::same(property, "move") ||
                 ztd::same(property, "link") || ztd::same(property, "delete") ||
                 ztd::same(property, "trash"))
        {
            // built-in task
            // copy SOURCE FILENAME [...] TARGET
            // move SOURCE FILENAME [...] TARGET
            // link SOURCE FILENAME [...] TARGET
            // delete SOURCE FILENAME [...]
            // get opts

            const std::vector<std::string> data = json["data"];

            const nlohmann::json cmd_json = nlohmann::json::parse(data[0]);

            // flags
            const std::filesystem::path opt_cwd = json["dir"];
            // file list
            const std::vector<std::string> opt_file_list = json["files"];

            if (opt_file_list.empty())
            {
                return {SOCKET_INVALID, std::format("{} failed, missing file list", property)};
            }

            if (!opt_cwd.empty() && !std::filesystem::is_directory(opt_cwd))
            {
                return {SOCKET_INVALID, std::format("no such directory '{}'", opt_cwd.string())};
            }

            // last argument is the TARGET
            const std::filesystem::path& target_dir = opt_file_list.back();
            if (!ztd::same(property, "delete") || !ztd::same(property, "trash"))
            {
                if (!ztd::startswith(target_dir.string(), "/"))
                {
                    return {SOCKET_INVALID,
                            std::format("TARGET must be absolute '{}'", target_dir.string())};
                }
            }

            std::vector<std::filesystem::path> file_list;
            for (const std::string_view file : opt_file_list)
            {
                if (ztd::startswith(file, "/"))
                { // absolute path
                    file_list.emplace_back(file);
                }
                else
                { // relative path
                    if (opt_cwd.empty())
                    {
                        return {SOCKET_INVALID,
                                std::format("relative path '{}' requires option --dir DIR", file)};
                    }
                    file_list.emplace_back(opt_cwd / file);
                }
            }

            if (!ztd::same(property, "delete") || !ztd::same(property, "trash"))
            {
                // remove TARGET from file list
                file_list.pop_back();
            }

            if (file_list.empty() ||
                (!ztd::same(property, "delete") && !ztd::same(property, "trash")))
            {
                return {SOCKET_INVALID,
                        std::format("task type {} requires FILE argument(s)", data[i])};
            }
            vfs::file_task_type task_type;
            if (ztd::same(property, "copy"))
            {
                task_type = vfs::file_task_type::copy;
            }
            else if (ztd::same(property, "move"))
            {
                task_type = vfs::file_task_type::move;
            }
            else if (ztd::same(property, "link"))
            {
                task_type = vfs::file_task_type::link;
            }
            else if (ztd::same(property, "delete"))
            {
                task_type = vfs::file_task_type::DELETE;
            }
            else if (ztd::same(property, "trash"))
            {
                task_type = vfs::file_task_type::trash;
            }
            else
            { // failsafe
                return {SOCKET_FAILURE, std::format("invalid task type '{}'", property)};
            }
            PtkFileTask* ptask =
                ptk_file_task_new(task_type,
                                  file_list,
                                  target_dir,
                                  GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                  file_browser->task_view());
            ptk_file_task_run(ptask);
            return {SOCKET_SUCCESS,
                    std::format("# Note: $new_task_id not valid until approx one "
                                "half second after task  start\nnew_task_window={}\n"
                                "new_task_id={}",
                                fmt::ptr(main_window),
                                fmt::ptr(ptask))};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task type '{}'", property)};
        }
    }
    else if (ztd::same(command, "emit-key"))
    { // KEYCODE [KEYMOD]
        const std::vector<std::string> data = json["data"];

        // this only handles keys assigned to menu items
        const auto event = (GdkEventKey*)gdk_event_new(GdkEventType::GDK_KEY_PRESS);
        event->keyval = std::stoul(data[i].data(), nullptr, 0);
        event->state = !data[i + 1].empty() ? std::stoul(data[i + 1], nullptr, 0) : 0;
        if (event->keyval)
        {
            gtk_window_present(GTK_WINDOW(main_window));
            main_window_keypress(main_window, event, nullptr);
        }
        else
        {
            gdk_event_free((GdkEvent*)event);
            return {SOCKET_INVALID, std::format("invalid keycode '{}'", data[i])};
        }
        gdk_event_free((GdkEvent*)event);
    }
    else if (ztd::same(command, "activate"))
    {
        const std::vector<std::string> data = json["data"];

        xset_t set = xset_find_custom(data[i]);
        if (!set)
        {
            return {SOCKET_INVALID,
                    std::format("custom command or submenu '{}' not found", data[i])};
        }
        if (set->menu_style == xset::menu::submenu)
        {
            // show submenu as popup menu
            set = xset_get(set->child.value());
            GtkWidget* widget = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();

            xset_add_menuitem(file_browser, GTK_WIDGET(widget), accel_group, set);
            g_idle_add((GSourceFunc)delayed_show_menu, widget);
        }
        else
        {
            // activate item
            main_window_keypress(nullptr, nullptr, set);
        }
    }
    else if (ztd::same(command, "add-event") || ztd::same(command, "replace-event") ||
             ztd::same(command, "remove-event"))
    {
        const std::vector<std::string> data = json["data"];

        xset_t set = xset_is(data[i]);
        if (!set)
        {
            return {SOCKET_INVALID, std::format("invalid event type '{}'", data[i])};
        }
        // build command
        std::string str = (ztd::same(command, "replace-event") ? "*" : "");
        // the first value in data is ignored as it is the xset name
        const std::vector<std::string> event_cmds = {data.cbegin() + 1, data.cend()};
        for (const std::string_view event_cmd : event_cmds)
        {
            str.append(std::format(" {}", event_cmd));
        }
        str = ztd::strip(str); // can not have any extra whitespace
        // modify list
        GList* l = nullptr;
        if (ztd::same(command, "remove-event"))
        {
            l = g_list_find_custom((GList*)set->ob2_data, str.data(), (GCompareFunc)ztd::compare);
            if (!l)
            {
                // remove replace event
                const std::string str2 = std::format("*{}", str);
                l = g_list_find_custom((GList*)set->ob2_data,
                                       str2.data(),
                                       (GCompareFunc)ztd::compare);
            }
            if (!l)
            {
                return {SOCKET_INVALID, "event handler not found"};
            }
            l = g_list_remove((GList*)set->ob2_data, l->data);
        }
        else
        {
            l = g_list_append((GList*)set->ob2_data, ztd::strdup(str));
        }
        set->ob2_data = (void*)l;
    }
    else if (ztd::same(command, "help"))
    {
        return {SOCKET_SUCCESS, "For help run, 'man spacefm-socket'"};
    }
    else if (ztd::same(command, "ping"))
    {
        return {SOCKET_SUCCESS, "pong"};
    }
    else
    {
        return {SOCKET_FAILURE, std::format("invalid socket method '{}'", command)};
    }
    return {SOCKET_SUCCESS, ""};
}
