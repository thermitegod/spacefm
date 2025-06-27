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

#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "xset/xset-defaults.hxx"
#include "xset/xset.hxx"

#include "gui/main-window.hxx"

#include "vfs/app-desktop.hxx"
#include "vfs/execute.hxx"
#include "vfs/mime-type.hxx"
#include "vfs/terminals.hxx"
#include "vfs/user-dirs.hxx"

#include "logger.hxx"
#include "settings.hxx"
#include "types.hxx"

void
load_settings(const std::shared_ptr<config::settings>& settings) noexcept
{
    assert(settings != nullptr);

    settings->load_saved_tabs = true;

    xset_defaults();

    if (!std::filesystem::exists(vfs::program::config()))
    {
        std::filesystem::create_directories(vfs::program::config());
        if (!std::filesystem::exists(vfs::program::config()))
        {
            logger::critical("Failed to create config dir: {}", vfs::program::config().string());
            return;
        }
        std::filesystem::permissions(vfs::program::config(), std::filesystem::perms::owner_all);
    }

#if defined(DEV_MODE)
    const auto command_script = std::filesystem::path() / DEV_SCRIPTS_PATH / "config-update-git";
    if (std::filesystem::exists(command_script))
    {
        [[maybe_unused]] const auto result = vfs::execute::command_line_sync(
            "{} --config-dir {} --config-file {} --config-version {}",
            command_script.string(),
            vfs::program::config().string(),
            config::disk_format::filename,
            config::disk_format::version);
    }
#endif

    const auto session = vfs::program::config() / config::disk_format::filename;
    if (std::filesystem::is_regular_file(session))
    {
        config::load(session, settings);
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
        const auto supported_terminals = vfs::terminals::supported_names();
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
            if (desktop)
            {
                xset_set(xset::name::editor, xset::var::s, desktop->path().string());
            }
        }
    }

    // set default keys
    xset_default_keys();
}

void
save_settings() noexcept
{
    save_settings(config::global::settings);
}

void
save_settings(const std::shared_ptr<config::settings>& settings) noexcept
{
    assert(settings != nullptr);

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
                    const auto pages = gtk_notebook_get_n_pages(main_window->get_panel_notebook(p));
                    if (pages) // panel was shown
                    {
                        std::string tabs;
                        for (const auto i : std::views::iota(0, pages))
                        {
                            gui::browser* browser = PTK_FILE_BROWSER_REINTERPRET(
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

    if (!std::filesystem::exists(vfs::program::config()))
    {
        std::filesystem::create_directories(vfs::program::config());
        if (!std::filesystem::exists(vfs::program::config()))
        {
            logger::critical("Failed to create config dir: {}", vfs::program::config().string());
            return;
        }
        std::filesystem::permissions(vfs::program::config(), std::filesystem::perms::owner_all);
    }

    config::save(vfs::program::config() / config::disk_format::filename, settings);
}
