/*
 *  C Implementation: vfs-app-desktop
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <string>
#include <string_view>

#include <vector>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-execute.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "utils.hxx"

VFSAppDesktop::VFSAppDesktop(const std::string& open_file_name)
{
    // LOG_INFO("VFSAppDesktop constructor");

    bool load;

    GKeyFile* file = g_key_file_new();

    if (g_path_is_absolute(open_file_name.c_str()))
    {
        m_file_name = g_path_get_basename(open_file_name.c_str());
        m_full_path = open_file_name;
        load = g_key_file_load_from_file(file, open_file_name.c_str(), G_KEY_FILE_NONE, nullptr);
    }
    else
    {
        m_file_name = open_file_name;
        std::string relative_path = g_build_filename("applications", m_file_name.c_str(), nullptr);
        char* full_path_c;
        load = g_key_file_load_from_data_dirs(file,
                                              relative_path.c_str(),
                                              &full_path_c,
                                              G_KEY_FILE_NONE,
                                              nullptr);
        if (full_path_c)
            m_full_path = full_path_c;
        g_free(full_path_c);
    }

    if (load)
    {
        static const std::string desktop_entry = "Desktop Entry";

        // clang-format off
        m_disp_name = ztd::null_check(g_key_file_get_locale_string(file, desktop_entry.c_str(), "Name", nullptr, nullptr));
        m_exec = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Exec", nullptr));
        m_icon_name = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Icon", nullptr));
        m_path = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Path", nullptr));
        m_terminal = g_key_file_get_boolean(file, desktop_entry.c_str(), "Terminal", nullptr);
        m_hidden = g_key_file_get_boolean(file, desktop_entry.c_str(), "NoDisplay", nullptr);
        m_startup = g_key_file_get_boolean(file, desktop_entry.c_str(), "StartupNotify", nullptr);
        // clang-format on
    }
    else
    {
        m_exec = m_file_name;
    }

    g_key_file_free(file);
}

VFSAppDesktop::~VFSAppDesktop()
{
    // LOG_INFO("VFSAppDesktop destructor");
}

const char*
VFSAppDesktop::get_name()
{
    return m_file_name.c_str();
}

const char*
VFSAppDesktop::get_disp_name()
{
    if (!m_disp_name.empty())
        return m_disp_name.c_str();
    return m_file_name.c_str();
}

const char*
VFSAppDesktop::get_exec()
{
    return m_exec.c_str();
}

bool
VFSAppDesktop::use_terminal()
{
    return m_terminal;
}

const char*
VFSAppDesktop::get_full_path()
{
    return m_full_path.c_str();
}

const char*
VFSAppDesktop::get_icon_name()
{
    return m_icon_name.c_str();
}

GdkPixbuf*
VFSAppDesktop::get_icon(int size)
{
    GtkIconTheme* theme;
    GdkPixbuf* icon = nullptr;

    if (m_icon_name.c_str())
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(theme, m_icon_name.c_str(), size);
    }

    // fallback to generic icon
    if (G_UNLIKELY(!icon))
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(theme, "application-x-executable", size);
        // fallback to generic icon
        if (G_UNLIKELY(!icon))
            icon = vfs_load_icon(theme, "gnome-mime-application-x-executable", size);
    }
    return icon;
}

bool
VFSAppDesktop::open_multiple_files()
{
    if (!m_exec.empty())
    {
        std::vector<std::string> keys{"%U", "%F"};
        if (ztd::contains(m_exec, keys))
            return true;
    }
    return false;
}

std::string
VFSAppDesktop::translate_app_exec_to_command_line(std::vector<std::string>& file_list)
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::string cmd = get_exec();

    bool add_files = false;

    std::vector<std::string> open_files_keys{"%F", "%U"};
    if (ztd::contains(cmd, open_files_keys))
    {
        std::string tmp;
        for (std::string file: file_list)
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

    std::vector<std::string> open_file_keys{"%f", "%u"};
    if (ztd::contains(cmd, open_file_keys))
    {
        std::string tmp;
        for (std::string file: file_list)
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
        std::string icon = fmt::format("--icon {}", get_icon_name());
        cmd = ztd::replace(cmd, "%i", icon);
    }

    if (!add_files)
    {
        std::string tmp;

        cmd.append(" ");
        for (std::string file: file_list)
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
                                const std::string& cmd)
{
    // task
    PtkFileTask* task = ptk_file_exec_new(app_name.c_str(), cwd.c_str(), nullptr, nullptr);

    task->task->exec_command = cmd;

    task->task->exec_terminal = true;
    // task->task->exec_keep_terminal = true;  // for test only
    task->task->exec_sync = false;
    task->task->exec_export = false;

    ptk_file_task_run(task);
}

bool
VFSAppDesktop::open_files(GdkScreen* screen, const std::string& working_dir,
                          std::vector<std::string>& file_paths, GError** err)
{
    if (!get_exec())
    {
        g_set_error(err,
                    G_SPAWN_ERROR,
                    G_SPAWN_ERROR_FAILED,
                    "%s\n\n%s",
                    "Command not found",
                    get_name());
        return false;
    }

    if (!screen)
        screen = gdk_screen_get_default();

    if (open_multiple_files())
    {
        exec_desktop(screen, working_dir, file_paths, err);
    }
    else
    {
        // app does not accept multiple files, so run multiple times
        for (std::string open_file: file_paths)
        {
            std::vector<std::string> open_files;
            open_files.push_back(open_file);

            exec_desktop(screen, working_dir, open_files, err);
        }
    }
    return true;
}

void
VFSAppDesktop::exec_desktop(GdkScreen* screen, const std::string& working_dir,
                            std::vector<std::string>& file_paths, GError** err)
{
    std::string cmd = translate_app_exec_to_command_line(file_paths);
    std::string sn_desc = get_disp_name();

    if (cmd.empty())
        return;

    // LOG_DEBUG("Execute: {}", cmd);

    if (use_terminal())
    {
        exec_in_terminal(sn_desc, !m_path.empty() ? m_path : working_dir, cmd);
    }
    else
    {
        char** argv = nullptr;
        int argc = 0;
        if (g_shell_parse_argv(cmd.c_str(), &argc, &argv, nullptr))
        {
            vfs_exec_on_screen(screen,
                               !m_path.empty() ? m_path.c_str() : working_dir.c_str(),
                               argv,
                               nullptr,
                               sn_desc.c_str(),
                               GSpawnFlags(VFS_EXEC_DEFAULT_FLAGS),
                               m_startup,
                               err);
            g_strfreev(argv);
        }
    }
}
