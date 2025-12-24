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

#pragma once

#include <filesystem>
#include <flat_map>
#include <memory>

#include <ztd/ztd.hxx>

// trash directories. There might be several on a system:
//
// One in $XDG_DATA_HOME/Trash or ~/.local/share/Trash
// if $XDG_DATA_HOME is not set
//
// Every mountpoint will get a trash directory at $TOPLEVEL/.Trash-$UID.
namespace vfs
{
// This class implements some of the XDG Trash specification:
//
// https://specifications.freedesktop.org/trash/1.0/
class trash_can final
{
  public:
    trash_can() noexcept;
    [[nodiscard]] static std::shared_ptr<vfs::trash_can> create() noexcept;

    // Move a file or directory into the trash.
    [[nodiscard]] static bool trash(const std::filesystem::path& path) noexcept;

    // Restore a file or directory from the trash to its original location.
    // Currently a NOOP
    [[nodiscard]] static bool restore(const std::filesystem::path& path) noexcept;

    // Empty a trash can
    // Currently a NOOP
    static void empty(const std::filesystem::path& path) noexcept;

  private:
    class trash_dir final
    {
      public:
        explicit trash_dir(const std::filesystem::path& path) noexcept;

        [[nodiscard]] std::filesystem::path
        unique_filename(const std::filesystem::path& path) const noexcept;
        void create_trash_dir() const noexcept;
        void create_trash_info(const std::filesystem::path& path,
                               const std::filesystem::path& target_filename) const noexcept;
        void move(const std::filesystem::path& path,
                  const std::filesystem::path& target_filename) const noexcept;

      private:
        std::filesystem::path trash_path_;
        std::filesystem::path files_path_;
        std::filesystem::path info_path_;
    };

    [[nodiscard]] static u64 mount_id(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static std::filesystem::path toplevel(const std::filesystem::path& path) noexcept;

    [[nodiscard]] std::shared_ptr<trash_dir>
    get_trash_dir(const std::filesystem::path& path) noexcept;

    std::flat_map<u64, std::shared_ptr<trash_dir>> trash_dirs_;
};
} // namespace vfs
