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

#include <ranges>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <nlohmann/json.hpp>

#include "compat/gtk4-porting.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "types.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-misc.hxx"

#include "settings/app.hxx"

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
    g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
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
            if (str == window)
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
        gtk_notebook_get_current_page(main_window->get_panel_notebook(panel)) == -1)
    {
        return {SOCKET_INVALID, std::format("panel {} is not visible", panel)};
    }

    // tab
    if (tab == 0)
    {
        tab = gtk_notebook_get_current_page(main_window->get_panel_notebook(panel)) + 1;
    }
    if (tab < 1 || tab > gtk_notebook_get_n_pages(main_window->get_panel_notebook(panel)))
    {
        return {SOCKET_INVALID, std::format("invalid tab {}", tab)};
    }
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
        gtk_notebook_get_nth_page(main_window->get_panel_notebook(panel), tab - 1));

    // command

    i8 i = 0; // socket commands index

    if (command == "set")
    {
        const std::vector<std::string> data = json["data"];

        if (property == "window-size" || property == "window-position")
        {
            const std::string_view value = data[0];

            i32 height = 0;
            i32 width = 0;

            // size format '620x480'
            if (!value.contains('x'))
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
            if (property == "window-size")
            {
                gtk_window_set_default_size(GTK_WINDOW(main_window), width, height);
            }
            else
            {
#if (GTK_MAJOR_VERSION == 4)
                return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
                gtk_window_move(GTK_WINDOW(main_window), width, height);
#endif
            }
        }
        else if (property == "window-maximized")
        {
            const std::string subproperty = json["subproperty"];

            if (subproperty == "true")
            {
                gtk_window_maximize(GTK_WINDOW(main_window));
            }
            else
            {
                gtk_window_unmaximize(GTK_WINDOW(main_window));
            }
        }
        else if (property == "window-fullscreen")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::main_full, subproperty == "true");
            main_window_fullscreen_activate(main_window);
        }
        else if (property == "window-vslider-top" || property == "window-vslider-bottom" ||
                 property == "window-hslider" || property == "window-tslider")
        {
            const std::string_view value = data[0];

            const i32 width = std::stoi(value.data());
            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }

            GtkPaned* pane;
            if (property == "window-vslider-top")
            {
                pane = main_window->hpane_top;
            }
            else if (property == "window-vslider-bottom")
            {
                pane = main_window->hpane_bottom;
            }
            else if (property == "window-hslider")
            {
                pane = main_window->vpane;
            }
            else
            {
                pane = main_window->task_vpane;
            }

            gtk_paned_set_position(pane, width);
        }
        else if (property == "focused-panel")
        {
            const std::string subproperty = json["subproperty"];

            i32 width = 0;

            if (subproperty == "prev")
            {
                width = panel_control_code_prev;
            }
            else if (subproperty == "next")
            {
                width = panel_control_code_next;
            }
            else if (subproperty == "hide")
            {
                width = panel_control_code_hide;
            }
            else if (subproperty == "panel1")
            {
                width = panel_1;
            }
            else if (subproperty == "panel2")
            {
                width = panel_2;
            }
            else if (subproperty == "panel3")
            {
                width = panel_3;
            }
            else if (subproperty == "panel4")
            {
                width = panel_4;
            }

            if (!is_valid_panel(width) || !is_valid_panel_code(width))
            {
                return {SOCKET_INVALID, "invalid panel number"};
            }
            main_window->focus_panel(width);
        }
        else if (property == "focused-pane")
        {
            const std::string subproperty = json["subproperty"];

            GtkWidget* widget = nullptr;

            if (subproperty == "filelist")
            {
                widget = file_browser->folder_view();
            }
            else if (subproperty == "devices")
            {
                widget = file_browser->side_dev;
            }
            else if (subproperty == "dirtree")
            {
                widget = file_browser->side_dir;
            }
            else if (subproperty == "pathbar")
            {
                widget = GTK_WIDGET(file_browser->path_bar());
            }

            if (GTK_IS_WIDGET(widget))
            {
                gtk_widget_grab_focus(widget);
            }
        }
        else if (property == "current-tab")
        {
            const std::string subproperty = json["subproperty"];

            tab_t new_tab = INVALID_TAB;

            if (subproperty == "prev")
            {
                new_tab = tab_control_code_prev;
            }
            else if (subproperty == "next")
            {
                new_tab = tab_control_code_next;
            }
            else if (subproperty == "close")
            {
                new_tab = tab_control_code_close;
            }
            else if (subproperty == "restore")
            {
                new_tab = tab_control_code_restore;
            }
            else if (subproperty == "tab1")
            {
                new_tab = tab_1;
            }
            else if (subproperty == "tab2")
            {
                new_tab = tab_2;
            }
            else if (subproperty == "tab3")
            {
                new_tab = tab_3;
            }
            else if (subproperty == "tab4")
            {
                new_tab = tab_4;
            }
            else if (subproperty == "tab5")
            {
                new_tab = tab_5;
            }
            else if (subproperty == "tab6")
            {
                new_tab = tab_6;
            }
            else if (subproperty == "tab7")
            {
                new_tab = tab_7;
            }
            else if (subproperty == "tab8")
            {
                new_tab = tab_8;
            }
            else if (subproperty == "tab9")
            {
                new_tab = tab_9;
            }
            else if (subproperty == "tab10")
            {
                new_tab = tab_10;
            }

            if (!(is_valid_tab(new_tab) || is_valid_tab_code(new_tab)) || new_tab == INVALID_TAB ||
                new_tab > gtk_notebook_get_n_pages(main_window->get_panel_notebook(panel)))
            {
                return {SOCKET_INVALID, std::format("invalid tab number: {}", new_tab)};
            }
            file_browser->go_tab(new_tab);
        }
        else if (property == "new-tab")
        {
            const std::string_view value = data[0];

            if (!std::filesystem::is_directory(value))
            {
                return {SOCKET_FAILURE, std::format("not a directory: '{}'", value)};
            }

            main_window->focus_panel(panel);
            main_window->new_tab(value);
        }
        else if (property == "devices-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_devmon,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "dirtree-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_dirtree,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "toolbar-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_toolbox,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "sidetoolbar-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_sidebar,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "hidden-files-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel, xset::panel::show_hidden, subproperty == "true");
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "panel1-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_1, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel2-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_2, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel3-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_3, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel4-visible")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b_panel(panel_4, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel-hslider-top" || property == "panel-hslider-bottom" ||
                 property == "panel-vslider")
        {
            const std::string_view value = data[0];

            const i32 width = std::stoi(value.data());

            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }
            GtkPaned* pane;
            if (property == "panel-hslider-top")
            {
                pane = file_browser->side_vpane_top;
            }
            else if (property == "panel-hslider-bottom")
            {
                pane = file_browser->side_vpane_bottom;
            }
            else
            {
                pane = file_browser->hpane;
            }
            gtk_paned_set_position(pane, width);
            file_browser->slider_release(nullptr);
            update_views_all_windows(nullptr, file_browser);
        }
        else if (property == "column-width")
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
                for (const auto [index, column_title] : std::views::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view()),
                                                   static_cast<i32>(index));
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (subproperty == title)
                    {
                        found = true;
                        break;
                    }

                    if (title == column_title &&
                        (subproperty == "name" || subproperty == "size" || subproperty == "bytes" ||
                         subproperty == "type" || subproperty == "mime" ||
                         subproperty == "permission" || subproperty == "owner" ||
                         subproperty == "group" || subproperty == "accessed" ||
                         subproperty == "created" || subproperty == "metadata" ||
                         subproperty == "modified"))
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
        else if (property == "sort-by")
        { // COLUMN
            const std::string subproperty = json["subproperty"];

            auto j = ptk::file_browser::sort_order::name;
            if (subproperty == "name")
            {
                j = ptk::file_browser::sort_order::name;
            }
            else if (subproperty == "size")
            {
                j = ptk::file_browser::sort_order::size;
            }
            else if (subproperty == "bytes")
            {
                j = ptk::file_browser::sort_order::bytes;
            }
            else if (subproperty == "type")
            {
                j = ptk::file_browser::sort_order::type;
            }
            else if (subproperty == "mime")
            {
                j = ptk::file_browser::sort_order::mime;
            }
            else if (subproperty == "permission")
            {
                j = ptk::file_browser::sort_order::perm;
            }
            else if (subproperty == "owner")
            {
                j = ptk::file_browser::sort_order::owner;
            }
            else if (subproperty == "group")
            {
                j = ptk::file_browser::sort_order::group;
            }
            else if (subproperty == "accessed")
            {
                j = ptk::file_browser::sort_order::atime;
            }
            else if (subproperty == "created")
            {
                j = ptk::file_browser::sort_order::btime;
            }
            else if (subproperty == "metadata")
            {
                j = ptk::file_browser::sort_order::ctime;
            }
            else if (subproperty == "modified")
            {
                j = ptk::file_browser::sort_order::mtime;
            }

            else
            {
                return {SOCKET_INVALID, std::format("invalid column name '{}'", subproperty)};
            }
            file_browser->set_sort_order(j);
        }
        else if (property == "sort-ascend")
        {
            const std::string subproperty = json["subproperty"];

            file_browser->set_sort_type(subproperty == "true" ? GtkSortType::GTK_SORT_ASCENDING
                                                              : GtkSortType::GTK_SORT_DESCENDING);
        }
        else if (property == "sort-natural")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::sortx_natural, subproperty == "true");
            file_browser->set_sort_extra(xset::name::sortx_natural);
        }
        else if (property == "sort-case")
        {
            const std::string subproperty = json["subproperty"];

            xset_set_b(xset::name::sortx_case, subproperty == "true");
            file_browser->set_sort_extra(xset::name::sortx_case);
        }
        else if (property == "sort-hidden-first")
        {
            const std::string subproperty = json["subproperty"];

            const xset::name name =
                subproperty == "true" ? xset::name::sortx_hidfirst : xset::name::sortx_hidlast;
            xset_set_b(name, true);
            file_browser->set_sort_extra(name);
        }
        else if (property == "sort-first")
        {
            const std::string subproperty = json["subproperty"];

            xset::name name;
            if (subproperty == "files")
            {
                name = xset::name::sortx_files;
            }
            else if (subproperty == "directories")
            {
                name = xset::name::sortx_directories;
            }
            else if (subproperty == "mixed")
            {
                name = xset::name::sortx_mix;
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid {} value", subproperty)};
            }
            file_browser->set_sort_extra(name);
        }
        else if (property == "show-thumbnails")
        {
            const std::string subproperty = json["subproperty"];

            if (app_settings.show_thumbnail() != (subproperty == "true"))
            {
                main_window_toggle_thumbnails_all_windows();
            }
        }
        else if (property == "max-thumbnail-size")
        {
            const std::string_view value = data[0];

            app_settings.max_thumb_size(std::stoi(value.data()));
        }
        else if (property == "large-icons")
        {
            const std::string subproperty = json["subproperty"];

            if (!file_browser->is_view_mode(ptk::file_browser::view_mode::icon_view))
            {
                xset_set_b_panel_mode(panel,
                                      xset::panel::list_large,
                                      main_window->panel_context.at(panel),
                                      subproperty == "true");
                update_views_all_windows(nullptr, file_browser);
            }
        }
        else if (property == "pathbar-text")
        { // TEXT [[SELSTART] SELEND]
            const std::string_view value = data[0];

            GtkEntry* path_bar = file_browser->path_bar();

#if (GTK_MAJOR_VERSION == 4)
            gtk_editable_set_text(GTK_EDITABLE(path_bar), value.data());
#elif (GTK_MAJOR_VERSION == 3)
            gtk_entry_set_text(GTK_ENTRY(path_bar), value.data());
#endif

            gtk_editable_set_position(GTK_EDITABLE(path_bar), -1);
            // gtk_editable_select_region(GTK_EDITABLE(path_bar), width, height);
            gtk_widget_grab_focus(GTK_WIDGET(path_bar));
        }
        else if (property == "clipboard-text" || property == "clipboard-primary-text")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
            const std::string_view value = data[0];

            if (!g_utf8_validate(value.data(), -1, nullptr))
            {
                return {SOCKET_INVALID, "text is not valid UTF-8"};
            }
            GtkClipboard* clip = gtk_clipboard_get(
                property == "clipboard-text" ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY);
            const std::string str = unescape(value);
            gtk_clipboard_set_text(clip, str.data(), -1);
