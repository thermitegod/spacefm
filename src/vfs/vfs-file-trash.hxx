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

#include <string>
#include <map>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class TrashDir;
using TrashDirMap = std::map<dev_t, TrashDir*>;

// This class implements some of the XDG Trash specification:
//
// https://standards.freedesktop.org/trash-spec/trashspec-1.0.html
class Trash
{
  public:
    // Move a file or directory into the trash.
    static bool trash(const std::string& path) noexcept;

    // Restore a file or directory from the trash to its original location.
    // Currently a NOOP
    static bool restore(const std::string& path) noexcept;

    // Empty all trash cans
    // Currently a NOOP
    static void empty() noexcept;

    // Return the singleton object for this class. The first use will create
    // the singleton. Notice that the static methods all access the singleton,
    // too, so the first call to any of those static methods will already
    // create the singleton.
    static Trash* instance() noexcept;

    // return the device of the file or directory
    static dev_t device(const std::string& path) noexcept;

  protected:
    // Constructor
    // Not for public use. Use instance() or the static methods instead.
    Trash() noexcept;

    // Destructor.
    virtual ~Trash();

    // Find the toplevel directory (mount point) for the device that 'path' is on.
    static const std::string toplevel(const std::string& path) noexcept;

    // Return the trash dir to use for 'path'.
    TrashDir* trash_dir(const std::string& path) noexcept;

    // Data Members
    static Trash* single_instance;

    dev_t home_device;
    TrashDir* home_trash_dir{nullptr};
    TrashDirMap trash_dirs;

}; // class Trash

// trash directories. There might be several on a system:
//
// One in $XDG_DATA_HOME/Trash or ~/.local/share/Trash
// if $XDG_DATA_HOME is not set
//
// Every mountpoint will get a trash directory at $TOPLEVEL/.Trash-$UID.
class TrashDir
{
  public:
    // Create the trash directory and subdirectories if they do not exist.
    TrashDir(const std::string& path, dev_t device) noexcept;

    // Return the full path for this trash directory.
    const std::string& trash_path() const noexcept;

    // Return the device (as returned from stat()) for this trash directory.
    dev_t device() const noexcept;

    // Return the path of the "files" subdirectory of this trash dir.
    const std::string& files_path() const noexcept;

    // Return the path of the "info" subdirectory of this trash dir.
    const std::string& info_path() const noexcept;

    // Get a unique name for use within the trash directory
    const std::string unique_name(const std::string& path) const noexcept;

    void create_trash_dir() const noexcept;

    // Create a .trashinfo file for a file or directory 'path'
    void create_trash_info(const std::string& path, const std::string& target_name) const noexcept;

    // Move a file or directory into the trash directory
    void move(const std::string& path, const std::string& target_name) const noexcept;

  protected:
    // Create a directory if it does not exist
    static void check_dir_exists(const std::string& dir) noexcept;

    // Data Members
    dev_t trash_dir_device;
    std::string trash_dir_path;
    std::string trash_dir_files_path;
    std::string trash_dir_info_path;
};
