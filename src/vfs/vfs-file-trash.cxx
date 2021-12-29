/*
 *
 * License: See COPYING file
 *
 */

#include <ctime>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <unistd.h>

#include <sys/stat.h>

#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-trash.hxx"
//#include "logger.hxx"

Trash* Trash::m_instance = nullptr;

Trash*
Trash::instance()
{
    if (!m_instance)
        m_instance = new Trash();

    return m_instance;
}

Trash::Trash()
{
    m_home_device = device(vfs_user_home_dir());

    std::string home_trash_dir = vfs_user_data_dir();
    home_trash_dir += "/Trash";

    m_home_trash_dir = new TrashDir(home_trash_dir, m_home_device);

    m_trash_dirs[m_home_device] = m_home_trash_dir;
}

Trash::~Trash()
{
    // NOOP
}

dev_t
Trash::device(const std::string& path)
{
    dev_t dev = 0;
    struct stat statBuf;
    int result = stat(path.c_str(), &statBuf);
    dev = statBuf.st_dev;

    // stat failed
    if (result < 0)
        dev = static_cast<dev_t>(-1);

    return dev;
}

std::string
Trash::toplevel(const std::string& path)
{
    dev_t dev = device(path);

    std::filesystem::path mount_parent;
    std::string mount_path = path;
    std::string last_path;

    // LOG_INFO("dev mount {}", device(mount_path));
    // LOG_INFO("dev       {}", dev);

    // walk up the path until it gets to the root of the device
    while (device(mount_path) == dev)
    {
        last_path = mount_path;
        mount_parent = std::filesystem::path(mount_path).parent_path();
        mount_path = std::string(mount_parent);
    }

    // LOG_INFO("last path   {}", last_path);
    // LOG_INFO("mount point {}", mount_path);

    return last_path;
}

TrashDir*
Trash::trash_dir(const std::string& path)
{
    dev_t dev = device(path);

    if (m_trash_dirs.count(dev))
        return m_trash_dirs[dev];

    // on another device cannot use $HOME trashcan
    std::string top_dir = toplevel(path);
    std::string uid = std::to_string(getuid());
    std::string trashPath = top_dir + "/.Trash-" + uid;

    TrashDir* trash_dir = new TrashDir(trashPath, dev);
    m_trash_dirs[dev] = trash_dir;

    return trash_dir;
}

bool
Trash::trash(const std::string& path)
{
    TrashDir* trash_dir = instance()->trash_dir(path);

    if (!trash_dir)
        return false;

    if (std::filesystem::is_symlink(path))
    {
        // TODO std::filesystem::rename does not support symlinks
        // should limit too only regular files and directories;

        // LOG_INFO("Cannot trash symlink: {}", path);
        return false;
    }

    std::string target_name = trash_dir->unique_name(path);
    trash_dir->create_trash_info(path, target_name);
    trash_dir->move(path, target_name);

    // LOG_INFO("moved to trash: {}", path);

    return true;
}

bool
Trash::restore(const std::string& path)
{
    // NOOP
    return true;
}

void
Trash::empty()
{
    // NOOP
}

TrashDir::TrashDir(const std::string& path, dev_t device) : m_path(path), m_device(device)
{
    // LOG_DEBUG("create trash dirs {}", path);

    check_dir_exists(path);
    check_dir_exists(files_path());
    check_dir_exists(info_path());
}

std::string
TrashDir::unique_name(const std::string& path)
{
    std::string filename = std::filesystem::path(path).filename();
    std::string basename = std::filesystem::path(path).stem();
    std::string ext = std::filesystem::path(path).extension();

    std::string to_trash_filename = filename;
    std::string to_trash_path = files_path() + "/" + to_trash_filename;

    if (std::filesystem::exists(to_trash_path))
    {
        for (int i = 1; true; ++i)
        {
            std::string ii = std::to_string(i);
            std::string check_to_trash_filename = basename + "_" + ii + ext;
            std::string check_to_trash_path = files_path() + "/" + check_to_trash_filename;
            if (!std::filesystem::exists(check_to_trash_path))
            {
                to_trash_filename = check_to_trash_filename;
                break;
            }
        }
    }
    return to_trash_filename;
}

void
TrashDir::check_dir_exists(const std::string& path)
{
    if (std::filesystem::is_directory(path))
        return;

    // LOG_INFO("trash mkdir {}", path);
    std::filesystem::create_directories(path);
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

void
TrashDir::create_trash_info(const std::string& path, const std::string& target_name)
{
    std::string trash_info = info_path() + "/" + target_name + ".trashinfo";

    std::time_t t = std::time(nullptr);
    char iso_time[100];
    std::strftime(iso_time, sizeof(iso_time), "%FT%TZ", std::localtime(&t));

    std::ofstream TrashInfoFile(trash_info);
    TrashInfoFile << "[Trash Info]\n";
    TrashInfoFile << "Path=" << path << "\n";
    TrashInfoFile << "DeletionDate=" << iso_time << "\n";
    TrashInfoFile.close();
}

void
TrashDir::move(const std::string& path, const std::string& target_name)
{
    // LOG_INFO("fp {}", files_path());
    // LOG_INFO("ip {}", info_path());
    std::string target_path = files_path() + "/" + target_name;
    std::filesystem::rename(path, target_path);
}