#endif
        }
        else if (property == "clipboard-from-file" || property == "clipboard-primary-from-file")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
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
            GtkClipboard* clip =
                gtk_clipboard_get(property == "clipboard-from-file" ? GDK_SELECTION_CLIPBOARD
                                                                    : GDK_SELECTION_PRIMARY);
            gtk_clipboard_set_text(clip, contents.data(), -1);
#endif
        }
        else if (property == "clipboard-cut-files" || property == "clipboard-copy-files")
        {
            ptk_clipboard_cut_or_copy_file_list(data, property == "clipboard_copy_files");
        }
        else if (property == "selected-filenames" || property == "selected-files")
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
        else if (property == "unselected-filenames" || property == "unselected-files")
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
        else if (property == "selected-pattern")
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
        else if (property == "current-dir")
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
            file_browser->chdir(value);
        }
        else if (property == "thumbnailer")
        {
            const std::string subproperty = json["subproperty"];
            if (subproperty == "api")
            {
                app_settings.thumbnailer_use_api(true);
            }
            else // if (subproperty == "cli"))
            {
                app_settings.thumbnailer_use_api(false);
            }
        }
        else if (property == "editor")
        {
            const std::string_view value = data[0];

            if (!value.ends_with(".desktop"))
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
        else if (property == "terminal")
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
                if (terminal.string() == supported_terminal)
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
    else if (command == "get")
    {
        // get
        if (property == "window-size")
        {
            i32 width;
            i32 height;
            gtk_window_get_default_size(GTK_WINDOW(main_window), &width, &height);
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
        }
        else if (property == "window-position")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
            i32 width;
            i32 height;
            gtk_window_get_position(GTK_WINDOW(main_window), &width, &height);
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
#endif
        }
        else if (property == "window-maximized")
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->maximized)};
        }
        else if (property == "window-fullscreen")
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->fullscreen)};
        }
        else if (property == "screen-size")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
            GdkRectangle workarea = GdkRectangle();
            gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
                                     &workarea);
            return {SOCKET_SUCCESS, std::format("{}x{}", workarea.width, workarea.height)};
