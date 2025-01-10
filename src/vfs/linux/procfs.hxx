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

#include <ztd/ztd.hxx>

inline const std::string MOUNTINFO{"/proc/self/mountinfo"};
// inline const std::string MTAB{"/proc/self/mounts"};

namespace vfs::linux::procfs
{
/* See Documentation/filesystems/proc.rst for the format of /proc/self/mountinfo
 *
 * Note that things like space are encoded as \020.
 *
 * 36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
 * (1)(2)(3)   (4)   (5)      (6)     (n…m) (m+1)(m+2) (m+3)         (m+4)
 *
 * (1)   mount ID:        unique identifier of the mount (may be reused after umount)
 * (2)   parent ID:       ID of parent (or of self for the top of the mount tree)
 * (3)   major:minor:     value of st_dev for files on filesystem
 * (4)   root:            root of the mount within the filesystem
 * (5)   mount point:     mount point relative to the process's root
 * (6)   mount options:   per mount options
 * (n…m) optional fields: zero or more fields of the form "tag[:value]"
 * (m+1) separator:       marks the end of the optional fields
 * (m+2) filesystem type: name of filesystem of the form "type[.subtype]"
 * (m+3) mount source:    filesystem specific information or "none"
 * (m+4) super options:   per super block options
 */

struct MountInfoEntry final
{
    u64 mount_id;
    u64 parent_id;
    dev_t major;
    dev_t minor;
    std::string root;
    std::string mount_point;
    std::string mount_options;
    std::string optional_fields;
    std::string separator;
    std::string filesystem_type;
    std::string mount_source;
    std::string super_options;
};

std::vector<MountInfoEntry> mountinfo() noexcept;
} // namespace vfs::linux::procfs
