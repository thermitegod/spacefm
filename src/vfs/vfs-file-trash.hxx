/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>
#include <map>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class TrashDir;
typedef std::map<dev_t, TrashDir*> TrashDirMap;

// This class implements some of the XDG Trash specification:
//
// https://standards.freedesktop.org/trash-spec/trashspec-1.0.html
class Trash
{
  public:
    // Move a file or directory into the trash.
    static bool trash(const std::string& path);

    // Restore a file or directory from the trash to its original location.
    // Currently a NOOP
    static bool restore(const std::string& path);

    // Empty all trash cans
    // Currently a NOOP
    static void empty();

    // Return the singleton object for this class. The first use will create
    // the singleton. Notice that the static methods all access the singleton,
    // too, so the first call to any of those static methods will already
    // create the singleton.
    static Trash* instance();

    // return the device of the file or directory
    static dev_t device(const std::string& path);

  protected:
    // Constructor
    // Not for public use. Use instance() or the static methods instead.
    Trash();

    // Destructor.
    virtual ~Trash();

    // Find the toplevel directory (mount point) for the device that 'path' is on.
    static std::string toplevel(const std::string& path);

    // Return the trash dir to use for 'path'.
    TrashDir* trash_dir(const std::string& path);

    // Data Members
    static Trash* _instance;

    dev_t _home_device;
    TrashDir* _home_trash_dir;
    TrashDirMap _trash_dirs;

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
    TrashDir(const std::string& _path, dev_t device);

    // Return the full path for this trash directory.
    std::string
    path() const
    {
        return _path;
    }

    // Return the device (as returned from stat()) for this trash directory.
    dev_t
    device() const
    {
        return _device;
    }

    // Return the path of the "files" subdirectory of this trash dir.
    std::string
    files_path() const
    {
        return _path + "/files";
    }

    // Return the path of the "info" subdirectory of this trash dir.
    std::string
    info_path() const
    {
        return _path + "/info";
    }

    // Get a unique name for use within the trash directory
    std::string unique_name(const std::string& path);

    // Create a .trashinfo file for a file or directory 'path'
    void create_trash_info(const std::string& path, const std::string& target_name);

    // Move a file or directory into the trash directory
    void move(const std::string& path, const std::string& target_name);

  protected:
    // Create a directory if it does not exist
    static bool check_dir_exists(const std::string& dir, mode_t mode);

    // Data Members
    std::string _path;
    dev_t _device;
};
