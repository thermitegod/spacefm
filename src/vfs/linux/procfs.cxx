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
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/linux/procfs.hxx"
#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

std::vector<vfs::linux::procfs::MountInfoEntry>
vfs::linux::procfs::mountinfo() noexcept
{
    std::vector<MountInfoEntry> mounts;

    const auto buffer = vfs::utils::read_file(MOUNTINFO);
    if (!buffer)
    {
        logger::error<logger::domain::vfs>("Failed to read mountinfo: {} {}",
                                           MOUNTINFO,
                                           buffer.error().message());
        return mounts;
    }

    for (const auto& line : ztd::split(*buffer, "\n"))
    {
        if (line.empty())
        {
            break; // EOF
        }

        const auto fields = ztd::split(line, " ");
        if (fields.size() != 11)
        {
            logger::error<logger::domain::vfs>("Invalid mountinfo entry: size={}, line={}",
                                               fields.size(),
                                               line);
            continue;
        }

        MountInfoEntry mount;

        mount.mount_id = ztd::from_string<std::uint64_t>(fields[0]).value();
        mount.parent_id = ztd::from_string<std::uint64_t>(fields[1]).value();
        const auto minor_major = ztd::partition(fields[2], ":");
        mount.major = ztd::from_string<std::uint64_t>(minor_major[0]).value();
        mount.minor = ztd::from_string<std::uint64_t>(minor_major[2]).value();
        mount.root = Glib::strcompress(fields[3]);        // Encoded Field
        mount.mount_point = Glib::strcompress(fields[4]); // Encoded Field
        mount.mount_options = fields[5];
        mount.optional_fields = fields[6];
        mount.separator = fields[7];
        mount.filesystem_type = fields[8];
        mount.mount_source = fields[9];
        mount.super_options = fields[10];

        // logger::info<logger::domain::vfs>("==========================================");
        // logger::info<logger::domain::vfs>("mount.mount_id        = {}", mount.mount_id);
        // logger::info<logger::domain::vfs>("mount.parent_id       = {}", mount.parent_id);
        // logger::info<logger::domain::vfs>("mount.major           = {}", mount.major);
        // logger::info<logger::domain::vfs>("mount.minor           = {}", mount.minor);
        // logger::info<logger::domain::vfs>("mount.root            = {}", mount.root);
        // logger::info<logger::domain::vfs>("mount.mount_point     = {}", mount.mount_point);
        // logger::info<logger::domain::vfs>("mount.mount_options   = {}", mount.mount_options);
        // logger::info<logger::domain::vfs>("mount.optional_fields = {}", mount.optional_fields);
        // logger::info<logger::domain::vfs>("mount.separator       = {}", mount.separator);
        // logger::info<logger::domain::vfs>("mount.filesystem_type = {}", mount.filesystem_type);
        // logger::info<logger::domain::vfs>("mount.mount_source    = {}", mount.mount_source);
        // logger::info<logger::domain::vfs>("mount.super_options   = {}", mount.super_options);

        mounts.push_back(mount);
    }

    return mounts;
}
