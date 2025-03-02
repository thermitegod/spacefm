/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/utils/vfs-utils.hxx"
#include "vfs/vfs-file.hxx"
#include "vfs/vfs-mime-type.hxx"

#include "logger.hxx"

std::shared_ptr<vfs::file>
vfs::file::create(const std::filesystem::path& path) noexcept
{
    return std::make_shared<vfs::file>(path);
}

vfs::file::file(const std::filesystem::path& path) noexcept : path_(path)
{
    // logger::debug<logger::domain::vfs>("vfs::file::file({})    {}", logger::utils::ptr(this), this->path_);

    if (this->path_ == "/")
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string. that causes subtle bugs
        // so hard code "/" as the value for root.
        this->name_ = "/";
    }
    else
    {
        this->name_ = this->path_.filename();
    }

    // Is a hidden file
    this->is_hidden_ = this->name_.starts_with('.');

    const auto result = this->update();
    if (!result)
    {
        logger::error<logger::domain::vfs>("Failed to create vfs::file for {}", path.string());
    }
}

vfs::file::~file() noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::file::~file({})   {}", logger::utils::ptr(this), this->path_);
}

bool
vfs::file::update() noexcept
{
    const auto stat = ztd::statx::create(this->path_, ztd::statx::symlink::no_follow);
    if (!stat)
    {
        this->mime_type_ = vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
        return false;
    }
    this->file_stat_ = stat.value();

    // logger::debug<logger::domain::vfs>("vfs::file::update({})    {}  size={}", logger::utils::ptr(this), this->name, this->file_stat.size());

    // this->status = std::filesystem::status(file_path);
    this->status_ = std::filesystem::symlink_status(this->path_);

    this->mime_type_ = vfs::mime_type::create_from_file(this->path_);

    // file size formated
    this->display_size_ = vfs::utils::format_file_size(this->size());
    this->display_size_bytes_ = std::format("{:L}", this->size());

    // disk file size formated
    this->display_disk_size_ = vfs::utils::format_file_size(this->size_on_disk());

    // owner
    const auto pw = ztd::passwd(this->file_stat_.uid());
    this->display_owner_ = pw.name();

    // group
    const auto gr = ztd::group(this->file_stat_.gid());
    this->display_group_ = gr.name();

    // time
    this->display_atime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->atime()));
    this->display_btime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->btime()));
    this->display_ctime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->ctime()));
    this->display_mtime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->mtime()));

    return true;
}

std::string_view
vfs::file::name() const noexcept
{
    return this->name_;
}

const std::filesystem::path&
vfs::file::path() const noexcept
{
    return this->path_;
}

u64
vfs::file::size() const noexcept
{
    return this->file_stat_.size();
}

u64
vfs::file::size_on_disk() const noexcept
{
    return this->file_stat_.size_on_disk();
}

std::string_view
vfs::file::display_size() const noexcept
{
    return this->display_size_;
}

std::string_view
vfs::file::display_size_in_bytes() const noexcept
{
    return this->display_size_bytes_;
}

std::string_view
vfs::file::display_size_on_disk() const noexcept
{
    return this->display_disk_size_;
}

u64
vfs::file::blocks() const noexcept
{
    return this->file_stat_.blocks();
}

const std::shared_ptr<vfs::mime_type>&
vfs::file::mime_type() const noexcept
{
    return this->mime_type_;
}

std::string_view
vfs::file::display_owner() const noexcept
{
    return this->display_owner_;
}

std::string_view
vfs::file::display_group() const noexcept
{
    return this->display_group_;
}

std::string_view
vfs::file::display_atime() const noexcept
{
    return this->display_atime_;
}

std::string_view
vfs::file::display_btime() const noexcept
{
    return this->display_btime_;
}

std::string_view
vfs::file::display_ctime() const noexcept
{
    return this->display_ctime_;
}

std::string_view
vfs::file::display_mtime() const noexcept
{
    return this->display_mtime_;
}

std::chrono::system_clock::time_point
vfs::file::atime() const noexcept
{
    return this->file_stat_.atime();
}

std::chrono::system_clock::time_point
vfs::file::btime() const noexcept
{
    return this->file_stat_.btime();
}

std::chrono::system_clock::time_point
vfs::file::ctime() const noexcept
{
    return this->file_stat_.ctime();
}

std::chrono::system_clock::time_point
vfs::file::mtime() const noexcept
{
    return this->file_stat_.mtime();
}

bool
vfs::file::is_directory() const noexcept
{
    if (std::filesystem::is_symlink(this->status_))
    {
        std::error_code ec;
        const auto symlink_path = std::filesystem::read_symlink(this->path_, ec);
        if (!ec)
        {
            return std::filesystem::is_directory(symlink_path);
        }
    }
    return std::filesystem::is_directory(this->status_);
}

bool
vfs::file::is_regular_file() const noexcept
{
    return std::filesystem::is_regular_file(this->status_);
}

bool
vfs::file::is_symlink() const noexcept
{
    return std::filesystem::is_symlink(this->status_);
}

bool
vfs::file::is_socket() const noexcept
{
    return std::filesystem::is_socket(this->status_);
}

bool
vfs::file::is_fifo() const noexcept
{
    return std::filesystem::is_fifo(this->status_);
}

bool
vfs::file::is_block_file() const noexcept
{
    return std::filesystem::is_block_file(this->status_);
}

bool
vfs::file::is_character_file() const noexcept
{
    return std::filesystem::is_character_file(this->status_);
}

bool
vfs::file::is_other() const noexcept
{
    return (!this->is_directory() && !this->is_regular_file() && !this->is_symlink());
}

bool
vfs::file::is_hidden() const noexcept
{
    return this->is_hidden_;
}

bool
vfs::file::is_compressed() const noexcept
{
    return this->file_stat_.is_compressed();
}

bool
vfs::file::is_immutable() const noexcept
{
    return this->file_stat_.is_immutable();
}

bool
vfs::file::is_append() const noexcept
{
    return this->file_stat_.is_append();
}

bool
vfs::file::is_nodump() const noexcept
{
    return this->file_stat_.is_nodump();
}

bool
vfs::file::is_encrypted() const noexcept
{
    return this->file_stat_.is_encrypted();
}

bool
vfs::file::is_automount() const noexcept
{
    return this->file_stat_.is_automount();
}

bool
vfs::file::is_mount_root() const noexcept
{
    return this->file_stat_.is_mount_root();
}

bool
vfs::file::is_verity() const noexcept
{
    return this->file_stat_.is_verity();
}

bool
vfs::file::is_dax() const noexcept
{
    return this->file_stat_.is_dax();
}

std::filesystem::perms
vfs::file::permissions() const noexcept
{
    return this->status_.permissions();
}
