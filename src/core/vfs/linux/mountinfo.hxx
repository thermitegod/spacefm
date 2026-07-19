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
#include <optional>
#include <string>
#include <vector>

#include <ztd/ztd.hxx>

namespace vfs::proc
{
inline const std::filesystem::path MOUNTINFO = "/proc/self/mountinfo";

/**
 * See Documentation/filesystems/proc.rst for the format of /proc/self/mountinfo
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
struct mountinfo_entry final
{
  public:
    mountinfo_entry() = default;
    mountinfo_entry(std::string_view line);

    [[nodiscard]] static std::optional<mountinfo_entry>
    create(const std::string_view line) noexcept;

    [[nodiscard]] std::size_t mount_id() const noexcept;
    [[nodiscard]] std::size_t parent_id() const noexcept;
    [[nodiscard]] dev_t major() const noexcept;
    [[nodiscard]] dev_t minor() const noexcept;
    [[nodiscard]] std::string root() const noexcept;
    [[nodiscard]] std::string mount_point() const noexcept;
    [[nodiscard]] std::string mount_options() const noexcept;
    [[nodiscard]] std::string optional_fields() const noexcept;
    [[nodiscard]] std::string filesystem_type() const noexcept;
    [[nodiscard]] std::string mount_source() const noexcept;
    [[nodiscard]] std::string super_options() const noexcept;

  private:
    std::size_t mount_id_;
    std::size_t parent_id_;
    dev_t major_;
    dev_t minor_;
    std::string root_;
    std::string mount_point_;
    std::string mount_options_;
    std::string optional_fields_;
    std::string filesystem_type_;
    std::string mount_source_;
    std::string super_options_;
};

std::vector<mountinfo_entry> mountinfo(const std::filesystem::path& path = MOUNTINFO) noexcept;
} // namespace vfs::proc
