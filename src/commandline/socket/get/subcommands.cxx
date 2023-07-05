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

#include "commandline/socket/get/subcommands.hxx"

/*
 * subcommand window-size
 */

void
commandline::socket::get::window_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-size", "Get property window-size");

    const auto run_subcommand = [opt]() { opt->property = "window-size"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-position
 */

void
commandline::socket::get::window_position(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-position", "Get property window-position");

    const auto run_subcommand = [opt]() { opt->property = "window-position"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-maximized
 */

void
commandline::socket::get::window_maximized(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-maximized", "Get property window-maximized");

    const auto run_subcommand = [opt]() { opt->property = "window-maximized"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-fullscreen
 */

void
commandline::socket::get::window_fullscreen(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-fullscreen", "Get property window-fullscreen");

    const auto run_subcommand = [opt]() { opt->property = "window-fullscreen"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand screen-size
 */

void
commandline::socket::get::screen_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("screen-size", "Get property screen-size");

    const auto run_subcommand = [opt]() { opt->property = "screen-size"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-vslider-top
 */

void
commandline::socket::get::window_vslider_top(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-vslider-top", "Get property window-vslider-top");

    const auto run_subcommand = [opt]() { opt->property = "window-vslider-top"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-vslider-bottom
 */

void
commandline::socket::get::window_vslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-vslider-bottom", "Get property window-vslider-bottom");

    const auto run_subcommand = [opt]() { opt->property = "window-vslider-bottom"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-hslider
 */

void
commandline::socket::get::window_hslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-hslider", "Get property window-hslider");

    const auto run_subcommand = [opt]() { opt->property = "window-hslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-tslider
 */

void
commandline::socket::get::window_tslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-tslider", "Get property window-tslider");

    const auto run_subcommand = [opt]() { opt->property = "window-tslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand focused-panel
 */

void
commandline::socket::get::focused_panel(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("focused-panel", "Get property focused-panel");

    const auto run_subcommand = [opt]() { opt->property = "focused-panel"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand focused-pane
 */

void
commandline::socket::get::focused_pane(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("focused-pane", "Get property focused-pane");

    const auto run_subcommand = [opt]() { opt->property = "focused-pane"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand current-tab
 */

void
commandline::socket::get::current_tab(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("current-tab", "Get property current-tab");

    const auto run_subcommand = [opt]() { opt->property = "current-tab"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-count
 */

void
commandline::socket::get::panel_count(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-count", "Get property panel-count");

    const auto run_subcommand = [opt]() { opt->property = "panel-count"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand tab-count
 */

void
commandline::socket::get::tab_count(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab-count", "Get property tab-count");

    const auto run_subcommand = [opt]() { opt->property = "tab-count"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand devices-visible
 */

void
commandline::socket::get::devices_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("devices-visible", "Get property devices-visible");

    const auto run_subcommand = [opt]() { opt->property = "devices-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand dirtree-visible
 */

void
commandline::socket::get::dirtree_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("dirtree-visible", "Get property dirtree-visible");

    const auto run_subcommand = [opt]() { opt->property = "dirtree-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand toolbar-visible
 */

void
commandline::socket::get::toolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("toolbar-visible", "Get property toolbar-visible");

    const auto run_subcommand = [opt]() { opt->property = "toolbar-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sidetoolbar-visible
 */

void
commandline::socket::get::sidetoolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sidetoolbar-visible", "Get property sidetoolbar-visible");

    const auto run_subcommand = [opt]() { opt->property = "sidetoolbar-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand hidden-files-visible
 */

void
commandline::socket::get::hidden_files_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("hidden-files-visible", "Get property hidden-files-visible");

    const auto run_subcommand = [opt]() { opt->property = "hidden-files-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel1-visible
 */

void
commandline::socket::get::panel1_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel1-visible", "Get property panel1-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel1-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel2-visible
 */

void
commandline::socket::get::panel2_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel2-visible", "Get property panel2-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel2-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel3-visible
 */

void
commandline::socket::get::panel3_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel3-visible", "Get property panel3-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel3-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel4-visible
 */

void
commandline::socket::get::panel4_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel4-visible", "Get property panel4-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel4-visible"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-hslider-top
 */

void
commandline::socket::get::panel_hslider_top(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-hslider-top", "Get property panel-hslider-top");

    const auto run_subcommand = [opt]() { opt->property = "panel-hslider-top"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-hslider-bottom
 */

void
commandline::socket::get::panel_hslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-hslider-bottom", "Get property panel-hslider-bottom");

    const auto run_subcommand = [opt]() { opt->property = "panel-hslider-bottom"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-vslider
 */

void
commandline::socket::get::panel_vslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-vslider", "Get property panel-vslider");

    const auto run_subcommand = [opt]() { opt->property = "panel-vslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand column-width
 */

void
setup_subcommand_get_column_width_name(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("name", "Get property name");

    const auto run_subcommand = [opt]() { opt->subproperty = "name"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("size", "Get property size");

    const auto run_subcommand = [opt]() { opt->subproperty = "size"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_bytes(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("bytes", "Get property bytes");

    const auto run_subcommand = [opt]() { opt->subproperty = "bytes"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_type(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("type", "Get property type");

    const auto run_subcommand = [opt]() { opt->subproperty = "type"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_mime(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("mime", "Get property mime");

    const auto run_subcommand = [opt]() { opt->subproperty = "mime"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_permission(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("permission", "Get property permission");

    const auto run_subcommand = [opt]() { opt->subproperty = "permission"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_owner(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("owner", "Get property owner");

    const auto run_subcommand = [opt]() { opt->subproperty = "owner"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_group(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("group", "Get property group");

    const auto run_subcommand = [opt]() { opt->subproperty = "group"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_accessed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("accessed", "Get property accessed");

    const auto run_subcommand = [opt]() { opt->subproperty = "accessed"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_modified(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("modified", "Get property modified");

    const auto run_subcommand = [opt]() { opt->subproperty = "modified"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_get_column_width_created(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("created", "Get property created");

    const auto run_subcommand = [opt]() { opt->subproperty = "created"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::get::column_width(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("column-width", "Get property column-width");

    const auto run_subcommand = [opt]() { opt->property = "column-width"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_get_column_width_name(sub, opt);
    setup_subcommand_get_column_width_size(sub, opt);
    setup_subcommand_get_column_width_bytes(sub, opt);
    setup_subcommand_get_column_width_type(sub, opt);
    setup_subcommand_get_column_width_mime(sub, opt);
    setup_subcommand_get_column_width_permission(sub, opt);
    setup_subcommand_get_column_width_owner(sub, opt);
    setup_subcommand_get_column_width_group(sub, opt);
    setup_subcommand_get_column_width_accessed(sub, opt);
    setup_subcommand_get_column_width_modified(sub, opt);
    setup_subcommand_get_column_width_created(sub, opt);
}

/*
 * subcommand sort-by
 */

void
commandline::socket::get::sort_by(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-by", "Get property sort-by");

    const auto run_subcommand = [opt]() { opt->property = "sort-by"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-ascend
 */

void
commandline::socket::get::sort_ascend(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-ascend", "Get property sort-ascend");

    const auto run_subcommand = [opt]() { opt->property = "sort-ascend"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-alphanum
 */

void
commandline::socket::get::sort_alphanum(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-alphanum", "Get property sort-alphanum");

    const auto run_subcommand = [opt]() { opt->property = "sort-alphanum"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-natural
 */

void
commandline::socket::get::sort_natural(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-natural", "Get property sort-natural");

    const auto run_subcommand = [opt]() { opt->property = "sort-natural"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-case
 */

void
commandline::socket::get::sort_case(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-case", "Get property sort-case");

    const auto run_subcommand = [opt]() { opt->property = "sort-case"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-hidden-first
 */

void
commandline::socket::get::sort_hidden_first(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-hidden-first", "Get property sort-hidden-first");

    const auto run_subcommand = [opt]() { opt->property = "sort-hidden-first"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand sort-first
 */

void
commandline::socket::get::sort_first(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-first", "Get property sort-first");

    const auto run_subcommand = [opt]() { opt->property = "sort-first"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand show-thumbnails
 */

void
commandline::socket::get::show_thumbnails(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("show-thumbnails", "Get property show-thumbnails");

    const auto run_subcommand = [opt]() { opt->property = "show-thumbnails"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand large-icons
 */

void
commandline::socket::get::large_icons(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("large-icons", "Get property large-icons");

    const auto run_subcommand = [opt]() { opt->property = "large-icons"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand statusbar-text
 */

void
commandline::socket::get::statusbar_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("statusbar-text", "Get property statusbar-text");

    const auto run_subcommand = [opt]() { opt->property = "statusbar-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand pathbar-text
 */

void
commandline::socket::get::pathbar_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("pathbar-text", "Get property pathbar-text");

    const auto run_subcommand = [opt]() { opt->property = "pathbar-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand current-dir
 */

void
commandline::socket::get::current_dir(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("current-dir", "Get property current-dir");

    const auto run_subcommand = [opt]() { opt->property = "current-dir"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand thumbnailer
 */

void
commandline::socket::get::thumbnailer(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("thumbnailer", "Get property thumbnailer");

    const auto run_subcommand = [opt]() { opt->property = "thumbnailer"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand selected-files
 */

void
commandline::socket::get::selected_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-files", "Get property selected-files");

    const auto run_subcommand = [opt]() { opt->property = "selected-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand selected-filenames
 */

void
commandline::socket::get::selected_filenames(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-filenames", "Get property selected-filenames");

    const auto run_subcommand = [opt]() { opt->property = "selected-filenames"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand selected-pattern
 */

void
commandline::socket::get::selected_pattern(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-pattern", "Get property selected-pattern");

    const auto run_subcommand = [opt]() { opt->property = "selected-pattern"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-text
 */

void
commandline::socket::get::clipboard_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-text", "Get property clipboard-text");

    const auto run_subcommand = [opt]() { opt->property = "clipboard-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-primary-text
 */

void
commandline::socket::get::clipboard_primary_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub =
        app->add_subcommand("clipboard-primary-text", "Get property clipboard-primary-text");

    const auto run_subcommand = [opt]() { opt->property = "clipboard-primary-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-copy-files
 */

void
commandline::socket::get::clipboard_copy_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-copy-files", "Get property clipboard-copy-files");

    const auto run_subcommand = [opt]() { opt->property = "clipboard-copy-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-cut-files
 */

void
commandline::socket::get::clipboard_cut_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-cut-files", "Get property clipboard-cut-files");

    const auto run_subcommand = [opt]() { opt->property = "clipboard-cut-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand editor
 */

void
commandline::socket::get::editor(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("editor", "Get property editor");

    const auto run_subcommand = [opt]() { opt->property = "editor"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand terminal
 */

void
commandline::socket::get::terminal(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("terminal", "Get property terminal");

    const auto run_subcommand = [opt]() { opt->property = "terminal"; };
    sub->callback(run_subcommand);
}
