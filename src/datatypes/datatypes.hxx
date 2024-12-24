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

namespace datatype::app_chooser_dialog
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
} // namespace datatype::app_chooser_dialog

template<> struct glz::meta<datatype::app_chooser_dialog::request>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "mime_type",      &datatype::app_chooser_dialog::request::mime_type,
        "focus_all_apps", &datatype::app_chooser_dialog::request::focus_all_apps,
        "show_command",   &datatype::app_chooser_dialog::request::show_command,
        "show_default",   &datatype::app_chooser_dialog::request::show_default,
        "dir_default",    &datatype::app_chooser_dialog::request::dir_default
    );
    // clang-format on
};

namespace datatype::file_action_dialog
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
} // namespace datatype::file_action_dialog

template<> struct glz::meta<datatype::file_action_dialog::request>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "name",   &datatype::file_action_dialog::request::name,
        "size",   &datatype::file_action_dialog::request::size,
        "is_dir", &datatype::file_action_dialog::request::is_dir
    );
    // clang-format on
};

template<> struct glz::meta<datatype::file_action_dialog::response>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "result", &datatype::file_action_dialog::response::result
    );
    // clang-format on
};

namespace datatype::keybinding_dialog
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
} // namespace datatype::keybinding_dialog

template<> struct glz::meta<datatype::keybinding_dialog::request>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "name",       &datatype::keybinding_dialog::request::name,
        "label",      &datatype::keybinding_dialog::request::label,
        "category",   &datatype::keybinding_dialog::request::category,
        "shared_key", &datatype::keybinding_dialog::request::category,
        "key",        &datatype::keybinding_dialog::request::key,
        "modifier",   &datatype::keybinding_dialog::request::modifier
    );
    // clang-format on
};

template<> struct glz::meta<datatype::keybinding_dialog::response>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "name",     &datatype::keybinding_dialog::response::name,
        "key",      &datatype::keybinding_dialog::response::key,
        "modifier", &datatype::keybinding_dialog::response::modifier
    );
    // clang-format on
};

namespace datatype::message_dialog
{
struct response
{
    std::string result;
};
} // namespace datatype::message_dialog

template<> struct glz::meta<datatype::message_dialog::response>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "result", &datatype::message_dialog::response::result
    );
    // clang-format on
};

namespace datatype::pattern_dialog
{
struct response
{
    std::string result;
};
} // namespace datatype::pattern_dialog

template<> struct glz::meta<datatype::pattern_dialog::response>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "result", &datatype::pattern_dialog::response::result
    );
    // clang-format on
};

namespace datatype::properties_dialog
{
struct request
{
    std::string cwd;
    std::uint32_t page;
    std::vector<std::string> files;
};
} // namespace datatype::properties_dialog

template<> struct glz::meta<datatype::properties_dialog::request>
{
    // clang-format off
    static constexpr auto value = glz::object(
        "cwd",   &datatype::properties_dialog::request::cwd,
        "page",  &datatype::properties_dialog::request::page,
        "files", &datatype::properties_dialog::request::files
    );
    // clang-format on
};
