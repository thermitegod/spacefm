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

#include <memory>

#include <ranges>

#include <gtkmm.h>
#include <glibmm.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"
#include "logger.hxx"

#include "types.hxx"
#include "autosave.hxx"

#include "settings/settings.hxx"

#include "terminal-handlers.hxx"

#include "main-window.hxx"

#include "xset/xset-lookup.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-location-view.hxx"

#include "preference-dialog.hxx"

/**
 * General Tab
 */

static void
update_big_icons(const std::shared_ptr<config::settings>& settings,
                 const datatype::settings_extended& new_settings) noexcept
{
    if (settings->icon_size_big == new_settings.settings.icon_size_big)
    {
        return;
    }

    settings->icon_size_big = new_settings.settings.icon_size_big;

    vfs::dir::global_unload_thumbnails(vfs::file::thumbnail_size::big);

    // update all windows/all panels/all browsers
    for (MainWindow* window : main_window_get_all())
    {
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                // update views
                gtk_widget_destroy(browser->folder_view());
                browser->folder_view(nullptr);
                if (browser->side_dir)
                {
                    gtk_widget_destroy(browser->side_dir);
                    browser->side_dir = nullptr;
                }
                browser->update_views();
            }
        }
    }
    ptk::view::location::update_volume_icons();
}

static void
update_small_icons(const std::shared_ptr<config::settings>& settings,
                   const datatype::settings_extended& new_settings) noexcept
{
    if (settings->icon_size_small == new_settings.settings.icon_size_small)
    {
        return;
    }

    settings->icon_size_small = new_settings.settings.icon_size_small;

    vfs::dir::global_unload_thumbnails(vfs::file::thumbnail_size::small);

    // update all windows/all panels/all browsers
    for (MainWindow* window : main_window_get_all())
    {
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                // update views
                gtk_widget_destroy(browser->folder_view());
                browser->folder_view(nullptr);
                if (browser->side_dir)
                {
                    gtk_widget_destroy(browser->side_dir);
                    browser->side_dir = nullptr;
                }
                browser->update_views();
            }
        }
    }
    ptk::view::location::update_volume_icons();
}

static void
update_tool_icons(const std::shared_ptr<config::settings>& settings,
                  const datatype::settings_extended& new_settings) noexcept
{
    if (settings->icon_size_tool == new_settings.settings.icon_size_tool)
    {
        return;
    }

    settings->icon_size_tool = new_settings.settings.icon_size_tool;

    main_window_rebuild_all_toolbars(nullptr);
}

static void
update_thumbnail_show(const std::shared_ptr<config::settings>& settings,
                      const datatype::settings_extended& new_settings) noexcept
{
    if (settings->show_thumbnails == new_settings.settings.show_thumbnails)
    {
        return;
    }

    settings->show_thumbnails = new_settings.settings.show_thumbnails;

    // update all windows/all panels/all browsers
    main_window_reload_thumbnails_all_windows();
}

static void
update_thumbnail_size_limits(const std::shared_ptr<config::settings>& settings,
                             const datatype::settings_extended& new_settings) noexcept
{
    if (settings->thumbnail_size_limit == new_settings.settings.thumbnail_size_limit)
    {
        return;
    }

    settings->thumbnail_size_limit = new_settings.settings.thumbnail_size_limit;
}

static void
update_thumbnail_max_size(const std::shared_ptr<config::settings>& settings,
                          const datatype::settings_extended& new_settings) noexcept
{
    if (settings->thumbnail_max_size == new_settings.settings.thumbnail_max_size)
    {
        return;
    }

    settings->thumbnail_max_size = new_settings.settings.thumbnail_max_size;

    main_window_reload_thumbnails_all_windows();
}

/**
 * Interface Tab
 */
static void
update_show_toolbar_home(const std::shared_ptr<config::settings>& settings,
                         const datatype::settings_extended& new_settings) noexcept
{
    if (settings->show_toolbar_home == new_settings.settings.show_toolbar_home)
    {
        return;
    }

    settings->show_toolbar_home = new_settings.settings.show_toolbar_home;

    for (MainWindow* window : main_window_get_all())
    { // update all windows/all panels/all browsers
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));

                if (settings->show_toolbar_home)
                {
                    gtk_widget_show(GTK_WIDGET(browser->toolbar_home));
                }
                else
                {
                    gtk_widget_hide(GTK_WIDGET(browser->toolbar_home));
                }
            }
        }
    }
}

