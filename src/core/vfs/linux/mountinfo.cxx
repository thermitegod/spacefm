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
#include <stdexcept>
#include <string>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/linux/mountinfo.hxx"
#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

std::vector<vfs::proc::mountinfo_entry>
vfs::proc::mountinfo(const std::filesystem::path& path) noexcept
{
    std::vector<mountinfo_entry> mounts;

    const auto buffer = vfs::utils::read_file(path);
    if (!buffer)
    {
        logger::error<logger::vfs>("Failed to read mountinfo: {} {}",
                                   path,
                                   buffer.error().message());
        return mounts;
    }

    for (const auto& line : ztd::split(*buffer, "\n"))
    {
        auto entry = mountinfo_entry::create(line);

        if (entry)
        {
            mounts.push_back(*entry);
        }
    }

    return mounts;
}

vfs::proc::mountinfo_entry::mountinfo_entry(const std::string_view line)
{
    if (line.empty())
    {
        throw std::runtime_error("");
    }

    const auto fields = ztd::split(line, " ");
    if (fields.size() != 11)
    {
        logger::error<logger::vfs>("Invalid mountinfo entry: size={}, line={}",
                                   fields.size(),
                                   line);
        throw std::runtime_error("");
    }

    mount_id_ = ztd::from_string<std::size_t>(fields[0]).value();
    parent_id_ = ztd::from_string<std::size_t>(fields[1]).value();
    const auto minor_major = ztd::partition(fields[2], ":");
    major_ = ztd::from_string<std::size_t>(minor_major[0]).value();
    minor_ = ztd::from_string<std::size_t>(minor_major[2]).value();
    root_ = Glib::strcompress(fields[3]);        // Encoded Field
    mount_point_ = Glib::strcompress(fields[4]); // Encoded Field
    mount_options_ = fields[5];
    optional_fields_ = fields[6];
    // separator = fields[7];
    filesystem_type_ = fields[8];
    mount_source_ = fields[9];
    super_options_ = fields[10];

    // logger::debug<logger::vfs>("==========================================");
    // logger::debug<logger::vfs>("mount_id        = {}", mount_id_);
    // logger::debug<logger::vfs>("parent_id       = {}", parent_id_);
    // logger::debug<logger::vfs>("major           = {}", major_);
    // logger::debug<logger::vfs>("minor           = {}", minor_);
    // logger::debug<logger::vfs>("root            = {}", root_);
    // logger::debug<logger::vfs>("mount_point     = {}", mount_point_);
    // logger::debug<logger::vfs>("mount_options   = {}", mount_options_);
    // logger::debug<logger::vfs>("optional_fields = {}", optional_fields_);
    // logger::debug<logger::vfs>("filesystem_type = {}", filesystem_type_);
    // logger::debug<logger::vfs>("mount_source    = {}", mount_source_);
    // logger::debug<logger::vfs>("super_options   = {}", super_options_);
}

std::optional<vfs::proc::mountinfo_entry>
vfs::proc::mountinfo_entry::create(std::string_view line) noexcept
{
    try
    {
        return mountinfo_entry(line);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::size_t
vfs::proc::mountinfo_entry::mount_id() const noexcept
{
    return mount_id_;
}

std::size_t
vfs::proc::mountinfo_entry::parent_id() const noexcept
{
    return parent_id_;
}

dev_t
vfs::proc::mountinfo_entry::major() const noexcept
{
    return major_;
}

dev_t
vfs::proc::mountinfo_entry::minor() const noexcept
{
    return minor_;
}

std::string
vfs::proc::mountinfo_entry::root() const noexcept
{
    return root_;
}

std::string
vfs::proc::mountinfo_entry::mount_point() const noexcept
{
    return mount_point_;
}

std::string
vfs::proc::mountinfo_entry::mount_options() const noexcept
{
    return mount_options_;
}

std::string
vfs::proc::mountinfo_entry::optional_fields() const noexcept
{
    return optional_fields_;
}

std::string
vfs::proc::mountinfo_entry::filesystem_type() const noexcept
{
    return filesystem_type_;
}

std::string
vfs::proc::mountinfo_entry::mount_source() const noexcept
{
    return mount_source_;
}

std::string
vfs::proc::mountinfo_entry::super_options() const noexcept
{
    return super_options_;
}
