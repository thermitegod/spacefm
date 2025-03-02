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
#include <vector>

#include <cstdint>

namespace datatype
{
struct settings
{
    // General Settings
    bool show_thumbnails{false};
    bool thumbnail_size_limit{true};
    std::uint32_t thumbnail_max_size{8 << 20}; // 8 MiB

    std::int32_t icon_size_big{48};
    std::int32_t icon_size_small{22};
    std::int32_t icon_size_tool{22};

    bool click_executes{false};

    bool confirm{true};
    bool confirm_delete{true};
    bool confirm_trash{true};

    bool load_saved_tabs{true};

    // Window State
    bool maximized{false};

    // Interface
    bool always_show_tabs{true};
    bool show_close_tab_buttons{false};
    bool new_tab_here{false};

    bool show_toolbar_home{true};
    bool show_toolbar_refresh{true};
    bool show_toolbar_search{true};

    bool use_si_prefix{false};
};

// Only to be used by preference dialog
struct settings_extended
{
    settings settings;

    // xset settings
    std::int32_t drag_action;
    std::string editor;
    std::string terminal;

    struct
    { // send only data
        std::vector<std::string> supported_terminals;
    } details;
};

namespace app_chooser
{
struct request
{
    std::string mime_type;
    bool focus_all_apps;
    bool show_command;
    bool show_default;
    bool dir_default;
};

struct response
{
    std::string app;
    bool is_desktop;
    bool set_default;
};
} // namespace app_chooser

namespace error
{
struct request
{
    std::string title;
    std::string message;
};
} // namespace error

namespace file_action
{
struct request
{
    std::string name;
    std::uint64_t size;
    bool is_dir;
};

struct response
{
    std::string result;
};
} // namespace file_action

namespace keybinding
{
struct request
{
    std::string name;
    std::string label;
    std::string category;
    std::string shared_key;
    std::uint32_t key;
    std::uint32_t modifier;
};

struct response
{
    std::string name;
    std::uint32_t key;
    std::uint32_t modifier;
};
} // namespace keybinding

namespace message
{
struct request
{
    std::string title;
    std::string message;
    std::string secondary_message;
    bool button_ok;
    bool button_cancel;
    bool button_close;
    bool button_yes_no;
    bool button_ok_cancel;
};

struct response
{
    std::string result;
};
} // namespace message

namespace pattern
{
struct response
{
    std::string result;
};
} // namespace pattern

namespace properties
{
struct request
{
    std::string cwd;
    std::uint32_t page;
    std::vector<std::string> files;
};
} // namespace properties

namespace create
{
struct settings_data
{
    bool filename{false};
    bool parent{false};
    bool path{false};
    bool target{false};
    bool confirm{false};
};

enum class mode : std::uint8_t
{
    file,
    dir,
    link,
    cancel,
};

struct request
{
    std::string cwd;
    std::string file; // used to create a new symlink to the selected file
    mode mode;
    settings_data settings;
};

struct response
{
    std::string target; // only used when creating a symlink
    std::string dest;
    mode mode;
    bool overwrite;
    bool auto_open; // open file / chdir into dest
    settings_data settings;
};
} // namespace create
} // namespace datatype
