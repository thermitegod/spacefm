/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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
#include <string_view>

#include <array>
#include <vector>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/keyfile.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "utils.hxx"

VFSAppDesktop::VFSAppDesktop(const std::string& open_file_name) noexcept
{
    // LOG_INFO("VFSAppDesktop constructor");

    bool load;

    const auto kf = Glib::KeyFile::create();

    if (Glib::path_is_absolute(open_file_name))
    {
        this->file_name = Glib::path_get_basename(open_file_name);
        this->full_path = open_file_name;
        load = kf->load_from_file(open_file_name, Glib::KeyFile::Flags::NONE);
    }
    else
    {
        this->file_name = open_file_name;
        const std::string relative_path = Glib::build_filename("applications", this->file_name);
        load = kf->load_from_data_dirs(relative_path, this->full_path, Glib::KeyFile::Flags::NONE);
    }

    if (load)
    {
        static const std::string desktop_entry = "Desktop Entry";
        Glib::ustring g_disp_name, g_exec, g_icon_name, g_path;

        try
        {
            g_disp_name = kf->get_string(desktop_entry, "Name");
            if (!g_disp_name.empty())
                this->disp_name = g_disp_name;
        }
        catch (Glib::KeyFileError)
        {
            this->disp_name = "";
        }

        try
        {
            g_exec = kf->get_string(desktop_entry, "Exec");
            if (!g_exec.empty())
                this->exec = g_exec;
        }
        catch (Glib::KeyFileError)
        {
            this->exec = "";
        }

        try
        {
            g_icon_name = kf->get_string(desktop_entry, "Icon");
            if (!g_icon_name.empty())
                this->icon_name = g_icon_name;
        }
        catch (Glib::KeyFileError)
        {
            this->icon_name = "";
        }

        try
        {
            g_path = kf->get_string(desktop_entry, "Path");
            if (!g_path.empty())
                this->path = g_path;
        }
        catch (Glib::KeyFileError)
        {
            this->path = "";
        }

        try
        {
            this->terminal = kf->get_boolean(desktop_entry, "Terminal");
        }
        catch (Glib::KeyFileError)
        {
            this->terminal = false;
        }
    }
    else
    {
        this->exec = this->file_name;
    }
}

VFSAppDesktop::~VFSAppDesktop() noexcept
{
    // LOG_INFO("VFSAppDesktop destructor");
}

const char*
VFSAppDesktop::get_name() noexcept
{
    return this->file_name.c_str();
}

const char*
VFSAppDesktop::get_disp_name() noexcept
{
    if (!this->disp_name.empty())
        return this->disp_name.c_str();
    return this->file_name.c_str();
}

const char*
VFSAppDesktop::get_exec() noexcept
{
    return this->exec.c_str();
}

bool
VFSAppDesktop::use_terminal() noexcept
{
    return this->terminal;
}

const char*
VFSAppDesktop::get_full_path() noexcept
{
    return this->full_path.c_str();
}

const char*
VFSAppDesktop::get_icon_name() noexcept
{
    return this->icon_name.c_str();
}

GdkPixbuf*
VFSAppDesktop::get_icon(int size) noexcept
{
    GdkPixbuf* icon = nullptr;

    if (this->icon_name.c_str())
    {
        icon = vfs_load_icon(this->icon_name.c_str(), size);
    }

    // fallback to generic icon
    if (!icon)
    {
        icon = vfs_load_icon("application-x-executable", size);
        // fallback to generic icon
        if (!icon)
            icon = vfs_load_icon("gnome-mime-application-x-executable", size);
    }
    return icon;
}

bool
VFSAppDesktop::open_multiple_files() noexcept
{
    if (this->exec.empty())
        return false;

    std::array<std::string, 2> keys{"%U", "%F"};
    if (ztd::contains(this->exec, keys))
        return true;
    return false;
}

const std::string
VFSAppDesktop::translate_app_exec_to_command_line(std::vector<std::string>& file_list) noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::string cmd = get_exec();

    bool add_files = false;

    std::array<std::string, 2> open_files_keys{"%F", "%U"};
    if (ztd::contains(cmd, open_files_keys))
    {
        std::string tmp;
        for (const std::string& file: file_list)
        {
            tmp.append(bash_quote(file));
            tmp.append(" ");
        }
        if (ztd::contains(cmd, open_files_keys[0]))
            cmd = ztd::replace(cmd, open_files_keys[0], tmp);
        if (ztd::contains(cmd, open_files_keys[1]))
            cmd = ztd::replace(cmd, open_files_keys[1], tmp);

        add_files = true;
    }

    std::array<std::string, 2> open_file_keys{"%f", "%u"};
    if (ztd::contains(cmd, open_file_keys))
    {
        std::string tmp;
        for (const std::string& file: file_list)
        {
            tmp.append(bash_quote(file));
        }
        if (ztd::contains(cmd, open_file_keys[0]))
            cmd = ztd::replace(cmd, open_file_keys[0], tmp);
        if (ztd::contains(cmd, open_file_keys[1]))
            cmd = ztd::replace(cmd, open_file_keys[1], tmp);

        add_files = true;
    }

    if (ztd::contains(cmd, "%c"))
    {
        cmd = ztd::replace(cmd, "%c", get_disp_name());
    }

    if (ztd::contains(cmd, "%i"))
    {
        const std::string icon = fmt::format("--icon {}", get_icon_name());
        cmd = ztd::replace(cmd, "%i", icon);
    }

    if (!add_files)
    {
        std::string tmp;

        cmd.append(" ");
        for (const std::string& file: file_list)
        {
            tmp.append(bash_quote(file));
            tmp.append(" ");
        }
        cmd.append(tmp);
    }

    return cmd;
}

void
VFSAppDesktop::exec_in_terminal(const std::string& app_name, const std::string& cwd,
                                const std::string& cmd) noexcept
{
    // task
    PtkFileTask* ptask = ptk_file_exec_new(app_name, cwd.c_str(), nullptr, nullptr);

    ptask->task->exec_command = cmd;

    ptask->task->exec_terminal = true;
    // ptask->task->exec_keep_terminal = true;  // for test only
    ptask->task->exec_sync = false;
    ptask->task->exec_export = false;

    ptk_file_task_run(ptask);
}

bool
VFSAppDesktop::open_files(const std::string& working_dir, std::vector<std::string>& file_paths)
{
    if (!get_exec())
    {
        const std::string msg = fmt::format("Command not found\n\n{}", get_name());
        throw VFSAppDesktopException(msg);
    }

    if (open_multiple_files())
    {
        exec_desktop(working_dir, file_paths);
    }
    else
    {
        // app does not accept multiple files, so run multiple times
        for (const std::string& open_file: file_paths)
        {
            std::vector<std::string> open_files;
            open_files.push_back(open_file);

            exec_desktop(working_dir, open_files);
        }
    }
    return true;
}

void
VFSAppDesktop::exec_desktop(const std::string& working_dir,
                            std::vector<std::string>& file_paths) noexcept
{
    const std::string cmd = translate_app_exec_to_command_line(file_paths);
    if (cmd.empty())
        return;

    // LOG_DEBUG("Execute: {}", cmd);

    if (use_terminal())
    {
        const std::string app_name = get_disp_name();
        exec_in_terminal(app_name, !this->path.empty() ? this->path : working_dir, cmd);
    }
    else
    {
        Glib::spawn_command_line_async(cmd);
    }
}
