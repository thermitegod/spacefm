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

#include <filesystem>
#include <format>

#include <ztd/ztd.hxx>

#include "vfs/error.hxx"
#include "vfs/execute.hxx"
#include "vfs/task/task.hxx"

vfs::copy&
vfs::copy::archive() noexcept
{
    this->options_ += "--archive ";
    return *this;
}

vfs::copy&
vfs::copy::recursive() noexcept
{
    this->options_ += "--recursive ";
    return *this;
}

vfs::copy&
vfs::copy::force() noexcept
{
    this->options_ += "--force ";
    return *this;
}

vfs::copy&
vfs::copy::source(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        this->ec_ = vfs::error_code::task_empty_source;
        return *this;
    }

    if (path == "/")
    {
        this->ec_ = vfs::error_code::task_root_preserve_source;
        return *this;
    }

    this->source_ = path;
    return *this;
}

vfs::copy&
vfs::copy::destination(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        this->ec_ = vfs::error_code::task_empty_destination;
        return *this;
    }

    if (path == "/")
    {
        this->ec_ = vfs::error_code::task_root_preserve_destination;
        return *this;
    }

    this->destination_ = path;
    return *this;
}

void
vfs::copy::compile() noexcept
{
    if (this->ec_)
    {
        return;
    }

    if (this->source_.empty() || this->destination_.empty())
    {
        this->ec_ = vfs::error_code::task_bad_construction;
        return;
    }

    this->cmd_ = std::format("cp {} {} {}",
                             ztd::strip(this->options_),
                             vfs::execute::quote(this->source_.string()),
                             vfs::execute::quote(this->destination_.string()));
}
