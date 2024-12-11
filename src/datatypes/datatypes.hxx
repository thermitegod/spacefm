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

#include <glaze/glaze.hpp>

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
    static constexpr auto value =
        glz::object("name", &datatype::file_action_dialog::request::name, "size",
                    &datatype::file_action_dialog::request::size, "is_dir",
                    &datatype::file_action_dialog::request::is_dir);
};

template<> struct glz::meta<datatype::file_action_dialog::response>
{
    static constexpr auto value =
        glz::object("result", &datatype::file_action_dialog::response::result);
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
    static constexpr auto value =
        glz::object("result", &datatype::message_dialog::response::result);
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
    static constexpr auto value =
        glz::object("result", &datatype::pattern_dialog::response::result);
};