static void
update_show_toolbar_refresh(const std::shared_ptr<config::settings>& settings,
                            const datatype::settings_extended& new_settings) noexcept
{
    if (settings->show_toolbar_refresh == new_settings.settings.show_toolbar_refresh)
    {
        return;
    }

    settings->show_toolbar_refresh = new_settings.settings.show_toolbar_refresh;

    for (MainWindow* window : main_window_get_all())
    { // update all windows/all panels/all browsers
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));

                if (settings->show_toolbar_refresh)
                {
                    gtk_widget_show(GTK_WIDGET(browser->toolbar_refresh));
                }
                else
                {
                    gtk_widget_hide(GTK_WIDGET(browser->toolbar_refresh));
                }
            }
        }
    }
}

static void
update_show_toolbar_search(const std::shared_ptr<config::settings>& settings,
                           const datatype::settings_extended& new_settings) noexcept
{
    if (settings->show_toolbar_search == new_settings.settings.show_toolbar_search)
    {
        return;
    }

    settings->show_toolbar_search = new_settings.settings.show_toolbar_search;

    for (MainWindow* window : main_window_get_all())
    { // update all windows/all panels/all browsers
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));

                if (settings->show_toolbar_search)
                {
                    gtk_widget_show(GTK_WIDGET(browser->search_bar_));
                }
                else
                {
                    gtk_widget_hide(GTK_WIDGET(browser->search_bar_));
                }
            }
        }
    }
}

static void
update_show_tab_bar(const std::shared_ptr<config::settings>& settings,
                    const datatype::settings_extended& new_settings) noexcept
{
    if (settings->always_show_tabs == new_settings.settings.always_show_tabs)
    {
        return;
    }

    settings->always_show_tabs = new_settings.settings.always_show_tabs;

    for (MainWindow* window : main_window_get_all())
    { // update all windows/all panels
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 n = gtk_notebook_get_n_pages(notebook);
            if (settings->always_show_tabs)
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

static void
update_hide_close_tab(const std::shared_ptr<config::settings>& settings,
                      const datatype::settings_extended& new_settings) noexcept
{
    if (settings->show_close_tab_buttons == new_settings.settings.show_close_tab_buttons)
    {
        return;
    }

    settings->show_close_tab_buttons = new_settings.settings.show_close_tab_buttons;

    for (MainWindow* window : main_window_get_all())
    { // update all windows/all panels/all browsers
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = window->get_panel_notebook(p);
            const i32 total_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : std::views::iota(0z, total_pages))
            {
                ptk::browser* browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                GtkWidget* tab_label = window->create_tab_label(browser);
                gtk_notebook_set_tab_label(notebook, GTK_WIDGET(browser), GTK_WIDGET(tab_label));
                browser->update_tab_label();
            }
        }
    }
}

static void
update_new_tab(const std::shared_ptr<config::settings>& settings,
               const datatype::settings_extended& new_settings) noexcept
{
    if (settings->new_tab_here == new_settings.settings.new_tab_here)
    {
        return;
    }

    settings->new_tab_here = new_settings.settings.new_tab_here;
}

static void
update_confirm(const std::shared_ptr<config::settings>& settings,
               const datatype::settings_extended& new_settings) noexcept
{
    if (settings->confirm == new_settings.settings.confirm)
    {
        return;
    }

    settings->confirm = new_settings.settings.confirm;
}

static void
update_confirm_trash(const std::shared_ptr<config::settings>& settings,
                     const datatype::settings_extended& new_settings) noexcept
{
    if (settings->confirm_trash == new_settings.settings.confirm_trash)
    {
        return;
    }

    settings->confirm_trash = new_settings.settings.confirm_trash;
}

