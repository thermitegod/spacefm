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

vfs::chown&
vfs::chown::recursive() noexcept
{
    this->options_ += "--recursive ";
    return *this;
}

vfs::chown&
vfs::chown::user(const std::string_view user) noexcept
{
    this->user_ = user;
    return *this;
}

vfs::chown&
vfs::chown::group(const std::string_view group) noexcept
{
    this->group_ = group;
    return *this;
}

vfs::chown&
vfs::chown::path(const std::filesystem::path& path) noexcept
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
vfs::chown::compile() noexcept
{
    if (this->ec_)
    {
        return;
    }

    if ((this->user_.empty() && this->group_.empty()) || this->path_.empty())
    {
        this->ec_ = vfs::error_code::task_bad_construction;
        return;
    }

    if (this->user_.empty())
    {
        this->cmd_ = std::format("chgrp --preserve-root {} {} {}",
                                 ztd::strip(this->options_),
                                 ztd::strip(this->group_),
                                 vfs::execute::quote(this->path_.string()));
    }
    else if (this->group_.empty())
    {
        this->cmd_ = std::format("chown --preserve-root {} {} {}",
                                 ztd::strip(this->options_),
                                 ztd::strip(this->user_),
                                 vfs::execute::quote(this->path_.string()));
    }
    else
    {
        this->cmd_ = std::format("chown --preserve-root {} {}:{} {}",
                                 ztd::strip(this->options_),
                                 ztd::strip(this->user_),
                                 ztd::strip(this->group_),
                                 vfs::execute::quote(this->path_.string()));
    }
}