#endif
        }
        else if (property == "window-vslider-top" || property == "window-vslider-bottom" ||
                 property == "window-hslider" || property == "window-tslider")
        {
            GtkPaned* pane;
            if (property == "window-vslider-top")
            {
                pane = main_window->hpane_top;
            }
            else if (property == "window-vslider-bottom")
            {
                pane = main_window->hpane_bottom;
            }
            else if (property == "window-hslider")
            {
                pane = main_window->vpane;
            }
            else if (property == "window-tslider")
            {
                pane = main_window->task_vpane;
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(pane))};
        }
        else if (property == "focused-panel")
        {
            return {SOCKET_SUCCESS, std::format("{}", main_window->curpanel)};
        }
        else if (property == "focused-pane")
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
            else if (file_browser->path_bar() &&
                     gtk_widget_is_focus(GTK_WIDGET(file_browser->path_bar())))
            {
                return {SOCKET_SUCCESS, "pathbar"};
            }
        }
        else if (property == "current-tab")
        {
            return {SOCKET_SUCCESS,
                    std::format("{}",
                                gtk_notebook_page_num(main_window->get_panel_notebook(panel),
                                                      GTK_WIDGET(file_browser)) +
                                    1)};
        }
        else if (property == "panel-count")
        {
            const auto counts = main_window_get_counts(file_browser);
            const panel_t panel_count = counts.panel_count;
            // const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", panel_count)};
        }
        else if (property == "tab-count")
        {
            const auto counts = main_window_get_counts(file_browser);
            // const panel_t panel_count = counts.panel_count;
            const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", tab_count)};
        }
        else if (property == "devices-visible" || property == "dirtree-visible" ||
                 property == "toolbar-visible" || property == "sidetoolbar-visible" ||
                 property == "hidden-files-visible" || property == "panel1-visible" ||
                 property == "panel2-visible" || property == "panel3-visible" ||
                 property == "panel4-visible")
        {
            bool valid = false;
            bool use_mode = false;
            xset::panel xset_panel_var;
            if (property == "devices-visible")
            {
                xset_panel_var = xset::panel::show_devmon;
                use_mode = true;
                valid = true;
            }
            else if (property == "dirtree-visible")
            {
                xset_panel_var = xset::panel::show_dirtree;
                use_mode = true;
                valid = true;
            }
            else if (property == "toolbar-visible")
            {
                xset_panel_var = xset::panel::show_toolbox;
                use_mode = true;
                valid = true;
            }
            else if (property == "sidetoolbar-visible")
            {
                xset_panel_var = xset::panel::show_sidebar;
                use_mode = true;
                valid = true;
            }
            else if (property == "hidden-files-visible")
            {
                xset_panel_var = xset::panel::show_hidden;
                valid = true;
            }
            else if (property.starts_with("panel"))
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
        else if (property == "panel-hslider-top" || property == "panel-hslider-bottom" ||
                 property == "panel-vslider")
        {
            GtkPaned* pane;
            if (property == "panel-hslider-top")
            {
                pane = file_browser->side_vpane_top;
            }
            else if (property == "panel-hslider-bottom")
            {
                pane = file_browser->side_vpane_bottom;
            }
            else if (property == "panel-vslider")
            {
                pane = file_browser->hpane;
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(pane))};
        }
        else if (property == "column-width")
        { // COLUMN
            const std::string subproperty = json["subproperty"];

            if (file_browser->is_view_mode(ptk::file_browser::view_mode::list_view))
            {
                bool found = false;
                GtkTreeViewColumn* col = nullptr;
                for (const auto [index, column_title] : std::views::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view()),
                                                   static_cast<i32>(index));
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (subproperty == title)
                    {
                        found = true;
                        break;
                    }
                    if (title == column_title &&
                        (subproperty == "name" || subproperty == "size" || subproperty == "bytes" ||
                         subproperty == "type" || subproperty == "mime" ||
                         subproperty == "permission" || subproperty == "owner" ||
                         subproperty == "group" || subproperty == "accessed" ||
                         subproperty == "created" || subproperty == "metadata" ||
                         subproperty == "modified"))
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
        else if (property == "sort-by")
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
        else if (property == "sort-ascend" || property == "sort-natural" ||
                 property == "sort-case" || property == "sort-hidden-first" ||
                 property == "sort-first" || property == "panel-hslider-top")
        {
            if (property == "sort-ascend")
            {
                return {SOCKET_SUCCESS,
                        std::format(
                            "{}",
                            file_browser->is_sort_type(GtkSortType::GTK_SORT_ASCENDING) ? 1 : 0)};
            }
            else if (property == "sort-natural")
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel(file_browser->panel(), xset::panel::sort_extra)
                                        ? 1
                                        : 0)};
            }
            else if (property == "sort-case")
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
            else if (property == "sort-hidden-first")
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_int_panel(file_browser->panel(),
                                                       xset::panel::sort_extra,
                                                       xset::var::z) == xset::b::xtrue
                                        ? 1
                                        : 0)};
            }
            else if (property == "sort-first")
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
        else if (property == "show-thumbnails")
        {
            return {SOCKET_SUCCESS, std::format("{}", app_settings.show_thumbnail() ? 1 : 0)};
        }
        else if (property == "max-thumbnail-size")
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", vfs_file_size_format(app_settings.max_thumb_size()))};
        }
        else if (property == "large-icons")
        {
            return {SOCKET_SUCCESS, std::format("{}", file_browser->using_large_icons() ? 1 : 0)};
        }
        else if (property == "statusbar-text")
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", gtk_label_get_text(file_browser->status_label))};
        }
        else if (property == "pathbar-text")
        {
            GtkEntry* path_bar = file_browser->path_bar();

#if (GTK_MAJOR_VERSION == 4)
            const std::string text = gtk_editable_get_text(GTK_EDITABLE(path_bar));
#elif (GTK_MAJOR_VERSION == 3)
            const std::string text = gtk_entry_get_text(GTK_ENTRY(path_bar));
#endif

            return {SOCKET_SUCCESS, text};
        }
        else if (property == "clipboard-text" || property == "clipboard-primary-text")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
            GtkClipboard* clip = gtk_clipboard_get(
                property == "clipboard-text" ? GDK_SELECTION_CLIPBOARD : GDK_SELECTION_PRIMARY);
            return {SOCKET_SUCCESS, gtk_clipboard_wait_for_text(clip)};
