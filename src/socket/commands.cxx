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

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#if (GTK_MAJOR_VERSION == 4)
#include "compat/gtk4-porting.hxx"
#endif

#include "socket/commands.hxx"
#include "socket/datatypes.hxx"

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "gui/clipboard.hxx"
#include "gui/file-browser.hxx"
#include "gui/file-task-view.hxx"
#include "gui/main-window.hxx"

#include "vfs/utils/file-ops.hxx"
#include "vfs/utils/vfs-editor.hxx"
#include "vfs/utils/vfs-utils.hxx"
#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-terminals.hxx"
#include "vfs/vfs-volume.hxx"

#include "logger.hxx"
#include "types.hxx"

static std::string
unescape(const std::string_view t) noexcept
{
    std::string unescaped = t.data();
    unescaped = ztd::replace(unescaped, "\\\n", "\\n");
    unescaped = ztd::replace(unescaped, "\\\t", "\\t");
    unescaped = ztd::replace(unescaped, "\\\r", "\\r");
    unescaped = ztd::replace(unescaped, "\\\"", "\"");

    return unescaped;
}

#if 0
static bool
delayed_show_menu(GtkWidget* menu) noexcept
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
#endif

std::tuple<i32, std::string>
socket::command(const std::string_view socket_commands_json) noexcept
{
    // These are also the sockets return code
    static constexpr i32 SOCKET_SUCCESS = 0; // Successful exit status.
    static constexpr i32 SOCKET_FAILURE = 1; // Failing exit status.
    static constexpr i32 SOCKET_INVALID = 2; // Invalid request exit status.

    const auto request_data = glz::read_json<socket::socket_request_data>(socket_commands_json);
    if (!request_data)
    {
        logger::error<logger::domain::ptk>(
            "Failed to decode json: {}",
            glz::format_error(request_data.error(), socket_commands_json));

        return {SOCKET_FAILURE,
                std::format("Failed to decode json: {}",
                            glz::format_error(request_data.error(), socket_commands_json))};
    }

    // socket flags
    panel_t panel = request_data->panel;
    tab_t tab = request_data->tab;
    auto window = request_data->window;
    // socket commands
    // subproperty and data are only retrived in the properties that need them
    const auto command = request_data->command;
    const auto property = request_data->property;

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
            const std::string str = std::format("{}", logger::utils::ptr(window2));
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
    ptk::browser* browser = PTK_FILE_BROWSER_REINTERPRET(
        gtk_notebook_get_nth_page(main_window->get_panel_notebook(panel), tab.data() - 1));

    // command

    const i32 i = 0; // socket commands index

    if (command == "set")
    {
        const auto data = request_data->data;

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
            width = i32::create(size[0]).value_or(0);
            height = i32::create(size[1]).value_or(0);

            if (height < 1 || width < 1)
            {
                return {SOCKET_INVALID, std::format("invalid size {}", value)};
            }
            if (property == "window-size")
            {
                gtk_window_set_default_size(GTK_WINDOW(main_window), width.data(), height.data());
            }
            else
            {
#if (GTK_MAJOR_VERSION == 4)
                return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
                gtk_window_move(GTK_WINDOW(main_window), width.data(), height.data());
#endif
            }
        }
        else if (property == "window-maximized")
        {
            const auto subproperty = request_data->subproperty;

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
            const auto subproperty = request_data->subproperty;

            xset_set_b(xset::name::main_full, subproperty == "true");
            main_window->fullscreen_activate();
        }
        else if (property == "window-vslider-top" || property == "window-vslider-bottom" ||
                 property == "window-hslider" || property == "window-tslider")
        {
            const std::string_view value = data[0];

            const auto width = i32::create(value.data()).value_or(0);
            if (width <= 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }

            GtkPaned* pane = nullptr;
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

            gtk_paned_set_position(pane, width.data());
        }
        else if (property == "focused-panel")
        {
            const auto subproperty = request_data->subproperty;

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
            const auto subproperty = request_data->subproperty;

            GtkWidget* widget = nullptr;

            if (subproperty == "filelist")
            {
                widget = browser->folder_view();
            }
            else if (subproperty == "devices")
            {
                widget = browser->side_dev;
            }
            else if (subproperty == "dirtree")
            {
                widget = browser->side_dir;
            }
            else if (subproperty == "pathbar")
            {
                widget = GTK_WIDGET(browser->path_bar());
            }

            if (GTK_IS_WIDGET(widget))
            {
                gtk_widget_grab_focus(widget);
            }
        }
        else if (property == "current-tab")
        {
            const auto subproperty = request_data->subproperty;

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
            browser->go_tab(new_tab);
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
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_devmon,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, browser);
        }
        else if (property == "dirtree-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_dirtree,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, browser);
        }
        else if (property == "toolbar-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel_mode(panel,
                                  xset::panel::show_toolbox,
                                  main_window->panel_context.at(panel),
                                  subproperty == "true");
            update_views_all_windows(nullptr, browser);
        }
        else if (property == "hidden-files-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel(panel, xset::panel::show_hidden, subproperty == "true");
            update_views_all_windows(nullptr, browser);
        }
        else if (property == "panel1-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel(panel_1, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel2-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel(panel_2, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel3-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel(panel_3, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel4-visible")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b_panel(panel_4, xset::panel::show, subproperty == "true");
            show_panels_all_windows(nullptr, main_window);
        }
        else if (property == "panel-hslider-top" || property == "panel-hslider-bottom" ||
                 property == "panel-vslider")
        {
            const std::string_view value = data[0];

            const auto width = i32::create(value.data()).value_or(0);

            if (width <= 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }
            GtkPaned* pane = nullptr;
            if (property == "panel-hslider-top")
            {
                pane = browser->side_vpane_top;
            }
            else if (property == "panel-hslider-bottom")
            {
                pane = browser->side_vpane_bottom;
            }
            else
            {
                pane = browser->hpane;
            }
            gtk_paned_set_position(pane, width.data());
            browser->slider_release(nullptr);
            update_views_all_windows(nullptr, browser);
        }
        else if (property == "column-width")
        { // COLUMN WIDTH
            const std::string_view value = data[0];
            const auto subproperty = request_data->subproperty;

            const auto width = i32::create(value.data()).value_or(0);

            if (width < 1)
            {
                return {SOCKET_INVALID, "invalid column width"};
            }
            if (browser->is_view_mode(ptk::browser::view_mode::list_view))
            {
                bool found = false;
                GtkTreeViewColumn* col = nullptr;
                for (const auto [index, column_title] : std::views::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(browser->folder_view()),
                                                   static_cast<std::int32_t>(index));
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
                    gtk_tree_view_column_set_fixed_width(col, width.data());
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid column name '{}'", value)};
                }
            }
        }
        else if (property == "sort-by")
        { // COLUMN
            const auto subproperty = request_data->subproperty;

            auto j = ptk::browser::sort_order::name;
            if (subproperty == "name")
            {
                j = ptk::browser::sort_order::name;
            }
            else if (subproperty == "size")
            {
                j = ptk::browser::sort_order::size;
            }
            else if (subproperty == "bytes")
            {
                j = ptk::browser::sort_order::bytes;
            }
            else if (subproperty == "type")
            {
                j = ptk::browser::sort_order::type;
            }
            else if (subproperty == "mime")
            {
                j = ptk::browser::sort_order::mime;
            }
            else if (subproperty == "permission")
            {
                j = ptk::browser::sort_order::perm;
            }
            else if (subproperty == "owner")
            {
                j = ptk::browser::sort_order::owner;
            }
            else if (subproperty == "group")
            {
                j = ptk::browser::sort_order::group;
            }
            else if (subproperty == "accessed")
            {
                j = ptk::browser::sort_order::atime;
            }
            else if (subproperty == "created")
            {
                j = ptk::browser::sort_order::btime;
            }
            else if (subproperty == "metadata")
            {
                j = ptk::browser::sort_order::ctime;
            }
            else if (subproperty == "modified")
            {
                j = ptk::browser::sort_order::mtime;
            }

            else
            {
                return {SOCKET_INVALID, std::format("invalid column name '{}'", subproperty)};
            }
            browser->set_sort_order(j);
        }
        else if (property == "sort-ascend")
        {
            const auto subproperty = request_data->subproperty;

            browser->set_sort_type(subproperty == "true" ? GtkSortType::GTK_SORT_ASCENDING
                                                         : GtkSortType::GTK_SORT_DESCENDING);
        }
        else if (property == "sort-natural")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b(xset::name::sortx_natural, subproperty == "true");
            browser->set_sort_extra(xset::name::sortx_natural);
        }
        else if (property == "sort-case")
        {
            const auto subproperty = request_data->subproperty;

            xset_set_b(xset::name::sortx_case, subproperty == "true");
            browser->set_sort_extra(xset::name::sortx_case);
        }
        else if (property == "sort-hidden-first")
        {
            const auto subproperty = request_data->subproperty;

            const xset::name name =
                subproperty == "true" ? xset::name::sortx_hidfirst : xset::name::sortx_hidlast;
            xset_set_b(name, true);
            browser->set_sort_extra(name);
        }
        else if (property == "sort-first")
        {
            const auto subproperty = request_data->subproperty;

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
            browser->set_sort_extra(name);
        }
        else if (property == "show-thumbnails")
        {
            const auto subproperty = request_data->subproperty;
            if (browser->settings_->show_thumbnails != (subproperty == "true"))
            {
                main_window_toggle_thumbnails_all_windows();
            }
        }
        else if (property == "max-thumbnail-size")
        {
            const std::string_view value = data[0];
            browser->settings_->thumbnail_max_size =
                u32::create(value.data()).value_or(8 << 20).data();
        }
        else if (property == "large-icons")
        {
            const auto subproperty = request_data->subproperty;

            if (!browser->is_view_mode(ptk::browser::view_mode::icon_view))
            {
                xset_set_b_panel_mode(panel,
                                      xset::panel::list_large,
                                      main_window->panel_context.at(panel),
                                      subproperty == "true");
                update_views_all_windows(nullptr, browser);
            }
        }
        else if (property == "pathbar-text")
        { // TEXT [[SELSTART] SELEND]
            const std::string_view value = data[0];

            GtkEntry* path_bar = browser->path_bar();

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

            const auto buffer = vfs::utils::read_file(value);
            if (!buffer)
            {
                return {SOCKET_INVALID, std::format("error reading file '{}'", value)};
            }
            if (!g_utf8_validate(buffer->data(), -1, nullptr))
            {
                return {SOCKET_INVALID,
                        std::format("file '{}' does not contain valid UTF-8 text", value)};
            }
            GtkClipboard* clip =
                gtk_clipboard_get(property == "clipboard-from-file" ? GDK_SELECTION_CLIPBOARD
                                                                    : GDK_SELECTION_PRIMARY);
            gtk_clipboard_set_text(clip, buffer->data(), -1);
#endif
        }
        else if (property == "clipboard-cut-files" || property == "clipboard-copy-files")
        {
            ptk::clipboard::cut_or_copy_file_list(data, property == "clipboard_copy_files");
        }
        else if (property == "selected-filenames" || property == "selected-files")
        {
            const auto& select_filenames = data;

            if (select_filenames.empty())
            {
                // unselect all
                browser->unselect_all();
            }
            else
            {
                for (const std::filesystem::path select_filename : select_filenames)
                {
                    browser->select_file(select_filename.filename(), false);
                }
            }
        }
        else if (property == "unselected-filenames" || property == "unselected-files")
        {
            const auto& select_filenames = data;

            if (select_filenames.empty())
            {
                // unselect all
                browser->unselect_all();
            }
            else
            {
                for (const std::filesystem::path select_filename : select_filenames)
                {
                    browser->unselect_file(select_filename.filename(), false);
                }
            }
        }
        else if (property == "selected-pattern")
        {
            const std::string_view value = data[0];

            if (value.empty())
            {
                // unselect all
                browser->unselect_all();
            }
            else
            {
                browser->select_pattern(value);
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
            browser->chdir(value);
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

            const auto supported_terminals = vfs::terminals::supported_names();
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
            i32 width = 0;
            i32 height = 0;
            gtk_window_get_default_size(GTK_WINDOW(main_window), width.unwrap(), height.unwrap());
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
        }
        else if (property == "window-position")
        {
#if (GTK_MAJOR_VERSION == 4)
            return {SOCKET_INVALID, "Not Implemented"};
#elif (GTK_MAJOR_VERSION == 3)
            i32 width = 0;
            i32 height = 0;
            gtk_window_get_position(GTK_WINDOW(main_window), width.unwrap(), height.unwrap());
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
            GtkPaned* pane = nullptr;
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
            if (browser->folder_view() && gtk_widget_is_focus(browser->folder_view()))
            {
                return {SOCKET_SUCCESS, "filelist"};
            }
            else if (browser->side_dev && gtk_widget_is_focus(browser->side_dev))
            {
                return {SOCKET_SUCCESS, "devices"};
            }
            else if (browser->side_dir && gtk_widget_is_focus(browser->side_dir))
            {
                return {SOCKET_SUCCESS, "dirtree"};
            }
            else if (browser->path_bar() && gtk_widget_is_focus(GTK_WIDGET(browser->path_bar())))
            {
                return {SOCKET_SUCCESS, "pathbar"};
            }
        }
        else if (property == "current-tab")
        {
            return {SOCKET_SUCCESS,
                    std::format("{}",
                                gtk_notebook_page_num(main_window->get_panel_notebook(panel),
                                                      GTK_WIDGET(browser)) +
                                    1)};
        }
        else if (property == "panel-count")
        {
            const auto counts = browser->get_tab_panel_counts();
            const panel_t panel_count = counts.panel_count;
            // const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", panel_count)};
        }
        else if (property == "tab-count")
        {
            const auto counts = browser->get_tab_panel_counts();
            // const panel_t panel_count = counts.panel_count;
            const tab_t tab_count = counts.tab_count;
            // const tab_t tab_num = counts.tab_num;

            return {SOCKET_SUCCESS, std::format("{}", tab_count)};
        }
        else if (property == "devices-visible" || property == "dirtree-visible" ||
                 property == "toolbar-visible" || property == "hidden-files-visible" ||
                 property == "panel1-visible" || property == "panel2-visible" ||
                 property == "panel3-visible" || property == "panel4-visible")
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
            else if (property == "hidden-files-visible")
            {
                xset_panel_var = xset::panel::show_hidden;
                valid = true;
            }
            else if (property.starts_with("panel"))
            {
                const auto j = panel_t::create(property.substr(5, 1)).value_or(1);
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
            GtkPaned* pane = nullptr;
            if (property == "panel-hslider-top")
            {
                pane = browser->side_vpane_top;
            }
            else if (property == "panel-hslider-bottom")
            {
                pane = browser->side_vpane_bottom;
            }
            else if (property == "panel-vslider")
            {
                pane = browser->hpane;
            }
            else
            {
                return {SOCKET_FAILURE, std::format("unknown property '{}'", property)};
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(pane))};
        }
        else if (property == "column-width")
        { // COLUMN
            const auto subproperty = request_data->subproperty;

            if (browser->is_view_mode(ptk::browser::view_mode::list_view))
            {
                bool found = false;
                GtkTreeViewColumn* col = nullptr;
                for (const auto [index, column_title] : std::views::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(browser->folder_view()),
                                                   static_cast<std::int32_t>(index));
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
            return {SOCKET_SUCCESS, std::string(magic_enum::enum_name(browser->sort_order_))};
        }
        else if (property == "sort-ascend" || property == "sort-natural" ||
                 property == "sort-case" || property == "sort-hidden-first" ||
                 property == "sort-first" || property == "panel-hslider-top")
        {
            if (property == "sort-ascend")
            {
                return {
                    SOCKET_SUCCESS,
                    std::format("{}",
                                browser->is_sort_type(GtkSortType::GTK_SORT_ASCENDING) ? 1 : 0)};
            }
            else if (property == "sort-natural")
            {
                return {SOCKET_SUCCESS,
                        std::format(
                            "{}",
                            xset_get_b_panel(browser->panel(), xset::panel::sort_extra) ? 1 : 0)};
            }
            else if (property == "sort-case")
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel(browser->panel(), xset::panel::sort_extra) &&
                                            xset_get_int_panel(browser->panel(),
                                                               xset::panel::sort_extra,
                                                               xset::var::x)
                                                    .data() == xset::set::enabled::yes
                                        ? 1
                                        : 0)};
            }
            else if (property == "sort-hidden-first")
            {
                return {
                    SOCKET_SUCCESS,
                    std::format(
                        "{}",
                        xset_get_int_panel(browser->panel(), xset::panel::sort_extra, xset::var::z)
                                    .data() == xset::set::enabled::yes
                            ? 1
                            : 0)};
            }
            else if (property == "sort-first")
            {
                const auto value =
                    xset_get_int_panel(browser->panel(), xset::panel::sort_extra, xset::var::y);
                if (value == 0)
                {
                    return {SOCKET_SUCCESS, "mixed"};
                }
                else if (value == 1)
                {
                    return {SOCKET_SUCCESS, "directories"};
                }
                else if (value == 2)
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
            return {SOCKET_SUCCESS, std::format("{}", browser->settings_->show_thumbnails ? 1 : 0)};
        }
        else if (property == "max-thumbnail-size")
        {
            return {
                SOCKET_SUCCESS,
                std::format("{}",
                            vfs::utils::format_file_size(browser->settings_->thumbnail_max_size))};
        }
        else if (property == "large-icons")
        {
            return {SOCKET_SUCCESS, std::format("{}", browser->using_large_icons() ? 1 : 0)};
        }
        else if (property == "statusbar-text")
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", gtk_label_get_text(browser->statusbar_label))};
        }
        else if (property == "pathbar-text")
        {
            GtkEntry* path_bar = browser->path_bar();

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
            GdkAtom gnome_target = nullptr;
            GdkAtom uri_list_target = nullptr;
            GtkSelectionData* sel_data = nullptr;

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

            const char* uri_list_str = (const char*)gtk_selection_data_get_data(sel_data);
            if (std::string(uri_list_str).starts_with("cut"))
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
                str.append(std::format("{} ", ::utils::shell_quote(path)));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
#endif
        }
        else if (property == "selected-filenames" || property == "selected-files")
        {
            const auto selected_files = browser->selected_files();
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
                str.append(std::format("{} ", ::utils::shell_quote(file->name())));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (property == "selected-pattern")
        {
        }
        else if (property == "current-dir")
        {
            return {SOCKET_SUCCESS, std::format("{}", browser->cwd().string())};
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
        const auto subproperty = request_data->subproperty;
        const auto data = request_data->data;
        const std::string_view value = data[0];

        // find task
        GtkTreeIter it;
        ptk::file_task* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, ptk::view::file_task::column::data, &ptask, -1);
                const std::string str = std::format("{}", logger::utils::ptr(ptask));
                if (str == data[i.cast_unsigned().data()])
                {
                    break;
                }
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            return {SOCKET_INVALID,
                    std::format("invalid task '{}'", data[i.cast_unsigned().data()])};
        }
        if (ptask->task->type_ != vfs::file_task::type::exec)
        {
            return {SOCKET_INVALID,
                    std::format("internal task {} is read-only", data[i.cast_unsigned().data()])};
        }

        // set model value
        i32 j = 0;
        if (property == "icon")
        {
            ptask->lock();
            ptask->task->exec_icon = value;
            ptask->pause_change_view_ = ptask->pause_change_ = true;
            ptask->unlock();
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "count")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::count);
        }
        else if (property == "directory" || subproperty == "from")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::path);
        }
        else if (property == "item")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::file);
        }
        else if (property == "to")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::to);
        }
        else if (property == "progress")
        {
            if (value.empty())
            {
                ptask->task->percent = 50;
            }
            else
            {
                const auto v = i32::create(value).value_or(0);
                ptask->task->percent = v.max(0).min(100);
            }
            ptask->task->custom_percent = value != "0";
            ptask->pause_change_view_ = ptask->pause_change_ = true;
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "total")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::total);
        }
        else if (property == "curspeed")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::curspeed);
        }
        else if (property == "curremain")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::curest);
        }
        else if (property == "avgspeed")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::avgspeed);
        }
        else if (property == "avgremain")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::avgest);
        }
        else if (property == "queue_state")
        {
            if (subproperty == "run")
            {
                ptask->pause(vfs::file_task::state::running);
            }
            else if (subproperty == "pause")
            {
                ptask->pause(vfs::file_task::state::pause);
            }
            else if (subproperty == "queue" || subproperty == "queued")
            {
                ptask->pause(vfs::file_task::state::queue);
            }
            else if (subproperty == "stop")
            {
                ptk::view::file_task::stop(main_window->task_view,
                                           xset::set::get(xset::name::task_stop_all),
                                           nullptr);
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid queue_state '{}'", subproperty)};
            }
            ptk::view::file_task::start_queued(main_window->task_view, nullptr);
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
        ptk::file_task* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, ptk::view::file_task::column::data, &ptask, -1);
                const std::string str = std::format("{}", logger::utils::ptr(ptask));
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
        i32 j = 0;
        if (property == "icon")
        {
            ptask->lock();
            if (!ptask->task->exec_icon.empty())
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->task->exec_icon)};
            }
            ptask->unlock();
            return {SOCKET_SUCCESS, ""};
        }
        else if (property == "count")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::count);
        }
        else if (property == "directory" || property == "from")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::path);
        }
        else if (property == "item")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::file);
        }
        else if (property == "to")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::to);
        }
        else if (property == "progress")
        {
            return {SOCKET_SUCCESS, std::format("{}", ptask->task->percent)};
        }
        else if (property == "total")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::total);
        }
        else if (property == "curspeed")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::curspeed);
        }
        else if (property == "curremain")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::curest);
        }
        else if (property == "avgspeed")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::avgspeed);
        }
        else if (property == "avgremain")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::avgest);
        }
        else if (property == "elapsed")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::elapsed);
        }
        else if (property == "started")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::started);
        }
        else if (property == "status")
        {
            j = magic_enum::enum_integer(ptk::view::file_task::column::status);
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
        g_autofree char* str2 = nullptr;
        gtk_tree_model_get(model, &it, j, &str2, -1);
        if (str2)
        {
            return {SOCKET_SUCCESS, std::format("{}", str2)};
        }
    }
    else if (command == "run-task")
    { // TYPE [OPTIONS] ...
        if (property == "cmd" || property == "command")
        {
            // custom command task
            // cmd [--task [--popup] [--scroll]] [--terminal]
            //                     [--user USER] [--title TITLE]
            //                     [--icon ICON] [--dir DIR] COMMAND

            const auto data = request_data->data;

            const auto task_data = glz::read_json<socket::socket_task_data>(data[0]);
            if (!task_data)
            {
                logger::error<logger::domain::ptk>(
                    "Failed to decode json: {}",
                    glz::format_error(task_data.error(), socket_commands_json));

                return {SOCKET_FAILURE,
                        std::format("Failed to decode json: {}",
                                    glz::format_error(task_data.error(), data[0]))};
            }

            if (task_data->cmd.empty())
            {
                return {SOCKET_FAILURE, std::format("{} requires a command", command)};
            }
            std::string cmd;
            for (const std::string_view c : task_data->cmd)
            {
                cmd.append(std::format(" {}", c));
            }

            ptk::file_task* ptask =
                ptk_file_exec_new(!task_data->title.empty() ? task_data->title : cmd,
                                  !task_data->cwd.empty() ? task_data->cwd.data() : browser->cwd(),
                                  GTK_WIDGET(browser),
                                  browser->task_view());
            ptask->task->exec_browser = browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_icon = task_data->icon;
            ptask->task->exec_terminal = task_data->terminal;
            ptask->task->exec_sync = task_data->task;
            ptask->task->exec_popup = task_data->popup;
            ptask->task->exec_show_output = task_data->popup;
            ptask->task->exec_show_error = true;
            if (task_data->popup)
            {
                gtk_window_present(GTK_WINDOW(main_window));
            }
            ptask->run();
            if (task_data->task)
            {
                return {SOCKET_SUCCESS,
                        std::format("Note: $new_task_id not valid until approx one "
                                    "half second after task start\nnew_task_window={}\n"
                                    "new_task_id={}",
                                    logger::utils::ptr(main_window),
                                    logger::utils::ptr(ptask))};
            }
        }
        else if (property == "edit")
        { // edit FILE
            const auto data = request_data->data;
            const std::string_view value = data[0];

            if (!std::filesystem::is_regular_file(value))
            {
                return {SOCKET_INVALID, std::format("no such file '{}'", value)};
            }
            vfs::utils::open_editor(value);
        }
        else if (property == "mount" || property == "umount")
        { // mount or unmount TARGET
            const auto data = request_data->data;
            const std::string_view value = data[0];

            // Resolve TARGET
            if (!std::filesystem::exists(value))
            {
                return {SOCKET_INVALID, std::format("path does not exist '{}'", value)};
            }

            const auto real_path_stat = ztd::stat::create(value);
            std::shared_ptr<vfs::volume> vol = nullptr;
            if (property == "umount" && std::filesystem::is_directory(value))
            {
                // umount DIR
                if (vfs::is_path_mountpoint(value))
                {
                    if (!real_path_stat || !real_path_stat->is_block_file())
                    {
                        // NON-block device - try to find vol by mount point
                        vol = vfs::volume_get_by_device(value);
                        if (!vol)
                        {
                            return {SOCKET_INVALID, std::format("invalid TARGET '{}'", value)};
                        }
                    }
                }
            }
            else if (real_path_stat && real_path_stat->is_block_file())
            {
                // block device eg /dev/sda1
                vol = vfs::volume_get_by_device(value);
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
                    const auto mount_command = vol->device_mount_cmd();
                    if (mount_command)
                    {
                        cmd = mount_command.value();
                    }
                }
                else
                {
                    const auto unmount_command = vol->device_unmount_cmd();
                    if (unmount_command)
                    {
                        cmd = unmount_command.value();
                    }
                }
            }

            if (cmd.empty())
            {
                return {SOCKET_INVALID, std::format("invalid mount TARGET '{}'", value)};
            }
            // Task
            ptk::file_task* ptask = ptk_file_exec_new(property,
                                                      browser->cwd(),
                                                      GTK_WIDGET(browser),
                                                      browser->task_view());
            ptask->task->exec_browser = browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_terminal = false;
            ptask->task->exec_sync = true;
            ptask->task->exec_show_error = true;
            ptask->run();
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

            const auto data = request_data->data;

            const auto raw_file_task_data = glz::read_json<socket::socket_file_task_data>(data[0]);
            if (!raw_file_task_data)
            {
                logger::error<logger::domain::ptk>(
                    "Failed to decode json: {}",
                    glz::format_error(raw_file_task_data.error(), socket_commands_json));

                return {SOCKET_FAILURE,
                        std::format("Failed to decode json: {}",
                                    glz::format_error(raw_file_task_data.error(), data[0]))};
            }
            const auto& file_task_data = raw_file_task_data.value();

            if (file_task_data.files.empty())
            {
                return {SOCKET_INVALID, std::format("{} failed, missing file list", property)};
            }

            if (!file_task_data.dir.empty() && !std::filesystem::is_directory(file_task_data.dir))
            {
                return {SOCKET_INVALID,
                        std::format("no such directory '{}'", file_task_data.dir.string())};
            }

            // last argument is the TARGET
            const std::filesystem::path& target_dir = file_task_data.files.back();
            if (property != "delete" || property != "trash")
            {
                if (!target_dir.string().starts_with('/'))
                {
                    return {SOCKET_INVALID,
                            std::format("TARGET must be absolute '{}'", target_dir.string())};
                }
            }

            std::vector<std::filesystem::path> file_list;
            for (const std::string_view file : file_task_data.files)
            {
                if (file.starts_with('/'))
                { // absolute path
                    file_list.emplace_back(file);
                }
                else
                { // relative path
                    if (file_task_data.dir.empty())
                    {
                        return {SOCKET_INVALID,
                                std::format("relative path '{}' requires option --dir DIR", file)};
                    }
                    file_list.push_back(file_task_data.dir / file);
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
                        std::format("task type {} requires FILE argument(s)",
                                    data[i.cast_unsigned().data()])};
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
            GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(browser)));