static void
update_confirm_delete(const std::shared_ptr<config::settings>& settings,
                      const datatype::settings_extended& new_settings) noexcept
{
    if (settings->confirm_delete == new_settings.settings.confirm_delete)
    {
        return;
    }

    settings->confirm_delete = new_settings.settings.confirm_delete;
}

static void
update_si_prefix(const std::shared_ptr<config::settings>& settings,
                 const datatype::settings_extended& new_settings) noexcept
{
    if (settings->use_si_prefix == new_settings.settings.use_si_prefix)
    {
        return;
    }

    settings->use_si_prefix = new_settings.settings.use_si_prefix;

    main_window_refresh_all();
}

static void
update_click_executes(const std::shared_ptr<config::settings>& settings,
                      const datatype::settings_extended& new_settings) noexcept
{
    if (settings->click_executes == new_settings.settings.click_executes)
    {
        return;
    }

    settings->click_executes = new_settings.settings.click_executes;
}

static void
update_drag_actions(const std::shared_ptr<config::settings>& settings,
                    const datatype::settings_extended& new_settings) noexcept
{
    (void)settings;

    const auto current_drag_action = xset_get_int(xset::name::drag_action, xset::var::x);
    if (current_drag_action == new_settings.drag_action)
    {
        return;
    }

    xset_set(xset::name::drag_action, xset::var::x, std::format("{}", new_settings.drag_action));
}

/**
 * Advanced Tab
 */

static void
update_editor(const std::shared_ptr<config::settings>& settings,
              const datatype::settings_extended& new_settings) noexcept
{
    (void)settings;

    xset_set(xset::name::editor, xset::var::s, new_settings.editor);
}

static void
update_terminal(const std::shared_ptr<config::settings>& settings,
                const datatype::settings_extended& new_settings) noexcept
{
    (void)settings;

    const std::filesystem::path terminal = Glib::find_program_in_path(new_settings.terminal);
    if (terminal.empty())
    {
        logger::error("Failed to set new terminal: {}, not installed", new_settings.terminal);
        return;
    }
    xset_set(xset::name::main_terminal, xset::var::s, new_settings.terminal);
    xset_set_b(xset::name::main_terminal, true); // discovery
}

void
show_preference_dialog(GtkWindow* parent,
                       const std::shared_ptr<config::settings>& settings) noexcept
{
    (void)parent;

    const auto buffer = glz::write_json(datatype::settings_extended{
        .settings = *settings,
        .drag_action = xset_get_int(xset::name::drag_action, xset::var::x),
        .editor = xset_get_s(xset::name::editor).value_or(""),
        .terminal = xset_get_s(xset::name::main_terminal).value_or(""),
        .details = {.supported_terminals = terminal_handlers->get_supported_terminal_names()}});
    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return;
    }
    // logger::trace("{}", buffer.value());

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_PREFERENCE);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_PREFERENCE);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find preference dialog binary: {}", DIALOG_PREFERENCE);
        return;
    }

    const auto command = std::format(R"({} --json '{}')", binary, buffer.value());

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);
    if (exit_status != 0 || standard_output.empty())
    {
        return;
    }

    const auto results = glz::read_json<datatype::settings_extended>(standard_output);
    if (!results)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(results.error(), standard_output));
        return;
    }

    // update changed settings

    // General
    update_big_icons(settings, results.value());
    update_small_icons(settings, results.value());
    update_tool_icons(settings, results.value());
    update_thumbnail_show(settings, results.value());
    update_thumbnail_size_limits(settings, results.value());
    update_thumbnail_max_size(settings, results.value());

    // Interface
    update_show_toolbar_home(settings, results.value());
    update_show_toolbar_refresh(settings, results.value());
    update_show_toolbar_search(settings, results.value());
    update_show_tab_bar(settings, results.value());
    update_hide_close_tab(settings, results.value());
    update_new_tab(settings, results.value());
    update_confirm(settings, results.value());
    update_confirm_trash(settings, results.value());
    update_confirm_delete(settings, results.value());
    update_si_prefix(settings, results.value());
    update_click_executes(settings, results.value());
    update_drag_actions(settings, results.value());

    // Advanced
    update_editor(settings, results.value());
    update_terminal(settings, results.value());

    autosave::request_add();
}
