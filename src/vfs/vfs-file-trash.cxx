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

#include <glib.h>

#include "vfs-file-trash.hxx"
//#include "../logger.hxx"

Trash* Trash::_instance = nullptr;

Trash*
Trash::instance()
{
    if (!_instance)
        _instance = new Trash();

    return _instance;
}

Trash::Trash()
{
    _home_device = device(g_get_home_dir());

    std::string home_trash_dir = g_get_user_data_dir();
    home_trash_dir += "/Trash";

    _home_trash_dir = new TrashDir(home_trash_dir, _home_device);

    _trash_dirs[_home_device] = _home_trash_dir;
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

    if (_trash_dirs.count(dev))
        return _trash_dirs[dev];

    // on another device cannot use $HOME trashcan
    std::string top_dir = toplevel(path);
    std::string uid = std::to_string(getuid());
    std::string trashPath = top_dir + "/.Trash-" + uid;

    struct stat file_stat;
    int result = stat(trashPath.c_str(), &file_stat);

    TrashDir* trash_dir = new TrashDir(trashPath, dev);
    _trash_dirs[dev] = trash_dir;

    return trash_dir;
}

bool
Trash::trash(const std::string& path)
{
    TrashDir* trash_dir = instance()->trash_dir(path);

    if (!trash_dir)
        return false;

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

TrashDir::TrashDir(const std::string& path, dev_t device) : _path(path), _device(device)
{
    // LOG_DEBUG("create trash dirs {}", path);

    check_dir_exists(path, 0700);
    check_dir_exists(files_path(), 0700);
    check_dir_exists(info_path(), 0700);
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
TrashDir::check_dir_exists(const std::string& path, mode_t mode)
{
    if (std::filesystem::is_directory(path))
        return;

    // LOG_INFO("trash mkdir {}", path);
    std::filesystem::create_directories(path);
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
