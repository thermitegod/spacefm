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

/*
 * Bool subcommand values
 */

void
setup_subcommand_value_true(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("true", "Set value true");

    const auto run_subcommand = [opt]() { opt->subproperty = "true"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_value_false(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("false", "Set value false");

    const auto run_subcommand = [opt]() { opt->subproperty = "false"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-size
 */

void
commandline::socket::set::window_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-size", "Set property window-size");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-size"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-position
 */

void
commandline::socket::set::window_position(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-position", "Set property window-position");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-position"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-maximized
 */

void
commandline::socket::set::window_maximized(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-maximized", "Set property window-maximized");

    const auto run_subcommand = [opt]() { opt->property = "window-maximized"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand window-fullscreen
 */

void
commandline::socket::set::window_fullscreen(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-fullscreen", "Set property window-fullscreen");

    const auto run_subcommand = [opt]() { opt->property = "window-fullscreen"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand window-vslider-top
 */

void
commandline::socket::set::window_vslider_top(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-vslider-top", "Set property window-vslider-top");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-vslider-top"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-vslider-bottom
 */

void
commandline::socket::set::window_vslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-vslider-bottom", "Set property window-vslider-bottom");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-vslider-bottom"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-hslider
 */

void
commandline::socket::set::window_hslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-hslider", "Set property window-hslider");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-hslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand window-tslider
 */

void
commandline::socket::set::window_tslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("window-tslider", "Set property window-tslider");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "window-tslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand focused-panel
 */

void
setup_subcommand_focused_panel_prev(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("prev", "switch focus to the prev panel");

    const auto run_subcommand = [opt]() { opt->subproperty = "prev"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_next(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("next", "switch focus to the next panel");

    const auto run_subcommand = [opt]() { opt->subproperty = "next"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_hide(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("hide", "hide the focused panel");

    const auto run_subcommand = [opt]() { opt->subproperty = "hide"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_panel1(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel1", "focus panel 1");

    const auto run_subcommand = [opt]() { opt->subproperty = "panel1"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_panel2(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel2", "focus panel 2");

    const auto run_subcommand = [opt]() { opt->subproperty = "panel2"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_panel3(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel3", "focus panel 3");

    const auto run_subcommand = [opt]() { opt->subproperty = "panel3"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_panel_panel4(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel4", "focus panel 4");

    const auto run_subcommand = [opt]() { opt->subproperty = "panel4"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::focused_panel(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("focused-panel", "Set property focused-panel");

    const auto run_subcommand = [opt]() { opt->property = "focused-panel"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_focused_panel_prev(sub, opt);
    setup_subcommand_focused_panel_next(sub, opt);
    setup_subcommand_focused_panel_hide(sub, opt);
    setup_subcommand_focused_panel_panel1(sub, opt);
    setup_subcommand_focused_panel_panel2(sub, opt);
    setup_subcommand_focused_panel_panel3(sub, opt);
    setup_subcommand_focused_panel_panel4(sub, opt);
}

/*
 * subcommand focused-pane
 */

void
setup_subcommand_focused_pane_filelist(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("filelist", "focus filelist");

    const auto run_subcommand = [opt]() { opt->subproperty = "filelist"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_pane_devices(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("devices", "focus devices");

    const auto run_subcommand = [opt]() { opt->subproperty = "devices"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_pane_dirtree(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("dirtree", "focus dirtree");

    const auto run_subcommand = [opt]() { opt->subproperty = "dirtree"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_focused_pane_pathbar(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("pathbar", "focus pathbar");

    const auto run_subcommand = [opt]() { opt->subproperty = "pathbar"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::focused_pane(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("focused-pane", "Set property focused-pane");

    const auto run_subcommand = [opt]() { opt->property = "focused-pane"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_focused_pane_filelist(sub, opt);
    setup_subcommand_focused_pane_devices(sub, opt);
    setup_subcommand_focused_pane_dirtree(sub, opt);
    setup_subcommand_focused_pane_pathbar(sub, opt);
}

/*
 * subcommand current-tab
 */

void
setup_subcommand_current_tab_prev(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("prev", "Goto prev tab");

    const auto run_subcommand = [opt]() { opt->subproperty = "prev"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_next(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("next", "Goto next tab");

    const auto run_subcommand = [opt]() { opt->subproperty = "next"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_close(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("close", "Close current tab");

    const auto run_subcommand = [opt]() { opt->subproperty = "close"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_restore(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("restore", "Restore last closed tab");

    const auto run_subcommand = [opt]() { opt->subproperty = "restore"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab1(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab1", "Open tab 1");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab1"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab2(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab2", "Open tab 2");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab2"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab3(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab3", "Open tab 3");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab3"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab4(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab4", "Open tab 4");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab4"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab5(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab5", "Open tab 5");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab5"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab6(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab6", "Open tab 6");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab6"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab7(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab7", "Open tab 7");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab7"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab8(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab8", "Open tab 8");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab8"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab9(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab9", "Open tab 9");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab9"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_current_tab_tab10(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("tab10", "Open tab 10");

    const auto run_subcommand = [opt]() { opt->subproperty = "tab10"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::current_tab(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("current-tab", "Set property current-tab");

    const auto run_subcommand = [opt]() { opt->property = "current-tab"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_current_tab_prev(sub, opt);
    setup_subcommand_current_tab_next(sub, opt);
    setup_subcommand_current_tab_close(sub, opt);
    setup_subcommand_current_tab_restore(sub, opt);
    setup_subcommand_current_tab_tab1(sub, opt);
    setup_subcommand_current_tab_tab2(sub, opt);
    setup_subcommand_current_tab_tab3(sub, opt);
    setup_subcommand_current_tab_tab4(sub, opt);
    setup_subcommand_current_tab_tab5(sub, opt);
    setup_subcommand_current_tab_tab6(sub, opt);
    setup_subcommand_current_tab_tab7(sub, opt);
    setup_subcommand_current_tab_tab8(sub, opt);
    setup_subcommand_current_tab_tab9(sub, opt);
    setup_subcommand_current_tab_tab10(sub, opt);
}

/*
 * subcommand new-tab
 */

void
commandline::socket::set::new_tab(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("new-tab", "Set property new-tab");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "new-tab"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand devices-visible
 */

void
commandline::socket::set::devices_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("devices-visible", "Set property devices-visible");

    const auto run_subcommand = [opt]() { opt->property = "devices-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand dirtree-visible
 */

void
commandline::socket::set::dirtree_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("dirtree-visible", "Set property dirtree-visible");

    const auto run_subcommand = [opt]() { opt->property = "dirtree-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand toolbar-visible
 */

void
commandline::socket::set::toolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("toolbar-visible", "Set property toolbar-visible");

    const auto run_subcommand = [opt]() { opt->property = "toolbar-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sidetoolbar-visible
 */

void
commandline::socket::set::sidetoolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sidetoolbar-visible", "Set property sidetoolbar-visible");

    const auto run_subcommand = [opt]() { opt->property = "sidetoolbar-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand hidden-files-visible
 */

void
commandline::socket::set::hidden_files_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("hidden-files-visible", "Set property hidden-files-visible");

    const auto run_subcommand = [opt]() { opt->property = "hidden-files-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand panel1-visible
 */

void
commandline::socket::set::panel1_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel1-visible", "Set property panel1-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel1-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand panel2-visible
 */

void
commandline::socket::set::panel2_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel2-visible", "Set property panel2-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel2-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand panel3-visible
 */

void
commandline::socket::set::panel3_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel3-visible", "Set property panel3-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel3-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand panel4-visible
 */

void
commandline::socket::set::panel4_visible(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel4-visible", "Set property panel4-visible");

    const auto run_subcommand = [opt]() { opt->property = "panel4-visible"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand panel-hslider-top
 */

void
commandline::socket::set::panel_hslider_top(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-hslider-top", "Set property panel-hslider-top");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "panel-hslider-top"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-hslider-bottom
 */

void
commandline::socket::set::panel_hslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-hslider-bottom", "Set property panel-hslider-bottom");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "panel-hslider-bottom"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand panel-vslider
 */

void
commandline::socket::set::panel_vslider(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("panel-vslider", "Set property panel-vslider");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "panel-vslider"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand column-width
 */

void
setup_subcommand_set_column_width_name(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("name", "Set property name");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "name"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("size", "Set property size");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "size"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_bytes(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("bytes", "Set property bytes");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "bytes"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_type(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("type", "Set property type");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "type"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_mime(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("mime", "Set property mime");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "mime"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_permission(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("permission", "Set property permission");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "permission"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_owner(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("owner", "Set property owner");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "owner"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_group(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("group", "Set property group");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "group"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_date_accessed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("accessed", "Set property accessed");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "accessed"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_date_created(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("created", "Set property created");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "created"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_date_metadata(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("metadata", "Set property metadata");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "metadata"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_column_width_date_modified(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("modified", "Set property modified");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->subproperty = "modified"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::column_width(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("column-width", "Set property column-width");

    const auto run_subcommand = [opt]() { opt->property = "column-width"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_set_column_width_name(sub, opt);
    setup_subcommand_set_column_width_size(sub, opt);
    setup_subcommand_set_column_width_bytes(sub, opt);
    setup_subcommand_set_column_width_type(sub, opt);
    setup_subcommand_set_column_width_mime(sub, opt);
    setup_subcommand_set_column_width_permission(sub, opt);
    setup_subcommand_set_column_width_owner(sub, opt);
    setup_subcommand_set_column_width_group(sub, opt);
    setup_subcommand_set_column_width_date_accessed(sub, opt);
    setup_subcommand_set_column_width_date_created(sub, opt);
    setup_subcommand_set_column_width_date_metadata(sub, opt);
    setup_subcommand_set_column_width_date_modified(sub, opt);
}

/*
 * subcommand sort-by
 */

void
setup_subcommand_set_sort_by_name(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("name", "Set property name");

    const auto run_subcommand = [opt]() { opt->subproperty = "name"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("size", "Set property size");

    const auto run_subcommand = [opt]() { opt->subproperty = "size"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_bytes(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("bytes", "Set property bytes");

    const auto run_subcommand = [opt]() { opt->subproperty = "bytes"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_type(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("type", "Set property type");

    const auto run_subcommand = [opt]() { opt->subproperty = "type"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_mime(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("mime", "Set property mime");

    const auto run_subcommand = [opt]() { opt->subproperty = "mime"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_permission(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("permission", "Set property permission");

    const auto run_subcommand = [opt]() { opt->subproperty = "permission"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_owner(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("owner", "Set property owner");

    const auto run_subcommand = [opt]() { opt->subproperty = "owner"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_group(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("group", "Set property group");

    const auto run_subcommand = [opt]() { opt->subproperty = "group"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_accessed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("accessed", "Set property accessed");

    const auto run_subcommand = [opt]() { opt->subproperty = "accessed"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_modified(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("modified", "Set property modified");

    const auto run_subcommand = [opt]() { opt->subproperty = "modified"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_by_created(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("created", "Set property created");

    const auto run_subcommand = [opt]() { opt->subproperty = "accessed"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::sort_by(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-by", "Set property sort-by");

    const auto run_subcommand = [opt]() { opt->property = "sort-by"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_set_sort_by_name(sub, opt);
    setup_subcommand_set_sort_by_size(sub, opt);
    setup_subcommand_set_sort_by_bytes(sub, opt);
    setup_subcommand_set_sort_by_type(sub, opt);
    setup_subcommand_set_sort_by_mime(sub, opt);
    setup_subcommand_set_sort_by_permission(sub, opt);
    setup_subcommand_set_sort_by_owner(sub, opt);
    setup_subcommand_set_sort_by_group(sub, opt);
    setup_subcommand_set_sort_by_accessed(sub, opt);
    setup_subcommand_set_sort_by_modified(sub, opt);
    setup_subcommand_set_sort_by_created(sub, opt);
}

/*
 * subcommand sort-ascend
 */

void
commandline::socket::set::sort_ascend(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-ascend", "Set property sort-ascend");

    const auto run_subcommand = [opt]() { opt->property = "sort-ascend"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sort-alphanum
 */

void
commandline::socket::set::sort_alphanum(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-alphanum", "Set property sort-alphanum");

    const auto run_subcommand = [opt]() { opt->property = "sort-alphanum"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sort-natural
 */

void
commandline::socket::set::sort_natural(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-natural", "Set property sort-natural");

    const auto run_subcommand = [opt]() { opt->property = "sort-natural"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sort-case
 */

void
commandline::socket::set::sort_case(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-case", "Set property sort-case");

    const auto run_subcommand = [opt]() { opt->property = "sort-case"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sort-hidden-first
 */

void
commandline::socket::set::sort_hidden_first(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-hidden-first", "Set property sort-hidden-first");

    const auto run_subcommand = [opt]() { opt->property = "sort-hidden-first"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand sort-first
 */

void
setup_subcommand_set_sort_first_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("files", "Set property files");

    const auto run_subcommand = [opt]() { opt->subproperty = "files"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_first_directories(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("directories", "Set property directories");

    const auto run_subcommand = [opt]() { opt->subproperty = "directories"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_set_sort_first_mixed(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("mixed", "Set property mixed");

    const auto run_subcommand = [opt]() { opt->subproperty = "mixed"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::sort_first(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("sort-first", "Set property sort-first");

    const auto run_subcommand = [opt]() { opt->property = "sort-first"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_set_sort_first_files(sub, opt);
    setup_subcommand_set_sort_first_directories(sub, opt);
    setup_subcommand_set_sort_first_mixed(sub, opt);
}

/*
 * subcommand show-thumbnails
 */

void
commandline::socket::set::show_thumbnails(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("show-thumbnails", "Set property show-thumbnails");

    const auto run_subcommand = [opt]() { opt->property = "show-thumbnails"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand max-thumbnail-size
 */

void
commandline::socket::set::max_thumbnail_size(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("max-thumbnail-size", "Set property max-thumbnail-size");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "max-thumbnail-size"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand large-icons
 */

void
commandline::socket::set::large_icons(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("large-icons", "Set property large-icons");

    const auto run_subcommand = [opt]() { opt->property = "large-icons"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_value_true(sub, opt);
    setup_subcommand_value_false(sub, opt);
}

/*
 * subcommand pathbar-text
 */

void
commandline::socket::set::pathbar_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("pathbar-text", "Set property pathbar-text");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "pathbar-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand current-dir
 */

void
commandline::socket::set::current_dir(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("current-dir", "Set property current-dir");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "current-dir"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand thumbnailer
 */

void
setup_subcommand_thumbnailer_api(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("api", "Set thumbnailer api");

    const auto run_subcommand = [opt]() { opt->subproperty = "api"; };
    sub->callback(run_subcommand);
}

void
setup_subcommand_thumbnailer_cli(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("cli", "Set thumbnailer cli");

    const auto run_subcommand = [opt]() { opt->subproperty = "cli"; };
    sub->callback(run_subcommand);
}

void
commandline::socket::set::thumbnailer(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("thumbnailer", "Get property thumbnailer");

    const auto run_subcommand = [opt]() { opt->property = "thumbnailer"; };
    sub->callback(run_subcommand);

    sub->require_subcommand();

    setup_subcommand_thumbnailer_api(sub, opt);
    setup_subcommand_thumbnailer_cli(sub, opt);
}

/*
 * subcommand selected-files
 */

void
commandline::socket::set::selected_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-files", "Set property selected-files");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1, -1);

    const auto run_subcommand = [opt]() { opt->property = "selected-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand selected-filenames
 */

void
commandline::socket::set::selected_filenames(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-filenames", "Set property selected-filenames");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1, -1);

    const auto run_subcommand = [opt]() { opt->property = "selected-filenames"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand unselected-files
 */

void
commandline::socket::set::unselected_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("unselected-files", "Set property unselected-files");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1, -1);

    const auto run_subcommand = [opt]() { opt->property = "unselected-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand unselected-filenames
 */

void
commandline::socket::set::unselected_filenames(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("unselected-filenames", "Set property unselected-filenames");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1, -1);

    const auto run_subcommand = [opt]() { opt->property = "unselected-filenames"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand selected-pattern
 */

void
commandline::socket::set::selected_pattern(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("selected-pattern", "Set property selected-pattern");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "selected-pattern"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-text
 */

void
commandline::socket::set::clipboard_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-text", "Set property clipboard-text");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-primary-text
 */

void
commandline::socket::set::clipboard_primary_text(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub =
        app->add_subcommand("clipboard-primary-text", "Set property clipboard-primary-text");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-primary-text"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-from-file
 */

void
commandline::socket::set::clipboard_from_file(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-from-file", "Set property clipboard-from-file");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-from-file"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-primary-from-file
 */

void
commandline::socket::set::clipboard_primary_from_file(CLI::App* app,
                                                      const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-primary-from-file",
                                    "Set property clipboard-primary-from-file");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-primary-from-file"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-copy-files
 */

void
commandline::socket::set::clipboard_copy_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-copy-files", "Set property clipboard-copy-files");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-copy-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand clipboard-cut-files
 */

void
commandline::socket::set::clipboard_cut_files(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("clipboard-cut-files", "Set property clipboard-cut-files");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "clipboard-cut-files"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand editor
 */

void
commandline::socket::set::editor(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("editor", "Set property editor");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "editor"; };
    sub->callback(run_subcommand);
}

/*
 * subcommand terminal
 */

void
commandline::socket::set::terminal(CLI::App* app, const socket_subcommand_data_t& opt)
{
    auto* sub = app->add_subcommand("terminal", "Set property terminal");

    sub->add_option("value", opt->socket_data, "Value to set")->required(true)->expected(1);

    const auto run_subcommand = [opt]() { opt->property = "terminal"; };
    sub->callback(run_subcommand);
}
