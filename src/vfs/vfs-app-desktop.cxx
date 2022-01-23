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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-execute.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "utils.hxx"

static const char desktop_entry_name[] = "Desktop Entry";

/*
 * If file_name is not a full path, this function searches default paths
 * for the desktop file.
 */
VFSAppDesktop*
vfs_app_desktop_new(const char* file_name)
{
    bool load;
    char* relative_path;

    VFSAppDesktop* desktop = g_slice_new0(VFSAppDesktop);
    desktop->ref_inc();

    GKeyFile* file = g_key_file_new();

    if (!file_name)
    {
        g_key_file_free(file);
        return desktop;
    }

    if (g_path_is_absolute(file_name))
    {
        desktop->file_name = g_path_get_basename(file_name);
        desktop->full_path = g_strdup(file_name);
        load = g_key_file_load_from_file(file, file_name, G_KEY_FILE_NONE, nullptr);
    }
    else
    {
        desktop->file_name = file_name;
        relative_path = g_build_filename("applications", desktop->file_name.c_str(), nullptr);
        load = g_key_file_load_from_data_dirs(file,
                                              relative_path,
                                              &desktop->full_path,
                                              G_KEY_FILE_NONE,
                                              nullptr);
        g_free(relative_path);

        if (!load)
        {
            // some desktop files are in subdirs of data dirs (out of spec)
            if ((desktop->full_path = mime_type_locate_desktop_file(nullptr, file_name)))
                load =
                    g_key_file_load_from_file(file, desktop->full_path, G_KEY_FILE_NONE, nullptr);
        }
    }

    if (load)
    {
        // clang-format off
        desktop->disp_name = g_key_file_get_locale_string(file, desktop_entry_name, "Name", nullptr, nullptr);
        desktop->exec = g_key_file_get_string(file, desktop_entry_name, "Exec", nullptr);
        desktop->icon_name = g_key_file_get_string(file, desktop_entry_name, "Icon", nullptr);
        desktop->terminal = g_key_file_get_boolean(file, desktop_entry_name, "Terminal", nullptr);
        desktop->hidden = g_key_file_get_boolean(file, desktop_entry_name, "NoDisplay", nullptr);
        desktop->startup = g_key_file_get_boolean(file, desktop_entry_name, "StartupNotify", nullptr);
        desktop->path = g_key_file_get_string(file, desktop_entry_name, "Path", nullptr);
        // clang-format on
    }
    else
    {
        desktop->exec = desktop->file_name;
    }

    g_key_file_free(file);

    return desktop;
}

static void
vfs_app_desktop_free(VFSAppDesktop* desktop)
{
    g_free(desktop->icon_name);
    g_free(desktop->path);
    g_free(desktop->full_path);

    g_slice_free(VFSAppDesktop, desktop);
}

void
vfs_app_desktop_unref(void* data)
{
    VFSAppDesktop* desktop = static_cast<VFSAppDesktop*>(data);
    desktop->ref_dec();
    if (desktop->ref_count() == 0)
        vfs_app_desktop_free(desktop);
}

const char*
vfs_app_desktop_get_name(VFSAppDesktop* desktop)
{
    return desktop->file_name.c_str();
}

const char*
vfs_app_desktop_get_disp_name(VFSAppDesktop* desktop)
{
    if (!desktop->disp_name.empty())
        return desktop->disp_name.c_str();
    return desktop->file_name.c_str();
}

const char*
vfs_app_desktop_get_exec(VFSAppDesktop* desktop)
{
    return desktop->exec.c_str();
}

const char*
vfs_app_desktop_get_icon_name(VFSAppDesktop* desktop)
{
    return desktop->icon_name;
}

