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

#include <CLI/CLI.hpp>

#include "commandline/socket.hxx"

#include "commandline/socket/set/subcommands.hxx"
#include "commandline/socket/get/subcommands.hxx"
#include "commandline/socket/set-task/subcommands.hxx"
#include "commandline/socket/get-task/subcommands.hxx"
#include "commandline/socket/run-task/subcommands.hxx"

#include "commandline/socket/subcommands.hxx"

/*
 * subcommand set
 */

void
setup_subcommand_set(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("set", "Sets a property");

    const auto run_subcommand = [&opt]() { opt->command = "set"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    // create subcommand entries
    commandline::socket::set::window_size(sub, opt);
    commandline::socket::set::window_position(sub, opt);
    commandline::socket::set::window_maximized(sub, opt);
    commandline::socket::set::window_fullscreen(sub, opt);
    commandline::socket::set::window_vslider_top(sub, opt);
    commandline::socket::set::window_vslider_bottom(sub, opt);
    commandline::socket::set::window_hslider(sub, opt);
    commandline::socket::set::window_tslider(sub, opt);
    commandline::socket::set::focused_panel(sub, opt);
    commandline::socket::set::focused_pane(sub, opt);
    commandline::socket::set::current_tab(sub, opt);
    commandline::socket::set::new_tab(sub, opt);
    commandline::socket::set::devices_visible(sub, opt);
    commandline::socket::set::dirtree_visible(sub, opt);
    commandline::socket::set::toolbar_visible(sub, opt);
    commandline::socket::set::sidetoolbar_visible(sub, opt);
    commandline::socket::set::hidden_files_visible(sub, opt);
    commandline::socket::set::panel1_visible(sub, opt);
    commandline::socket::set::panel2_visible(sub, opt);
    commandline::socket::set::panel3_visible(sub, opt);
    commandline::socket::set::panel4_visible(sub, opt);
    commandline::socket::set::panel_hslider_top(sub, opt);
    commandline::socket::set::panel_hslider_bottom(sub, opt);
    commandline::socket::set::panel_vslider(sub, opt);
    commandline::socket::set::column_width(sub, opt);
    commandline::socket::set::sort_by(sub, opt);
    commandline::socket::set::sort_ascend(sub, opt);
    commandline::socket::set::sort_natural(sub, opt);
    commandline::socket::set::sort_case(sub, opt);
    commandline::socket::set::sort_hidden_first(sub, opt);
    commandline::socket::set::sort_first(sub, opt);
    commandline::socket::set::show_thumbnails(sub, opt);
    commandline::socket::set::max_thumbnail_size(sub, opt);
    commandline::socket::set::large_icons(sub, opt);
    commandline::socket::set::pathbar_text(sub, opt);
    commandline::socket::set::current_dir(sub, opt);
    commandline::socket::set::thumbnailer(sub, opt);
    commandline::socket::set::selected_files(sub, opt);
    commandline::socket::set::selected_filenames(sub, opt);
    commandline::socket::set::unselected_files(sub, opt);
    commandline::socket::set::unselected_filenames(sub, opt);
    commandline::socket::set::selected_pattern(sub, opt);
    commandline::socket::set::clipboard_text(sub, opt);
    commandline::socket::set::clipboard_primary_text(sub, opt);
    commandline::socket::set::clipboard_from_file(sub, opt);
    commandline::socket::set::clipboard_primary_from_file(sub, opt);
    commandline::socket::set::clipboard_copy_files(sub, opt);
    commandline::socket::set::clipboard_cut_files(sub, opt);
    commandline::socket::set::editor(sub, opt);
    commandline::socket::set::terminal(sub, opt);
}

/*
 * subcommand get
 */

void
setup_subcommand_get(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("get", "Gets a property");

    const auto run_subcommand = [&opt]() { opt->command = "get"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    // create subcommand entries
    commandline::socket::get::window_size(sub, opt);
    commandline::socket::get::window_position(sub, opt);
    commandline::socket::get::window_maximized(sub, opt);
    commandline::socket::get::window_fullscreen(sub, opt);
    commandline::socket::get::screen_size(sub, opt);
    commandline::socket::get::window_vslider_top(sub, opt);
    commandline::socket::get::window_vslider_bottom(sub, opt);
    commandline::socket::get::window_hslider(sub, opt);
    commandline::socket::get::window_tslider(sub, opt);
    commandline::socket::get::focused_panel(sub, opt);
    commandline::socket::get::focused_pane(sub, opt);
    commandline::socket::get::current_tab(sub, opt);
    commandline::socket::get::panel_count(sub, opt);
    commandline::socket::get::tab_count(sub, opt);
    commandline::socket::get::devices_visible(sub, opt);
    commandline::socket::get::dirtree_visible(sub, opt);
    commandline::socket::get::toolbar_visible(sub, opt);
    commandline::socket::get::sidetoolbar_visible(sub, opt);
    commandline::socket::get::hidden_files_visible(sub, opt);
    commandline::socket::get::panel1_visible(sub, opt);
    commandline::socket::get::panel2_visible(sub, opt);
    commandline::socket::get::panel3_visible(sub, opt);
    commandline::socket::get::panel4_visible(sub, opt);
    commandline::socket::get::panel_hslider_top(sub, opt);
    commandline::socket::get::panel_hslider_bottom(sub, opt);
    commandline::socket::get::panel_vslider(sub, opt);
    commandline::socket::get::column_width(sub, opt);
    commandline::socket::get::sort_by(sub, opt);
    commandline::socket::get::sort_ascend(sub, opt);
    commandline::socket::get::sort_natural(sub, opt);
    commandline::socket::get::sort_case(sub, opt);
    commandline::socket::get::sort_hidden_first(sub, opt);
    commandline::socket::get::sort_first(sub, opt);
    commandline::socket::get::show_thumbnails(sub, opt);
    commandline::socket::get::max_thumbnail_size(sub, opt);
    commandline::socket::get::large_icons(sub, opt);
    commandline::socket::get::statusbar_text(sub, opt);
    commandline::socket::get::pathbar_text(sub, opt);
    commandline::socket::get::current_dir(sub, opt);
    commandline::socket::get::thumbnailer(sub, opt);
    commandline::socket::get::selected_files(sub, opt);
    commandline::socket::get::selected_filenames(sub, opt);
    commandline::socket::get::selected_pattern(sub, opt);
    commandline::socket::get::clipboard_text(sub, opt);
    commandline::socket::get::clipboard_primary_text(sub, opt);
    commandline::socket::get::clipboard_copy_files(sub, opt);
    commandline::socket::get::clipboard_cut_files(sub, opt);
    commandline::socket::get::editor(sub, opt);
    commandline::socket::get::terminal(sub, opt);
}

/*
 * subcommand set-task
 */

void
setup_subcommand_set_task(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("set-task", "Sets a task property");

    const auto run_subcommand = [&opt]() { opt->command = "set-task"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    // create subcommand entries
    commandline::socket::set_task::icon(sub, opt);
    commandline::socket::set_task::count(sub, opt);
    commandline::socket::set_task::directory(sub, opt);
    commandline::socket::set_task::from(sub, opt);
    commandline::socket::set_task::item(sub, opt);
    commandline::socket::set_task::to(sub, opt);
    commandline::socket::set_task::progress(sub, opt);
    commandline::socket::set_task::total(sub, opt);
    commandline::socket::set_task::curspeed(sub, opt);
    commandline::socket::set_task::curremain(sub, opt);
    commandline::socket::set_task::avgspeed(sub, opt);
    commandline::socket::set_task::avgremain(sub, opt);
    commandline::socket::set_task::queue_state(sub, opt);
    commandline::socket::set_task::popup_handler(sub, opt);
}

/*
 * subcommand get-task
 */

void
setup_subcommand_get_task(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("get-task", "Gets a task property");

    sub->add_option("property", opt->socket_data, "Property to get")->required(true)->expected(2);

    const auto run_subcommand = [&opt]() { opt->command = "get-task"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    // create subcommand entries
    commandline::socket::get_task::icon(sub, opt);
    commandline::socket::get_task::count(sub, opt);
    commandline::socket::get_task::directory(sub, opt);
    commandline::socket::get_task::from(sub, opt);
    commandline::socket::get_task::item(sub, opt);
    commandline::socket::get_task::to(sub, opt);
    commandline::socket::get_task::progress(sub, opt);
    commandline::socket::get_task::total(sub, opt);
    commandline::socket::get_task::curspeed(sub, opt);
    commandline::socket::get_task::curremain(sub, opt);
    commandline::socket::get_task::avgspeed(sub, opt);
    commandline::socket::get_task::avgremain(sub, opt);
    commandline::socket::get_task::elapsed(sub, opt);
    commandline::socket::get_task::started(sub, opt);
    commandline::socket::get_task::status(sub, opt);
    commandline::socket::get_task::queue_state(sub, opt);
    commandline::socket::get_task::popup_handler(sub, opt);
}

/*
 * subcommand run-task
 */

void
setup_subcommand_run_task(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("run-task", "Starts a new task");

    const auto run_subcommand = [&opt]() { opt->command = "run-task"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    // create subcommand entries
    commandline::socket::run_task::cmd(sub, opt);
    commandline::socket::run_task::edit(sub, opt);
    commandline::socket::run_task::mount(sub, opt);
    commandline::socket::run_task::umount(sub, opt);
    commandline::socket::run_task::copy(sub, opt);
    commandline::socket::run_task::move(sub, opt);
    commandline::socket::run_task::link(sub, opt);
    commandline::socket::run_task::del(sub, opt);
    commandline::socket::run_task::trash(sub, opt);
}

/*
 * subcommand emit-key
 */

void
setup_subcommand_emit_key(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub =
        app->add_subcommand("emit-key", "Activates a menu item by emitting its shortcut key");

    sub->add_option("property", opt->socket_data, "Property to get")
        ->required(true)
        ->expected(1, 2);

    const auto run_subcommand = [&opt]() { opt->command = "emit-key"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand activate
 */

void
setup_subcommand_activate(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("activate", "Runs custom command or shows submenu named NAME");

    sub->add_option("property", opt->socket_data, "Property to get")->required(true)->expected(1);

    const auto run_subcommand = [&opt]() { opt->command = "activate"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand add-event
 */

void
setup_subcommand_add_event(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("add-event", "Add asynchronous handler COMMAND to EVENT");

    sub->add_option("property", opt->socket_data, "Property to get")
        ->required(true)
        ->expected(2, -1);

    const auto run_subcommand = [&opt]() { opt->command = "add-event"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand replace-event
 */

void
setup_subcommand_replace_event(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub =
        app->add_subcommand("replace-event",
                            "Add synchronous handler COMMAND to EVENT, replacing default handler");

    sub->add_option("property", opt->socket_data, "Property to get")
        ->required(true)
        ->expected(2, -1);

    const auto run_subcommand = [&opt]() { opt->command = "replace-event"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand remove-event
 */

void
setup_subcommand_remove_event(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("remove-event", "Remove handler COMMAND from EVENT");

    sub->add_option("property", opt->socket_data, "Property to get")
        ->required(true)
        ->expected(2, -1);

    const auto run_subcommand = [&opt]() { opt->command = "remove-event"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand help
 */

void
setup_subcommand_help(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("help", "Show socket help");

    const auto run_subcommand = [&opt]() { opt->command = "help"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand ping
 */

void
setup_subcommand_ping(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("ping", "Test socket read/write");

    const auto run_subcommand = [&opt]() { opt->command = "ping"; };
    sub->callback(run_subcommand);
}
