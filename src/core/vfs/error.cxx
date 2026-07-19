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
#include <system_error>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/error.hxx"

const std::error_category&
vfs::error_category() noexcept
{
    struct category final : std::error_category
    {
        const char*
        name() const noexcept override final
        {
            return "vfs::error_category()";
        }

        std::string
        message(int c) const override final
        {
            return ztd::replace(magic_enum::enum_name(static_cast<vfs::error_code>(c)), "_", " ");
        }
    };
    static const category instance{};
    return instance;
}