#endif
        }
        else if (property == "clipboard-cut-files" || property == "clipboard-copy-files")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
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
                if (property == "clipboard-copy-files")
                {
                    gtk_selection_data_free(sel_data);
                    return {SOCKET_SUCCESS, ""};
                }
            }
            else if (property == "clipboard-cut-files")
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
#endif
        }
        else if (property == "selected-filenames" || property == "selected-files")
        {
            const auto selected_files = file_browser->selected_files();
            if (selected_files.empty())
            {
                return {SOCKET_SUCCESS, ""};
            }

            // build fish array
            std::string str;
            for (const auto& file : selected_files)
            {
                if (!file)
                {
                    continue;
                }
                str.append(std::format("{} ", ztd::shell::quote(file->name())));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (property == "selected-pattern")
        {
        }
        else if (property == "current-dir")
        {
            return {SOCKET_SUCCESS, std::format("{}", file_browser->cwd().string())};
        }
        else if (property == "thumbnailer")
        {
            return {SOCKET_SUCCESS, app_settings.thumbnailer_use_api() ? "api" : "cli"};
        }
        else if (property == "editor")
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
        else if (property == "terminal")
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
    else if (command == "set-task")
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
                if (str == data[i])
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
        if (ptask->task->type_ != vfs::file_task::type::exec)
        {
            return {SOCKET_INVALID, std::format("internal task {} is read-only", data[i])};
        }

        // set model value
        i32 j;
        if (property == "icon")
        {
            ptk_file_task_lock(ptask);
            ptask->task->exec_icon = value;
            ptask->pause_change_view = ptask->pause_change = true;
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "count")
        {
            j = task_view_column::count;
        }
        else if (property == "directory" || subproperty == "from")
        {
            j = task_view_column::path;
        }
        else if (property == "item")
        {
            j = task_view_column::file;
        }
        else if (property == "to")
        {
            j = task_view_column::to;
        }
        else if (property == "progress")
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
            ptask->task->custom_percent = value != "0";
            ptask->pause_change_view = ptask->pause_change = true;
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "total")
        {
            j = task_view_column::total;
        }
        else if (property == "curspeed")
        {
            j = task_view_column::curspeed;
        }
        else if (property == "curremain")
        {
            j = task_view_column::curest;
        }
        else if (property == "avgspeed")
        {
            j = task_view_column::avgspeed;
        }
        else if (property == "avgremain")
        {
            j = task_view_column::avgest;
        }
        else if (property == "queue_state")
        {
            if (subproperty == "run")
            {
                ptk_file_task_pause(ptask, vfs::file_task::state::running);
            }
            else if (subproperty == "pause")
            {
                ptk_file_task_pause(ptask, vfs::file_task::state::pause);
            }
            else if (subproperty == "queue" || subproperty == "queued")
            {
                ptk_file_task_pause(ptask, vfs::file_task::state::queue);
            }
            else if (subproperty == "stop")
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
        else
        {
            return {SOCKET_INVALID, std::format("invalid task property '{}'", subproperty)};
        }
        gtk_list_store_set(GTK_LIST_STORE(model), &it, j, value.data(), -1);
    }
    else if (command == "get-task")
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
                if (str == property)
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
        if (property == "icon")
        {
            ptk_file_task_lock(ptask);
            if (!ptask->task->exec_icon.empty())
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->task->exec_icon)};
            }
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "count")
        {
            j = task_view_column::count;
        }
        else if (property == "directory" || property == "from")
        {
            j = task_view_column::path;
        }
        else if (property == "item")
        {
            j = task_view_column::file;
        }
        else if (property == "to")
        {
            j = task_view_column::to;
        }
        else if (property == "progress")
        {
            return {SOCKET_SUCCESS, std::format("{}", ptask->task->percent)};
        }
        else if (property == "total")
        {
            j = task_view_column::total;
        }
        else if (property == "curspeed")
        {
            j = task_view_column::curspeed;
        }
        else if (property == "curremain")
        {
            j = task_view_column::curest;
        }
        else if (property == "avgspeed")
        {
            j = task_view_column::avgspeed;
        }
        else if (property == "avgremain")
        {
            j = task_view_column::avgest;
        }
        else if (property == "elapsed")
        {
            j = task_view_column::elapsed;
        }
        else if (property == "started")
        {
            j = task_view_column::started;
        }
        else if (property == "status")
        {
            j = task_view_column::status;
        }
        else if (property == "queue_state")
        {
            if (ptask->task->state_pause_ == vfs::file_task::state::running)
            {
                return {SOCKET_SUCCESS, "run"};
            }
            else if (ptask->task->state_pause_ == vfs::file_task::state::pause)
            {
                return {SOCKET_SUCCESS, "pause"};
            }
            else if (ptask->task->state_pause_ == vfs::file_task::state::queue)
            {
                return {SOCKET_SUCCESS, "queue"};
            }
            else
            { // failsafe
                return {SOCKET_SUCCESS, "stop"};
            }
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
    else if (command == "run-task")
    { // TYPE [OPTIONS] ...
        if (property == "cmd" || property == "command")
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
            ptask->task->exec_sync = opt_task;
            ptask->task->exec_popup = opt_popup;
            ptask->task->exec_show_output = opt_popup;
            ptask->task->exec_show_error = true;
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
        else if (property == "edit")
        { // edit FILE
            const std::vector<std::string> data = json["data"];
            const std::string_view value = data[0];

            if (!std::filesystem::is_regular_file(value))
            {
                return {SOCKET_INVALID, std::format("no such file '{}'", value)};
            }
            xset_edit(GTK_WIDGET(file_browser), value);
        }
        else if (property == "mount" || property == "umount")
        { // mount or unmount TARGET
            const std::vector<std::string> data = json["data"];
            const std::string_view value = data[0];

            // Resolve TARGET
            if (!std::filesystem::exists(value))
            {
                return {SOCKET_INVALID, std::format("path does not exist '{}'", value)};
            }

            const auto real_path_stat = ztd::statx(value);
            std::shared_ptr<vfs::volume> vol = nullptr;
            if (property == "umount" && std::filesystem::is_directory(value))
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
                if (property == "mount")
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
            ptask->task->exec_sync = true;
            ptask->task->exec_show_error = true;
            ptk_file_task_run(ptask);
        }
        else if (property == "copy" || property == "move" || property == "link" ||
                 property == "delete" || property == "trash")
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
            if (property != "delete" || property != "trash")
            {
                if (!target_dir.string().starts_with('/'))
                {
                    return {SOCKET_INVALID,
                            std::format("TARGET must be absolute '{}'", target_dir.string())};
                }
            }

            std::vector<std::filesystem::path> file_list;
            for (const std::string_view file : opt_file_list)
            {
                if (file.starts_with('/'))
                { // absolute path
                    file_list.push_back(file);
                }
                else
                { // relative path
                    if (opt_cwd.empty())
                    {
                        return {SOCKET_INVALID,
                                std::format("relative path '{}' requires option --dir DIR", file)};
                    }
                    file_list.push_back(opt_cwd / file);
                }
            }

            if (property != "delete" || property != "trash")
            {
                // remove TARGET from file list
                file_list.pop_back();
            }

            if (file_list.empty() || (property != "delete" && property != "trash"))
            {
                return {SOCKET_INVALID,
                        std::format("task type {} requires FILE argument(s)", data[i])};
            }
            vfs::file_task::type task_type;
            if (property == "copy")
            {
                task_type = vfs::file_task::type::copy;
            }
            else if (property == "move")
            {
                task_type = vfs::file_task::type::move;
            }
            else if (property == "link")
            {
                task_type = vfs::file_task::type::link;
            }
            else if (property == "delete")
            {
                task_type = vfs::file_task::type::del;
            }
            else if (property == "trash")
            {
                task_type = vfs::file_task::type::trash;
            }
            else
            { // failsafe
                return {SOCKET_FAILURE, std::format("invalid task type '{}'", property)};
            }

#if (GTK_MAJOR_VERSION == 4)
            GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(file_browser)));
