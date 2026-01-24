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

vfs::remove&
vfs::remove::recursive() noexcept
{
    this->options_ += "--recursive ";
    return *this;
}

vfs::remove&
vfs::remove::force() noexcept
{
    this->options_ += "--force ";
    return *this;
}

vfs::remove&
vfs::remove::path(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        this->ec_ = vfs::error_code::task_empty_path;
        return *this;
    }

    if (path == "/")
    {
        this->ec_ = vfs::error_code::task_root_preserve;
        return *this;
    }

    this->path_ = path;
    return *this;
}

void
vfs::remove::compile() noexcept
{
    if (this->ec_)
    {
        return;
    }

    if (this->path_.empty())
    {
        this->ec_ = vfs::error_code::task_bad_construction;
        return;
    }

    this->cmd_ = std::format("rm --one-file-system --preserve-root {} {}",
                             ztd::strip(this->options_),
                             vfs::execute::quote(this->path_.string()));
}
