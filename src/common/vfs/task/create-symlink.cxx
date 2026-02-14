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

vfs::create_symlink&
vfs::create_symlink::force() noexcept
{
    this->options_ += "--force ";
    return *this;
}

vfs::create_symlink&
vfs::create_symlink::target(const std::filesystem::path& path) noexcept
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

    this->target_ = path;
    return *this;
}

vfs::create_symlink&
vfs::create_symlink::name(const std::filesystem::path& path) noexcept
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

    this->name_ = path;
    return *this;
}

void
vfs::create_symlink::compile() noexcept
{
    if (this->ec_)
    {
        return;
    }

    if (this->target_.empty() || this->name_.empty())
    {
        this->ec_ = vfs::error_code::task_bad_construction;
        return;
    }

    this->cmd_ = std::format("ln -s {} {} {}",
                             ztd::strip(this->options_),
                             vfs::execute::quote(this->target_.string()),
                             vfs::execute::quote(this->name_.string()));
}
