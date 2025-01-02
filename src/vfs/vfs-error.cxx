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

#include "vfs/vfs-error.hxx"

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
            switch (static_cast<vfs::error_code>(c))
            {
                case vfs::error_code::none:
                    return "no error";
                case vfs::error_code::parse_error:
                    return "parse error";
                case vfs::error_code::key_not_found:
                    return "key not found";
                case vfs::error_code::unknown_key:
                    return "unknown key";
                case vfs::error_code::missing_key:
                    return "missing key";
                case vfs::error_code::file_not_found:
                    return "file not found";
                case vfs::error_code::file_too_large:
                    return "file too large";
                case vfs::error_code::file_open_failure:
                    return "file open failure";
                case vfs::error_code::file_read_failure:
                    return "file read failure";
                case vfs::error_code::file_write_failure:
                    return "file write failure";
                case vfs::error_code::file_close_failure:
                    return "file close failure";
                default:
                    return "unknown error";
            }
        }
    };
    static const category instance{};
    return instance;
}