GdkPixbuf*
vfs_app_desktop_get_icon(VFSAppDesktop* desktop, int size)
{
    GtkIconTheme* theme;
    GdkPixbuf* icon = nullptr;

    if (desktop->icon_name)
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(theme, desktop->icon_name, size);
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
vfs_app_desktop_open_multiple_files(VFSAppDesktop* desktop)
{
    if (!desktop->exec.empty())
    {
        if (ztd::contains(desktop->exec, "%U") || ztd::contains(desktop->exec, "%F"))
            return true;
    }
    return false;
}

bool
vfs_app_desktop_open_in_terminal(VFSAppDesktop* desktop)
{
    return desktop->terminal;
}

static bool
vfs_app_desktop_uses_startup_notify(VFSAppDesktop* desktop)
{
    return desktop->startup;
}

/*
 * Parse Exec command line of app desktop file, and translate
 * it into a real command which can be passed to g_spawn_command_line_async().
 * file_list is a null-terminated file list containing full
 * paths of the files passed to app.
 * returned char* should be freed when no longer needed.
 */
static std::string
translate_app_exec_to_command_line(VFSAppDesktop* desktop, GList* file_list)
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    const char* pexec = vfs_app_desktop_get_exec(desktop);
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
                    cmd.append(vfs_app_desktop_get_disp_name(desktop));
                    break;
                case 'i':
                    // Add icon name
                    if (vfs_app_desktop_get_icon_name(desktop))
                    {
                        cmd.append("--icon ");
                        cmd.append(vfs_app_desktop_get_icon_name(desktop));
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

static void
exec_in_terminal(const char* app_name, const char* cwd, const char* cmd)
{
    // task
    PtkFileTask* task = ptk_file_exec_new(app_name, cwd, nullptr, nullptr);

    task->task->exec_command = cmd;

    task->task->exec_terminal = true;
    // task->task->exec_keep_terminal = true;  // for test only
    task->task->exec_sync = false;
    task->task->exec_export = false;

    ptk_file_task_run(task);
}

bool
vfs_app_desktop_open_files(GdkScreen* screen, const char* working_dir, VFSAppDesktop* desktop,
                           GList* file_paths, GError** err)
{
    char* exec = nullptr;
    std::string cmd;
    GList* l;
    char** argv = nullptr;
    int argc = 0;
    const char* sn_desc;

    if (vfs_app_desktop_get_exec(desktop))
    {
        if (!strchr(vfs_app_desktop_get_exec(desktop), '%'))
        { /* No filename parameters */
            exec = g_strconcat(vfs_app_desktop_get_exec(desktop), " %f", nullptr);
        }
        else
        {
            exec = g_strdup(vfs_app_desktop_get_exec(desktop));
        }
    }

    if (exec)
    {
        if (!screen)
            screen = gdk_screen_get_default();

        sn_desc = vfs_app_desktop_get_disp_name(desktop);
        if (!sn_desc)
            sn_desc = exec;

        if (vfs_app_desktop_open_multiple_files(desktop))
        {
            cmd = translate_app_exec_to_command_line(desktop, file_paths);
            if (!cmd.empty())
            {
                if (vfs_app_desktop_open_in_terminal(desktop))
                    exec_in_terminal(sn_desc,
                                     desktop->path && desktop->path[0] ? desktop->path
                                                                       : working_dir,
                                     cmd.c_str());
                else
                {
                    // LOG_DEBUG("Execute: {}", cmd);
                    if (g_shell_parse_argv(cmd.c_str(), &argc, &argv, nullptr))
                    {
                        vfs_exec_on_screen(screen,
                                           desktop->path && desktop->path[0] ? desktop->path
                                                                             : working_dir,
                                           argv,
                                           nullptr,
                                           sn_desc,
                                           (GSpawnFlags)VFS_EXEC_DEFAULT_FLAGS,
                                           vfs_app_desktop_uses_startup_notify(desktop),
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
                cmd = translate_app_exec_to_command_line(desktop, single);
                g_list_free(single);
                if (!cmd.empty())
                {
                    if (vfs_app_desktop_open_in_terminal(desktop))
                        exec_in_terminal(sn_desc,
                                         desktop->path && desktop->path[0] ? desktop->path
                                                                           : working_dir,
                                         cmd.c_str());
                    else
                    {
                        // LOG_DEBUG("Execute: {}", cmd);
                        if (g_shell_parse_argv(cmd.c_str(), &argc, &argv, nullptr))
                        {
                            vfs_exec_on_screen(
                                screen,
                                desktop->path && desktop->path[0] ? desktop->path : working_dir,
                                argv,
                                nullptr,
                                sn_desc,
                                GSpawnFlags(G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                                            G_SPAWN_STDERR_TO_DEV_NULL),
                                vfs_app_desktop_uses_startup_notify(desktop),
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
                desktop->file_name.c_str());
    return false;
}

void
VFSAppDesktop::ref_inc()
{
    ++n_ref;
}

void
VFSAppDesktop::ref_dec()
{
    --n_ref;
}

unsigned int
VFSAppDesktop::ref_count()
{
    return n_ref;
}