#elif (GTK_MAJOR_VERSION == 3)
            GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
#endif

            ptk::file_task* ptask = ptk_file_task_new(task_type,
                                                      file_list,
                                                      target_dir,
                                                      GTK_WINDOW(parent),
                                                      browser->task_view());
            ptask->run();
            return {SOCKET_SUCCESS,
                    std::format("# Note: $new_task_id not valid until approx one "
                                "half second after task  start\nnew_task_window={}\n"
                                "new_task_id={}",
                                logger::utils::ptr(main_window),
                                logger::utils::ptr(ptask))};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task type '{}'", property)};
        }
    }
    else if (command == "emit-key")
    { // KEYCODE [KEYMOD]
#if 0
        const auto data = request_data->data;

        // this only handles keys assigned to menu items
        const auto event = (GdkEventKey*)gdk_event_new(GdkEventType::GDK_KEY_PRESS);
        event->keyval = std::stoul(data[i].data(), nullptr, 0);
        event->state = !data[i + 1].empty() ? std::stoul(data[i + 1], nullptr, 0) : 0;
        if (event->keyval)
        {
            gtk_window_present(GTK_WINDOW(main_window));
            main_window->keypress(event, nullptr);
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
#if 0
        const auto data = request_data->data;

        xset_t set = xset_find_custom(data[i]);
        if (!set)
        {
            return {SOCKET_INVALID,
                    std::format("custom command or submenu '{}' not found", data[i])};
        }
        if (set->menu.type == xset::set::menu_type::submenu)
        {
            // show submenu as popup menu
            set = xset::set::get(set->child.value());
            GtkWidget* widget = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
            GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
            GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

            xset_add_menuitem(browser, GTK_WIDGET(widget), accel_group, set);
            g_idle_add((GSourceFunc)delayed_show_menu, widget);
        }
        else
        {
            // activate item
            main_window->keypress(nullptr, set.get());
        }
#else
        return {SOCKET_INVALID, "Not Implemented"};
#endif
    }
    else if (command == "add-event" || command == "replace-event" || command == "remove-event")
    {
#if 0
        const auto data = request_data->data;

        const auto set = xset::set::get(data[i], true);
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
            l = g_list_find_custom((GList*)set->ob2_data,
                                   str.data(),
                                   (GCompareFunc)ztd::sort::compare);
            if (!l)
            {
                // remove replace event
                const std::string str2 = std::format("*{}", str);
                l = g_list_find_custom((GList*)set->ob2_data,
                                       str2.data(),
                                       (GCompareFunc)ztd::sort::compare);
            }
            if (!l)
            {
                return {SOCKET_INVALID, "event handler not found"};
            }
            l = g_list_remove((GList*)set->ob2_data, l->data);
        }
        else
        {
            l = g_list_append((GList*)set->ob2_data, ::utils::strdup(str));
        }
        set->ob2_data = (void*)l;
#else
        return {SOCKET_INVALID, "Not Implemented"};
#endif
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