#elif (GTK_MAJOR_VERSION == 3)
            GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
#endif

            PtkFileTask* ptask = ptk_file_task_new(task_type,
                                                   file_list,
                                                   target_dir,
                                                   GTK_WINDOW(parent),
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
    else if (command == "emit-key")
    { // KEYCODE [KEYMOD]
#if 0
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
#else
        return {SOCKET_INVALID, "Not Implemented"};
#endif
    }
    else if (command == "activate")
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

#if (GTK_MAJOR_VERSION == 4)
            GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
            GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

            xset_add_menuitem(file_browser, GTK_WIDGET(widget), accel_group, set);
            g_idle_add((GSourceFunc)delayed_show_menu, widget);
        }
        else
        {
            // activate item
            main_window_keypress(nullptr, nullptr, set.get());
        }
    }
    else if (command == "add-event" || command == "replace-event" || command == "remove-event")
    {
        const std::vector<std::string> data = json["data"];

        xset_t set = xset_is(data[i]);
        if (!set)
        {
            return {SOCKET_INVALID, std::format("invalid event type '{}'", data[i])};
        }
        // build command
        std::string str = (command == "replace-event" ? "*" : "");
        // the first value in data is ignored as it is the xset name
        const std::vector<std::string> event_cmds = {data.cbegin() + 1, data.cend()};
        for (const std::string_view event_cmd : event_cmds)
        {
            str.append(std::format(" {}", event_cmd));
        }
        str = ztd::strip(str); // can not have any extra whitespace
        // modify list
        GList* l = nullptr;
        if (command == "remove-event")
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
    else if (command == "help")
    {
        return {SOCKET_SUCCESS, "For help run, 'man spacefm-socket'"};
    }
    else if (command == "ping")
    {
        return {SOCKET_SUCCESS, "pong"};
    }
    else
    {
        return {SOCKET_FAILURE, std::format("invalid socket method '{}'", command)};
    }
    return {SOCKET_SUCCESS, ""};
}
