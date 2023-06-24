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

#include <unordered_map>

#include <cassert>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "xset/xset-lookup.hxx"

using namespace std::literals::string_view_literals;

// clang-format off

// panels

// panel1
static const std::unordered_map<xset::panel, xset::name> xset_panel1_map{
    // panel1
    {xset::panel::show,             xset::name::panel1_show},
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed},
    {xset::panel::list_icons,       xset::name::panel1_list_icons},
    {xset::panel::list_compact,     xset::name::panel1_list_compact},
    {xset::panel::list_large,       xset::name::panel1_list_large},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab},
    {xset::panel::icon_status,      xset::name::panel1_icon_status},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra},
    {xset::panel::book_fol,         xset::name::panel1_book_fol},
    {xset::panel::tool_l,           xset::name::panel1_tool_l},
    {xset::panel::tool_r,           xset::name::panel1_tool_r},
    {xset::panel::tool_s,           xset::name::panel1_tool_s},
};

// panel2
static const std::unordered_map<xset::panel, xset::name> xset_panel2_map{
    // panel2
    {xset::panel::show,             xset::name::panel2_show},
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed},
    {xset::panel::list_icons,       xset::name::panel2_list_icons},
    {xset::panel::list_compact,     xset::name::panel2_list_compact},
    {xset::panel::list_large,       xset::name::panel2_list_large},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab},
    {xset::panel::icon_status,      xset::name::panel2_icon_status},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra},
    {xset::panel::book_fol,         xset::name::panel2_book_fol},
    {xset::panel::tool_l,           xset::name::panel2_tool_l},
    {xset::panel::tool_r,           xset::name::panel2_tool_r},
    {xset::panel::tool_s,           xset::name::panel2_tool_s},
};

// panel3
static const std::unordered_map<xset::panel, xset::name> xset_panel3_map{
    // panel3
    {xset::panel::show,             xset::name::panel3_show},
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed},
    {xset::panel::list_icons,       xset::name::panel3_list_icons},
    {xset::panel::list_compact,     xset::name::panel3_list_compact},
    {xset::panel::list_large,       xset::name::panel3_list_large},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab},
    {xset::panel::icon_status,      xset::name::panel3_icon_status},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra},
    {xset::panel::book_fol,         xset::name::panel3_book_fol},
    {xset::panel::tool_l,           xset::name::panel3_tool_l},
    {xset::panel::tool_r,           xset::name::panel3_tool_r},
    {xset::panel::tool_s,           xset::name::panel3_tool_s},
};

// panel4
static const std::unordered_map<xset::panel, xset::name> xset_panel4_map{
    // panel4
    {xset::panel::show,             xset::name::panel4_show},
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed},
    {xset::panel::list_icons,       xset::name::panel4_list_icons},
    {xset::panel::list_compact,     xset::name::panel4_list_compact},
    {xset::panel::list_large,       xset::name::panel4_list_large},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab},
    {xset::panel::icon_status,      xset::name::panel4_icon_status},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra},
    {xset::panel::book_fol,         xset::name::panel4_book_fol},
    {xset::panel::tool_l,           xset::name::panel4_tool_l},
    {xset::panel::tool_r,           xset::name::panel4_tool_r},
    {xset::panel::tool_s,           xset::name::panel4_tool_s},
};

// panel modes

// panel1

// panel1 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox0},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon0},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree0},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar0},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions0},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed0},
    {xset::panel::list_icons,       xset::name::panel1_list_icons0},
    {xset::panel::list_compact,     xset::name::panel1_list_compact0},
    {xset::panel::list_large,       xset::name::panel1_list_large0},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden0},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab0},
    {xset::panel::icon_status,      xset::name::panel1_icon_status0},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name0},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size0},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type0},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm0},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner0},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date0},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra0},
    {xset::panel::book_fol,         xset::name::panel1_book_fol0},
    {xset::panel::tool_l,           xset::name::panel1_tool_l0},
    {xset::panel::tool_r,           xset::name::panel1_tool_r0},
    {xset::panel::tool_s,           xset::name::panel1_tool_s0},
};

// panel1 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox1},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon1},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree1},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar1},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions1},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed1},
    {xset::panel::list_icons,       xset::name::panel1_list_icons1},
    {xset::panel::list_compact,     xset::name::panel1_list_compact1},
    {xset::panel::list_large,       xset::name::panel1_list_large1},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden1},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab1},
    {xset::panel::icon_status,      xset::name::panel1_icon_status1},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name1},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size1},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type1},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm1},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner1},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date1},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra1},
    {xset::panel::book_fol,         xset::name::panel1_book_fol1},
    {xset::panel::tool_l,           xset::name::panel1_tool_l1},
    {xset::panel::tool_r,           xset::name::panel1_tool_r1},
    {xset::panel::tool_s,           xset::name::panel1_tool_s1},
};

