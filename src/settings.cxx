/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <ranges>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "types.hxx"

#include "main-window.hxx"

#include "xset/xset.hxx"
#include "xset/xset-defaults.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

#include "terminal-handlers.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "settings.hxx"

void
load_settings() noexcept
{
    const auto settings_config_dir = vfs::program::config();

    config::settings.load_saved_tabs = true;

    xset_defaults();

    const auto get_config_file_path = [settings_config_dir]()
    {
        auto path_toml = settings_config_dir / config::disk_format::filename;
        auto path_json = settings_config_dir / config::disk_format::filename_json;
        if (std::filesystem::exists(path_json))
        { // Try to load JSON config first
            return path_json;
        }
        else if (std::filesystem::exists(path_toml))
        { // Next try old TOML config
            return path_toml;
        }
        // No config found use JSON
        return path_json;
    };
    const auto session = get_config_file_path();

    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

#if defined(HAVE_DEV)
    const auto command_script =
        std::filesystem::path() / PACKAGE_SCRIPTS_PATH / "config-update-git";
    if (std::filesystem::exists(command_script))
    {
        const auto command_args =
            std::format("{} --config-dir {} --config-file {} --config-version {}",
                        command_script.string(),
                        settings_config_dir.string(),
                        session.filename().string(),
                        config::disk_format::version);

        logger::debug<logger::domain::dev>("SCRIPT({})", command_script.string());
        Glib::spawn_command_line_sync(command_args);
    }
#endif

    if (std::filesystem::is_regular_file(session))
    {
        config::load(session);
    }
    else
    {
        logger::info("No config file found, using defaults.");
    }

    // turn off fullscreen
    xset_set_b(xset::name::main_full, false);

    // terminal discovery
    const auto main_terminal = xset_get_s(xset::name::main_terminal);
    if (!main_terminal)
    {
        const auto supported_terminals = terminal_handlers->get_supported_terminal_names();
        for (const std::string_view supported_terminal : supported_terminals)
        {
            const std::filesystem::path terminal =
                Glib::find_program_in_path(supported_terminal.data());
            if (terminal.empty())
            {
                continue;
            }

            xset_set(xset::name::main_terminal, xset::var::s, terminal.filename().string());
            xset_set_b(xset::name::main_terminal, true); // discovery
            break;
        }
    }

    // editor discovery
    const auto main_editor = xset_get_s(xset::name::editor);
    if (!main_editor)
    {
        const auto mime_type = vfs::mime_type::create_from_type("text/plain");
        const auto default_app = mime_type->default_action();
        if (default_app)
        {
            const auto desktop = vfs::desktop::create(default_app.value());
            xset_set(xset::name::editor, xset::var::s, desktop->path().string());
        }
    }

    // set default keys
    xset_default_keys();
}

void
save_settings() noexcept
{
    // logger::info("save_settings");

    MainWindow* main_window = main_window_get_last_active();

    // save tabs
    const bool save_tabs = xset_get_b(xset::name::main_save_tabs);

    if (GTK_IS_WIDGET(main_window))
    {
        if (save_tabs)
        {
            for (const panel_t p : PANELS)
            {
                const auto set = xset::set::get(xset::panel::show, p);
                if (GTK_IS_NOTEBOOK(main_window->get_panel_notebook(p)))
                {
                    const i32 pages = gtk_notebook_get_n_pages(main_window->get_panel_notebook(p));
                    if (pages) // panel was shown
                    {
                        std::string tabs;
                        for (const auto i : std::views::iota(0z, pages))
                        {
                            ptk::browser* browser = PTK_FILE_BROWSER_REINTERPRET(
                                gtk_notebook_get_nth_page(main_window->get_panel_notebook(p), i));
                            tabs = std::format("{}{}{}",
                                               tabs,
                                               config::disk_format::tab_delimiter,
                                               // Need to use .string() as libfmt will by default
                                               // format paths with double quotes surrounding them
                                               // which will break config file parsing.
                                               browser->cwd().string());
                        }
                        set->s = tabs;

                        // save current tab
                        const i32 current_page =
                            gtk_notebook_get_current_page(main_window->get_panel_notebook(p));
                        set->x = std::format("{}", current_page);
                    }
                }
            }
        }
        else
        {
            // clear saved tabs
            for (const panel_t p : PANELS)
            {
                const auto set = xset::set::get(xset::panel::show, p);
                set->s = std::nullopt;
                set->x = std::nullopt;
            }
        }
    }

    /* save settings */
    const auto settings_config_dir = vfs::program::config();
    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    config::save();
}

#if 0
void
multi_input_select_region(GtkWidget* input, i32 start, i32 end) noexcept
{
    GtkTextIter iter, siter;

    if (start < 0 || !GTK_IS_TEXT_VIEW(input))
    {
        return;
    }

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));

    gtk_text_buffer_get_iter_at_offset(buf, &siter, start);

    if (end < 0)
    {
        gtk_text_buffer_get_end_iter(buf, &iter);
    }
    else
    {
        gtk_text_buffer_get_iter_at_offset(buf, &iter, end);
    }

    gtk_text_buffer_select_range(buf, &iter, &siter);
}
#endif
