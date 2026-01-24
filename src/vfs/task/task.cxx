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

#include <expected>
#include <string>
#include <system_error>

#include "vfs/execute.hxx"
#include "vfs/task/task.hxx"

void
vfs::task::run() noexcept
{
    if (this->error() || this->cmd_.empty())
    {
        this->signal_failure().emit();
        return;
    }

    auto data = vfs::execute::command_line_sync(this->cmd_);

    if (data.exit_status != 0)
    {
        this->signal_failure().emit();
        return;
    }

    this->signal_success().emit();
}

[[nodiscard]] std::error_code
vfs::task::error() const noexcept
{
    return this->ec_;
}

[[nodiscard]] std::expected<std::string, std::error_code>
vfs::task::dump() const noexcept
{
    if (this->ec_)
    {
        return std::unexpected(this->ec_);
    }
    else
    {
        return this->cmd_;
    }
}