// panel1 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox2},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon2},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree2},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar2},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions2},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed2},
    {xset::panel::list_icons,       xset::name::panel1_list_icons2},
    {xset::panel::list_compact,     xset::name::panel1_list_compact2},
    {xset::panel::list_large,       xset::name::panel1_list_large2},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden2},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab2},
    {xset::panel::icon_status,      xset::name::panel1_icon_status2},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name2},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size2},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type2},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm2},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner2},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date2},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra2},
    {xset::panel::book_fol,         xset::name::panel1_book_fol2},
    {xset::panel::tool_l,           xset::name::panel1_tool_l2},
    {xset::panel::tool_r,           xset::name::panel1_tool_r2},
    {xset::panel::tool_s,           xset::name::panel1_tool_s2},
};

// panel1 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox3},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon3},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree3},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar3},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions3},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed3},
    {xset::panel::list_icons,       xset::name::panel1_list_icons3},
    {xset::panel::list_compact,     xset::name::panel1_list_compact3},
    {xset::panel::list_large,       xset::name::panel1_list_large3},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden3},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab3},
    {xset::panel::icon_status,      xset::name::panel1_icon_status3},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name3},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size3},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type3},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm3},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner3},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date3},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra3},
    {xset::panel::book_fol,         xset::name::panel1_book_fol3},
    {xset::panel::tool_l,           xset::name::panel1_tool_l3},
    {xset::panel::tool_r,           xset::name::panel1_tool_r3},
    {xset::panel::tool_s,           xset::name::panel1_tool_s3},
};

// panel2

// panel2 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox0},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon0},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree0},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar0},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions0},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed0},
    {xset::panel::list_icons,       xset::name::panel2_list_icons0},
    {xset::panel::list_compact,     xset::name::panel2_list_compact0},
    {xset::panel::list_large,       xset::name::panel2_list_large0},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden0},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab0},
    {xset::panel::icon_status,      xset::name::panel2_icon_status0},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name0},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size0},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type0},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm0},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner0},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date0},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra0},
    {xset::panel::book_fol,         xset::name::panel2_book_fol0},
    {xset::panel::tool_l,           xset::name::panel2_tool_l0},
    {xset::panel::tool_r,           xset::name::panel2_tool_r0},
    {xset::panel::tool_s,           xset::name::panel2_tool_s0},
};

// panel2 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox1},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon1},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree1},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar1},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions1},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed1},
    {xset::panel::list_icons,       xset::name::panel2_list_icons1},
    {xset::panel::list_compact,     xset::name::panel2_list_compact1},
    {xset::panel::list_large,       xset::name::panel2_list_large1},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden1},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab1},
    {xset::panel::icon_status,      xset::name::panel2_icon_status1},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name1},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size1},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type1},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm1},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner1},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date1},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra1},
    {xset::panel::book_fol,         xset::name::panel2_book_fol1},
    {xset::panel::tool_l,           xset::name::panel2_tool_l1},
    {xset::panel::tool_r,           xset::name::panel2_tool_r1},
    {xset::panel::tool_s,           xset::name::panel2_tool_s1},
};

// panel2 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox2},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon2},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree2},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar2},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions2},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed2},
    {xset::panel::list_icons,       xset::name::panel2_list_icons2},
    {xset::panel::list_compact,     xset::name::panel2_list_compact2},
    {xset::panel::list_large,       xset::name::panel2_list_large2},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden2},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab2},
    {xset::panel::icon_status,      xset::name::panel2_icon_status2},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name2},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size2},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type2},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm2},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner2},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date2},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra2},
    {xset::panel::book_fol,         xset::name::panel2_book_fol2},
    {xset::panel::tool_l,           xset::name::panel2_tool_l2},
    {xset::panel::tool_r,           xset::name::panel2_tool_r2},
    {xset::panel::tool_s,           xset::name::panel2_tool_s2},
};

// panel2 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox3},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon3},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree3},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar3},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions3},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed3},
    {xset::panel::list_icons,       xset::name::panel2_list_icons3},
    {xset::panel::list_compact,     xset::name::panel2_list_compact3},
    {xset::panel::list_large,       xset::name::panel2_list_large3},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden3},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab3},
    {xset::panel::icon_status,      xset::name::panel2_icon_status3},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name3},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size3},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type3},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm3},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner3},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date3},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra3},
    {xset::panel::book_fol,         xset::name::panel2_book_fol3},
    {xset::panel::tool_l,           xset::name::panel2_tool_l3},
    {xset::panel::tool_r,           xset::name::panel2_tool_r3},
    {xset::panel::tool_s,           xset::name::panel2_tool_s3},
};

// panel3

