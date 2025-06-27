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

#include <glibmm.h>

#include "vfs/execute.hxx"

#include "logger.hxx"

vfs::execute::sync_data
vfs::execute::command_line_sync(const std::string_view command) noexcept
{
    logger::info<logger::domain::execute>(command);

    sync_data data;
    Glib::spawn_command_line_sync(command.data(),
                                  &data.standard_output,
                                  &data.standard_error,
                                  &data.exit_status);

    return data;
}

void
vfs::execute::command_line_async(const std::string_view command) noexcept
{
    logger::info<logger::domain::execute>(command);

    Glib::spawn_command_line_async(command.data());
}
