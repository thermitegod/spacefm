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

#pragma once

#include <CLI/CLI.hpp>

#include "commandline/socket.hxx"

namespace commandline::socket::set
{
    void window_size(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_position(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_maximized(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_fullscreen(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_vslider_top(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_vslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_hslider(CLI::App* app, const socket_subcommand_data_t& opt);
    void window_tslider(CLI::App* app, const socket_subcommand_data_t& opt);
    void focused_panel(CLI::App* app, const socket_subcommand_data_t& opt);
    void focused_pane(CLI::App* app, const socket_subcommand_data_t& opt);
    void current_tab(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel_count(CLI::App* app, const socket_subcommand_data_t& opt);
    void new_tab(CLI::App* app, const socket_subcommand_data_t& opt);
    void devices_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void dirtree_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void toolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void sidetoolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void hidden_files_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel1_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel2_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel3_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel4_visible(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel_hslider_top(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel_hslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt);
    void panel_vslider(CLI::App* app, const socket_subcommand_data_t& opt);
    void column_width(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_by(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_ascend(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_alphanum(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_natural(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_case(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_hidden_first(CLI::App* app, const socket_subcommand_data_t& opt);
    void sort_first(CLI::App* app, const socket_subcommand_data_t& opt);
    void show_thumbnails(CLI::App* app, const socket_subcommand_data_t& opt);
    void max_thumbnail_size(CLI::App* app, const socket_subcommand_data_t& opt);
    void large_icons(CLI::App* app, const socket_subcommand_data_t& opt);
    void statusbar_text(CLI::App* app, const socket_subcommand_data_t& opt);
    void pathbar_text(CLI::App* app, const socket_subcommand_data_t& opt);
    void current_dir(CLI::App* app, const socket_subcommand_data_t& opt);
    void thumbnailer(CLI::App* app, const socket_subcommand_data_t& opt);
    void selected_files(CLI::App* app, const socket_subcommand_data_t& opt);
    void selected_filenames(CLI::App* app, const socket_subcommand_data_t& opt);
    void selected_pattern(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_text(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_primary_text(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_from_file(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_primary_from_file(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_copy_files(CLI::App* app, const socket_subcommand_data_t& opt);
    void clipboard_cut_files(CLI::App* app, const socket_subcommand_data_t& opt);
    void editor(CLI::App* app, const socket_subcommand_data_t& opt);
    void terminal(CLI::App* app, const socket_subcommand_data_t& opt);
} // namespace commandline::socket::set
