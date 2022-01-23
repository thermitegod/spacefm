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
        file_name = g_path_get_basename(open_file_name.c_str());
        full_path = open_file_name;
        load = g_key_file_load_from_file(file, open_file_name.c_str(), G_KEY_FILE_NONE, nullptr);
    }
    else
    {
        file_name = open_file_name;
        std::string relative_path = g_build_filename("applications", file_name.c_str(), nullptr);
        char* full_path_c;
        load = g_key_file_load_from_data_dirs(file,
                                              relative_path.c_str(),
                                              &full_path_c,
                                              G_KEY_FILE_NONE,
                                              nullptr);
        if (full_path_c)
            full_path = full_path_c;
        g_free(full_path_c);
    }

    if (load)
    {
        static const std::string desktop_entry = "Desktop Entry";

        // clang-format off
        disp_name = ztd::null_check(g_key_file_get_locale_string(file, desktop_entry.c_str(), "Name", nullptr, nullptr));
        exec = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Exec", nullptr));
        icon_name = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Icon", nullptr));
        path = ztd::null_check(g_key_file_get_string(file, desktop_entry.c_str(), "Path", nullptr));
        terminal = g_key_file_get_boolean(file, desktop_entry.c_str(), "Terminal", nullptr);
        hidden = g_key_file_get_boolean(file, desktop_entry.c_str(), "NoDisplay", nullptr);
        startup = g_key_file_get_boolean(file, desktop_entry.c_str(), "StartupNotify", nullptr);
        // clang-format on
    }
    else
    {
        exec = file_name;
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
    return file_name.c_str();
}

const char*
VFSAppDesktop::get_disp_name()
{
    if (!disp_name.empty())
        return disp_name.c_str();
    return file_name.c_str();
}

const char*
VFSAppDesktop::get_exec()
{
    return exec.c_str();
}

bool
VFSAppDesktop::use_terminal()
{
    return terminal;
}

const char*
VFSAppDesktop::get_full_path()
{
    return full_path.c_str();
}

const char*
VFSAppDesktop::get_icon_name()
{
    return icon_name.c_str();
}

GdkPixbuf*
VFSAppDesktop::get_icon(int size)
{
    GtkIconTheme* theme;
    GdkPixbuf* icon = nullptr;

    if (icon_name.c_str())
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(theme, icon_name.c_str(), size);
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
    if (!exec.empty())
    {
        std::vector<std::string> keys{"%U", "%F"};
        if (ztd::contains(exec, keys))
            return true;
    }
    return false;
}

/*
 * Parse Exec command line of app desktop file, and translate
 * it into a real command which can be passed to g_spawn_command_line_async().
 * file_list is a null-terminated file list containing full
 * paths of the files passed to app.
 * returned char* should be freed when no longer needed.
 */
std::string
VFSAppDesktop::translate_app_exec_to_command_line(GList* file_list)
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    const char* pexec = get_exec();
    char* file;
    GList* l;
    std::string tmp;
    std::string cmd = "";
    bool add_files = false;

    for (; *pexec; ++pexec)
    {
        if (*pexec == '%')
        {
            ++pexec;
            switch (*pexec)
            {
                case 'F':
                case 'U':
                    for (l = file_list; l; l = l->next)
                    {
                        file = (char*)l->data;
                        cmd.append(bash_quote(file));
                        cmd.append(" ");
                    }
                    add_files = true;
                    break;
                case 'f':
                case 'u':
                    if (file_list && file_list->data)
                    {
                        file = (char*)file_list->data;
                        cmd.append(bash_quote(file));
                        add_files = true;
                    }
                    break;
                case 'c':
                    cmd.append(get_disp_name());
                    break;
                case 'i':
                    // Add icon name
                    if (get_icon_name())
                    {
                        cmd.append("--icon ");
                        cmd.append(get_icon_name());
                    }
                    break;
                case 'k':
                    // Location of the desktop file
                    break;
                case '%':
                    cmd.append("%");
                    break;
                case '\0':
                    break;
                default:
                    break;
            }
        }
        else
        {
            // not % escaped part
            cmd.append(fmt::format("{}", (char)*pexec));
        }
    }
    if (!add_files)
    {
        cmd.append(" ");
        for (l = file_list; l; l = l->next)
        {
            file = (char*)l->data;
            cmd.append(bash_quote(file));
            cmd.append(" ");
        }
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
VFSAppDesktop::open_files(GdkScreen* screen, const char* working_dir, GList* file_paths,
                          GError** err)
{
    char* exec = nullptr;
    std::string cmd;
    GList* l;
    char** argv = nullptr;
    int argc = 0;
    const char* sn_desc;

    if (get_exec())
    {
        if (!strchr(get_exec(), '%'))
        { /* No filename parameters */
            exec = g_strconcat(get_exec(), " %f", nullptr);
        }
        else
        {
            exec = g_strdup(get_exec());
        }
    }

    if (exec)
    {
        if (!screen)
            screen = gdk_screen_get_default();

        sn_desc = get_disp_name();
        if (!sn_desc)
            sn_desc = exec;

        if (open_multiple_files())
        {
            cmd = translate_app_exec_to_command_line(file_paths);
            if (!cmd.empty())
            {
                if (use_terminal())
                    exec_in_terminal(sn_desc, !path.empty() ? path : working_dir, cmd);
                else
                {
                    // LOG_DEBUG("Execute: {}", cmd);
                    if (g_shell_parse_argv(cmd.c_str(), &argc, &argv, nullptr))
                    {
                        vfs_exec_on_screen(screen,
                                           !path.empty() ? path.c_str() : working_dir,
                                           argv,
                                           nullptr,
                                           sn_desc,
                                           (GSpawnFlags)VFS_EXEC_DEFAULT_FLAGS,
                                           startup,
                                           err);
                        g_strfreev(argv);
                    }
                }
            }
        }
        else
        {
            // app does not accept multiple files, so run multiple times
            GList* single;
            l = file_paths;
            do
            {
                if (l)
                {
                    // just pass a single file path to translate
                    single = g_list_append(nullptr, l->data);
                }
                else
                {
                    // there are no files being passed, just run once
                    single = nullptr;
                }
                cmd = translate_app_exec_to_command_line(single);
                g_list_free(single);
                if (!cmd.empty())
                {
                    if (use_terminal())
                        exec_in_terminal(sn_desc, !path.empty() ? path : working_dir, cmd);
                    else
                    {
                        // LOG_DEBUG("Execute: {}", cmd);
                        if (g_shell_parse_argv(cmd.c_str(), &argc, &argv, nullptr))
                        {
                            vfs_exec_on_screen(screen,
                                               !path.empty() ? path.c_str() : working_dir,
                                               argv,
                                               nullptr,
                                               sn_desc,
                                               (GSpawnFlags)VFS_EXEC_DEFAULT_FLAGS,
                                               startup,
                                               err);
                            g_strfreev(argv);
                        }
                    }
                }
            } while ((l = l ? l->next : nullptr));
        }
        g_free(exec);
        return true;
    }

    g_set_error(err,
                G_SPAWN_ERROR,
                G_SPAWN_ERROR_FAILED,
                "%s\n\n%s",
                "Command not found",
                get_name());
    return false;
}
