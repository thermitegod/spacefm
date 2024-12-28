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

#include <glaze/glaze.hpp>

namespace datatype
{
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
} // namespace datatype
