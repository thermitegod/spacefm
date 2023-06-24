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

#include <string>
#include <string_view>

#include <map>

#include <ztd/ztd.hxx>

// delimiter used in config file for tabs
const std::string CONFIG_FILE_TABS_DELIM = "///";

// Config file macros for on disk names

/**
 *  TOML data structure types for serialization
 */

// map<var, value>
using setvars_t = std::map<std::string, std::string>;
// map<xset_name, setvars_t>
using xsetpak_t = std::map<std::string, setvars_t>;

/**
 * TOML
 */

// TOML config file
inline constexpr u64 CONFIG_FILE_VERSION{1}; // 3.0.0-dev
const std::string CONFIG_FILE_FILENAME{"session.toml"};

// TOML config on disk names - TOML sections
const std::string TOML_SECTION_VERSION{"Version"};
const std::string TOML_SECTION_GENERAL{"General"};
const std::string TOML_SECTION_WINDOW{"Window"};
const std::string TOML_SECTION_INTERFACE{"Interface"};
const std::string TOML_SECTION_XSET{"XSet"};

// TOML config on disk names - TOML section keys
const std::string TOML_KEY_VERSION{"version"};

const std::string TOML_KEY_SHOW_THUMBNAIL{"show_thumbnail"};
const std::string TOML_KEY_MAX_THUMB_SIZE{"max_thumb_size"};
const std::string TOML_KEY_ICON_SIZE_BIG{"icon_size_big"};
const std::string TOML_KEY_ICON_SIZE_SMALL{"icon_size_small"};
const std::string TOML_KEY_ICON_SIZE_TOOL{"icon_size_tool"};
const std::string TOML_KEY_SINGLE_CLICK{"single_click"};
const std::string TOML_KEY_SINGLE_HOVER{"single_hover"};
const std::string TOML_KEY_SORT_ORDER{"sort_order"};
const std::string TOML_KEY_SORT_TYPE{"sort_type"};
const std::string TOML_KEY_USE_SI_PREFIX{"use_si_prefix"};
const std::string TOML_KEY_CLICK_EXECUTE{"click_executes"};
const std::string TOML_KEY_CONFIRM{"confirm"};
const std::string TOML_KEY_CONFIRM_DELETE{"confirm_delete"};
const std::string TOML_KEY_CONFIRM_TRASH{"confirm_trash"};

const std::string TOML_KEY_HEIGHT{"height"};
const std::string TOML_KEY_WIDTH{"width"};
const std::string TOML_KEY_MAXIMIZED{"maximized"};

const std::string TOML_KEY_SHOW_TABS{"always_show_tabs"};
const std::string TOML_KEY_SHOW_CLOSE{"show_close_tab_buttons"};

// TOML XSet
const std::string XSET_KEY_S{"s"};
const std::string XSET_KEY_B{"b"};
const std::string XSET_KEY_X{"x"};
const std::string XSET_KEY_Y{"y"};
const std::string XSET_KEY_Z{"z"};
const std::string XSET_KEY_KEY{"key"};
const std::string XSET_KEY_KEYMOD{"keymod"};
const std::string XSET_KEY_STYLE{"style"};
const std::string XSET_KEY_DESC{"desc"};
const std::string XSET_KEY_TITLE{"title"};
const std::string XSET_KEY_MENU_LABEL{"menu_label"};
const std::string XSET_KEY_MENU_LABEL_CUSTOM{"menu_label_custom"};
const std::string XSET_KEY_ICN{"icn"};
const std::string XSET_KEY_ICON{"icon"};
const std::string XSET_KEY_SHARED_KEY{"shared_key"};
const std::string XSET_KEY_NEXT{"next"};
const std::string XSET_KEY_PREV{"prev"};
const std::string XSET_KEY_PARENT{"parent"};
const std::string XSET_KEY_CHILD{"child"};
const std::string XSET_KEY_CONTEXT{"context"};
const std::string XSET_KEY_LINE{"line"};
const std::string XSET_KEY_TOOL{"tool"};
const std::string XSET_KEY_TASK{"task"};
const std::string XSET_KEY_TASK_POP{"task_pop"};
const std::string XSET_KEY_TASK_ERR{"task_err"};
const std::string XSET_KEY_TASK_OUT{"task_out"};
const std::string XSET_KEY_RUN_IN_TERMINAL{"run_in_terminal"};
const std::string XSET_KEY_KEEP_TERMINAL{"keep_terminal"};
const std::string XSET_KEY_SCROLL_LOCK{"scroll_lock"};
const std::string XSET_KEY_DISABLE{"disable"};
const std::string XSET_KEY_OPENER{"opener"};

// TOML Plugins
const std::string PLUGIN_FILE_FILENAME{"plugin.toml"};

const std::string PLUGIN_FILE_SECTION_PLUGIN{"Plugin"};
