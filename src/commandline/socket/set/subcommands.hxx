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
void window_size(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_position(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_maximized(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_fullscreen(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_vslider_top(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_vslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_hslider(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void window_tslider(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void focused_panel(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void focused_pane(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void current_tab(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel_count(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void new_tab(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void devices_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void dirtree_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void toolbar_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void hidden_files_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel1_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel2_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel3_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel4_visible(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel_hslider_top(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel_hslider_bottom(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void panel_vslider(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void column_width(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_by(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_ascend(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_natural(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_case(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_hidden_first(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void sort_first(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void show_thumbnails(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void max_thumbnail_size(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void large_icons(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void pathbar_text(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void current_dir(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void thumbnailer(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void selected_files(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void selected_filenames(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void unselected_files(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void unselected_filenames(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void selected_pattern(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_text(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_primary_text(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_from_file(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_primary_from_file(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_copy_files(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void clipboard_cut_files(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void editor(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void terminal(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
} // namespace commandline::socket::set
