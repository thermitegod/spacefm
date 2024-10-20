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

#include <fstream>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "vfs/linux/procfs.hxx"

std::vector<vfs::linux::procfs::MountInfoEntry>
vfs::linux::procfs::mountinfo() noexcept
{
    std::vector<MountInfoEntry> mounts;

    std::ifstream file(MOUNTINFO);
    if (!file)
    {
        logger::error<logger::domain::vfs>("Failed to open the file: {}", MOUNTINFO);
        return mounts;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = ztd::split(line, " ");
        if (fields.size() != 11)
        {
            logger::error<logger::domain::vfs>("Invalid mountinfo entry: size={}, line={}",
                                               fields.size(),
                                               line);
            continue;
        }

        MountInfoEntry mount;

        mount.mount_id = std::stoul(fields[0]);
        mount.parent_id = std::stoul(fields[1]);
        const auto minor_major = ztd::partition(fields[2], ":");
        mount.major = std::stoul(minor_major[0]);
        mount.minor = std::stoul(minor_major[2]);
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