// panel3 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox0},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon0},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree0},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar0},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions0},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed0},
    {xset::panel::list_icons,       xset::name::panel3_list_icons0},
    {xset::panel::list_compact,     xset::name::panel3_list_compact0},
    {xset::panel::list_large,       xset::name::panel3_list_large0},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden0},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab0},
    {xset::panel::icon_status,      xset::name::panel3_icon_status0},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name0},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size0},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type0},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm0},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner0},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date0},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra0},
    {xset::panel::book_fol,         xset::name::panel3_book_fol0},
    {xset::panel::tool_l,           xset::name::panel3_tool_l0},
    {xset::panel::tool_r,           xset::name::panel3_tool_r0},
    {xset::panel::tool_s,           xset::name::panel3_tool_s0},
};

// panel3 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox1},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon1},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree1},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar1},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions1},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed1},
    {xset::panel::list_icons,       xset::name::panel3_list_icons1},
    {xset::panel::list_compact,     xset::name::panel3_list_compact1},
    {xset::panel::list_large,       xset::name::panel3_list_large1},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden1},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab1},
    {xset::panel::icon_status,      xset::name::panel3_icon_status1},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name1},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size1},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type1},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm1},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner1},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date1},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra1},
    {xset::panel::book_fol,         xset::name::panel3_book_fol1},
    {xset::panel::tool_l,           xset::name::panel3_tool_l1},
    {xset::panel::tool_r,           xset::name::panel3_tool_r1},
    {xset::panel::tool_s,           xset::name::panel3_tool_s1},
};

// panel3 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox2},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon2},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree2},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar2},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions2},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed2},
    {xset::panel::list_icons,       xset::name::panel3_list_icons2},
    {xset::panel::list_compact,     xset::name::panel3_list_compact2},
    {xset::panel::list_large,       xset::name::panel3_list_large2},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden2},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab2},
    {xset::panel::icon_status,      xset::name::panel3_icon_status2},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name2},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size2},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type2},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm2},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner2},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date2},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra2},
    {xset::panel::book_fol,         xset::name::panel3_book_fol2},
    {xset::panel::tool_l,           xset::name::panel3_tool_l2},
    {xset::panel::tool_r,           xset::name::panel3_tool_r2},
    {xset::panel::tool_s,           xset::name::panel3_tool_s2},
};

// panel3 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox3},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon3},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree3},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar3},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions3},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed3},
    {xset::panel::list_icons,       xset::name::panel3_list_icons3},
    {xset::panel::list_compact,     xset::name::panel3_list_compact3},
    {xset::panel::list_large,       xset::name::panel3_list_large3},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden3},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab3},
    {xset::panel::icon_status,      xset::name::panel3_icon_status3},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name3},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size3},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type3},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm3},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner3},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date3},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra3},
    {xset::panel::book_fol,         xset::name::panel3_book_fol3},
    {xset::panel::tool_l,           xset::name::panel3_tool_l3},
    {xset::panel::tool_r,           xset::name::panel3_tool_r3},
    {xset::panel::tool_s,           xset::name::panel3_tool_s3},
};

// panel4

// panel4 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox0},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon0},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree0},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar0},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions0},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed0},
    {xset::panel::list_icons,       xset::name::panel4_list_icons0},
    {xset::panel::list_compact,     xset::name::panel4_list_compact0},
    {xset::panel::list_large,       xset::name::panel4_list_large0},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden0},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab0},
    {xset::panel::icon_status,      xset::name::panel4_icon_status0},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name0},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size0},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type0},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm0},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner0},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date0},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra0},
    {xset::panel::book_fol,         xset::name::panel4_book_fol0},
    {xset::panel::tool_l,           xset::name::panel4_tool_l0},
    {xset::panel::tool_r,           xset::name::panel4_tool_r0},
    {xset::panel::tool_s,           xset::name::panel4_tool_s0},
};

// panel4 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox1},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon1},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree1},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar1},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions1},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed1},
    {xset::panel::list_icons,       xset::name::panel4_list_icons1},
    {xset::panel::list_compact,     xset::name::panel4_list_compact1},
    {xset::panel::list_large,       xset::name::panel4_list_large1},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden1},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab1},
    {xset::panel::icon_status,      xset::name::panel4_icon_status1},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name1},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size1},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type1},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm1},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner1},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date1},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra1},
    {xset::panel::book_fol,         xset::name::panel4_book_fol1},
    {xset::panel::tool_l,           xset::name::panel4_tool_l1},
    {xset::panel::tool_r,           xset::name::panel4_tool_r1},
    {xset::panel::tool_s,           xset::name::panel4_tool_s1},
};

// panel4 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox2},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon2},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree2},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar2},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions2},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed2},
    {xset::panel::list_icons,       xset::name::panel4_list_icons2},
    {xset::panel::list_compact,     xset::name::panel4_list_compact2},
    {xset::panel::list_large,       xset::name::panel4_list_large2},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden2},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab2},
    {xset::panel::icon_status,      xset::name::panel4_icon_status2},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name2},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size2},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type2},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm2},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner2},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date2},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra2},
    {xset::panel::book_fol,         xset::name::panel4_book_fol2},
    {xset::panel::tool_l,           xset::name::panel4_tool_l2},
    {xset::panel::tool_r,           xset::name::panel4_tool_r2},
    {xset::panel::tool_s,           xset::name::panel4_tool_s2},
};

// panel4 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox3},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon3},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree3},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar3},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions3},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed3},
    {xset::panel::list_icons,       xset::name::panel4_list_icons3},
    {xset::panel::list_compact,     xset::name::panel4_list_compact3},
    {xset::panel::list_large,       xset::name::panel4_list_large3},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden3},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab3},
    {xset::panel::icon_status,      xset::name::panel4_icon_status3},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name3},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size3},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type3},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm3},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner3},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date3},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra3},
    {xset::panel::book_fol,         xset::name::panel4_book_fol3},
    {xset::panel::tool_l,           xset::name::panel4_tool_l3},
    {xset::panel::tool_r,           xset::name::panel4_tool_r3},
    {xset::panel::tool_s,           xset::name::panel4_tool_s3},
};

static const std::unordered_map<xset::main_window_panel, std::string_view> main_window_panel_mode_map{
    {xset::main_window_panel::panel_neither, "0"sv},
    {xset::main_window_panel::panel_horiz,   "1"sv},
    {xset::main_window_panel::panel_vert,    "2"sv},
    {xset::main_window_panel::panel_both,    "3"sv},
};

// clang-format on

xset::name
xset::get_xsetname_from_name(const std::string_view name)
{
    const auto enum_value = magic_enum::enum_cast<xset::name>(name);
    if (!enum_value.has_value())
    {
        // ztd::logger::debug("name lookup custom {}", name);
        return xset::name::custom;
    }
    // ztd::logger::debug("name lookup {}", name);
    return enum_value.value();
}

const std::string_view
xset::get_name_from_xsetname(xset::name name)
{
    const auto value = magic_enum::enum_name(name);
    return value;
}

// panel

xset::name
xset::get_xsetname_from_panel(panel_t panel, xset::panel name)
{
    assert(panel == 1 || panel == 2 || panel == 3 || panel == 4);

    switch (panel)
    {
        case 1:
            return xset_panel1_map.at(name);
        case 2:
            return xset_panel2_map.at(name);
        case 3:
            return xset_panel3_map.at(name);
        case 4:
            return xset_panel4_map.at(name);
        default:
            // ztd::logger::warn("Panel out of range, using panel 1");
            return xset_panel1_map.at(name);
    }
}

const std::string_view
xset::get_name_from_panel(panel_t panel, xset::panel name)
{
    const auto set_name = xset::get_xsetname_from_panel(panel, name);
    const auto value = magic_enum::enum_name(set_name);
    return value;
}

// panel mode

xset::name
xset::get_xsetname_from_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode)
{
    assert(panel == 1 || panel == 2 || panel == 3 || panel == 4);

    switch (panel)
    {
        case 1:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel1_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel1_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel1_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel1_mode3_map.at(name);
            }
        case 2:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel2_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel2_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel2_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel2_mode3_map.at(name);
            }
        case 3:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel3_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel3_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel3_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel3_mode3_map.at(name);
            }
        case 4:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel4_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel4_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel4_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel4_mode3_map.at(name);
            }
        default:
            // ztd::logger::warn("Panel Mode out of range: {}, using panel 1",
            // std::to_string(mode));
            return xset::get_xsetname_from_panel_mode(1, name, mode);
    }
}

const std::string_view
xset::get_name_from_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode)
{
    const auto set_name = xset::get_xsetname_from_panel_mode(panel, name, mode);
    const auto value = magic_enum::enum_name(set_name);
    return value;
}

// xset var

xset::var
xset::get_xsetvar_from_name(const std::string_view name)
{
    const auto enum_value = magic_enum::enum_cast<xset::var>(name);
    if (!enum_value.has_value())
    {
        const auto msg = std::format("Invalid xset::var enum name xset::var::{}", name);
        ztd::logger::critical(msg);
        throw std::logic_error(msg);
    }
    return enum_value.value();
}

const std::string_view
xset::get_name_from_xsetvar(xset::var name)
{
    const auto value = magic_enum::enum_name(name);
    return value;
}

// main window panel mode

const std::string_view
xset::get_window_panel_mode(xset::main_window_panel mode)
{
    return main_window_panel_mode_map.at(mode);
}
