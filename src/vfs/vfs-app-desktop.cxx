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

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-execute.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "logger.hxx"

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

    VFSAppDesktop* app = g_slice_new0(VFSAppDesktop);
    app->ref_inc();

    GKeyFile* file = g_key_file_new();

    if (!file_name)
    {
        g_key_file_free(file);
        return app;
    }

    if (g_path_is_absolute(file_name))
    {
        app->file_name = g_path_get_basename(file_name);
        app->full_path = g_strdup(file_name);
        load = g_key_file_load_from_file(file, file_name, G_KEY_FILE_NONE, nullptr);
    }
    else
    {
        app->file_name = g_strdup(file_name);
        relative_path = g_build_filename("applications", app->file_name, nullptr);
        load = g_key_file_load_from_data_dirs(file,
                                              relative_path,
                                              &app->full_path,
                                              G_KEY_FILE_NONE,
                                              nullptr);
        g_free(relative_path);

        if (!load)
        {
            // some desktop files are in subdirs of data dirs (out of spec)
            if ((app->full_path = mime_type_locate_desktop_file(nullptr, file_name)))
                load = g_key_file_load_from_file(file, app->full_path, G_KEY_FILE_NONE, nullptr);
        }
    }

    if (load)
    {
        app->disp_name =
            g_key_file_get_locale_string(file, desktop_entry_name, "Name", nullptr, nullptr);
        app->comment =
            g_key_file_get_locale_string(file, desktop_entry_name, "Comment", nullptr, nullptr);
        app->exec = g_key_file_get_string(file, desktop_entry_name, "Exec", nullptr);
        app->icon_name = g_key_file_get_string(file, desktop_entry_name, "Icon", nullptr);
        app->terminal = g_key_file_get_boolean(file, desktop_entry_name, "Terminal", nullptr);
        app->hidden = g_key_file_get_boolean(file, desktop_entry_name, "NoDisplay", nullptr);
        app->startup = g_key_file_get_boolean(file, desktop_entry_name, "StartupNotify", nullptr);
        app->path = g_key_file_get_string(file, desktop_entry_name, "Path", nullptr);
    }

    g_key_file_free(file);

    return app;
}

static void
vfs_app_desktop_free(VFSAppDesktop* app)
{
    g_free(app->disp_name);
    g_free(app->comment);
    g_free(app->exec);
    g_free(app->icon_name);
    g_free(app->path);
    g_free(app->full_path);

    g_slice_free(VFSAppDesktop, app);
}

void
vfs_app_desktop_unref(void* data)
{
    VFSAppDesktop* app = static_cast<VFSAppDesktop*>(data);
    app->ref_dec();
    if (app->ref_count() == 0)
        vfs_app_desktop_free(app);
}

const char*
vfs_app_desktop_get_name(VFSAppDesktop* app)
{
    return app->file_name;
}

const char*
vfs_app_desktop_get_disp_name(VFSAppDesktop* app)
{
    if (G_LIKELY(app->disp_name))
        return app->disp_name;
    return app->file_name;
}

const char*
vfs_app_desktop_get_exec(VFSAppDesktop* app)
{
    return app->exec;
}

const char*
vfs_app_desktop_get_icon_name(VFSAppDesktop* app)
{
    return app->icon_name;
}

static GdkPixbuf*
load_icon_file(const char* file_name, int size)
{
    GdkPixbuf* icon = nullptr;
    const char** dirs = (const char**)vfs_system_data_dir();
    const char** dir;
    for (dir = dirs; *dir; ++dir)
    {
        char* file_path = g_build_filename(*dir, "pixmaps", file_name, nullptr);
        icon = gdk_pixbuf_new_from_file_at_scale(file_path, size, size, true, nullptr);
        g_free(file_path);
        if (icon)
            break;
    }
    return icon;
}

GdkPixbuf*
vfs_app_desktop_get_icon(VFSAppDesktop* app, int size, bool use_fallback)
{
    GtkIconTheme* theme;
    char* icon_name = nullptr;
    char* suffix;
    GdkPixbuf* icon = nullptr;

    if (app->icon_name)
    {
        if (g_path_is_absolute(app->icon_name))
        {
            icon = gdk_pixbuf_new_from_file_at_scale(app->icon_name, size, size, true, nullptr);
        }
        else
        {
            theme = gtk_icon_theme_get_default();
            // sfm 1.0.6 changed strchr to strrchr [Teklad]
            suffix = strrchr(app->icon_name, '.');
            if (suffix) /* has file extension, it's a basename of icon file */
            {
                /* try to find it in pixmaps dirs */
                icon = load_icon_file(app->icon_name, size);
                if (G_UNLIKELY(!icon)) /* unfortunately, not found */
                {
                    /* Let's remove the suffix, and see if this name can match an icon
                         in current icon theme */
                    icon_name = g_strndup(app->icon_name, (suffix - app->icon_name));
                    icon = vfs_load_icon(theme, icon_name, size);
                    g_free(icon_name);
                    icon_name = nullptr;
                    if (!icon)
                        // sfm 1.0.6 Try to lookup with full name instead. [Teklad]
                        icon = vfs_load_icon(theme, app->icon_name, size);
                }
            }
            else /* no file extension, it could be an icon name in the icon theme */
            {
                icon = vfs_load_icon(theme, app->icon_name, size);
            }
        }
    }
    if (G_UNLIKELY(!icon) && use_fallback) /* fallback to generic icon */
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(theme, "application-x-executable", size);
        if (G_UNLIKELY(!icon)) /* fallback to generic icon */
        {
            icon = vfs_load_icon(theme, "gnome-mime-application-x-executable", size);
        }
    }
    return icon;
}

bool
vfs_app_desktop_open_multiple_files(VFSAppDesktop* app)
{
    if (app->exec)
    {
        if (strstr(app->exec, "%U") || strstr(app->exec, "%F") || strstr(app->exec, "%N") ||
            strstr(app->exec, "%D"))
            return true;
    }
    return false;
}

bool
vfs_app_desktop_open_in_terminal(VFSAppDesktop* app)
{
    return app->terminal;
}

static bool
vfs_app_desktop_uses_startup_notify(VFSAppDesktop* app)
{
    return app->startup;
}

/*
 * Parse Exec command line of app desktop file, and translate
 * it into a real command which can be passed to g_spawn_command_line_async().
 * file_list is a null-terminated file list containing full
 * paths of the files passed to app.
 * returned char* should be freed when no longer needed.
 */
static char*
translate_app_exec_to_command_line(VFSAppDesktop* app, GList* file_list)
{
    const char* pexec = vfs_app_desktop_get_exec(app);
    char* file;
    GList* l;
    char* tmp;
    GString* cmd = g_string_new("");
    bool add_files = false;

    for (; *pexec; ++pexec)
    {
        if (*pexec == '%')
        {
            ++pexec;
            switch (*pexec)
            {
                /* 0.9.4: Treat %u/%U as %f/%F acceptable for local files per spec at
                 * http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
                 * This seems to be more common behavior among file managers and
                 * some common .desktop files erroneously make this assumption.
                case 'U':
                    for( l = file_list; l; l = l->next )
                    {
                        tmp = g_filename_to_uri( (char*)l->data, nullptr, nullptr );
                        file = g_shell_quote( tmp );
                        g_free( tmp );
                        g_string_append( cmd, file );
                        g_string_append_c( cmd, ' ' );
                        g_free( file );
                    }
                    add_files = true;
                    break;
                case 'u':
                    if( file_list && file_list->data )
                    {
                        file = (char*)file_list->data;
                        tmp = g_filename_to_uri( file, nullptr, nullptr );
                        file = g_shell_quote( tmp );
                        g_free( tmp );
                        g_string_append( cmd, file );
                        g_free( file );
                        add_files = true;
                    }
                    break;
                */
                case 'F':
                case 'N':
                case 'U':
                    for (l = file_list; l; l = l->next)
                    {
                        file = (char*)l->data;
                        tmp = g_shell_quote(file);
                        g_string_append(cmd, tmp);
                        g_string_append_c(cmd, ' ');
                        g_free(tmp);
                    }
                    add_files = true;
                    break;
                case 'f':
                case 'n':
                case 'u':
                    if (file_list && file_list->data)
                    {
                        file = (char*)file_list->data;
                        tmp = g_shell_quote(file);
                        g_string_append(cmd, tmp);
                        g_free(tmp);
                        add_files = true;
                    }
                    break;
                case 'D':
                    for (l = file_list; l; l = l->next)
                    {
                        tmp = g_path_get_dirname((char*)l->data);
                        file = g_shell_quote(tmp);
                        g_free(tmp);
                        g_string_append(cmd, file);
                        g_string_append_c(cmd, ' ');
                        g_free(file);
                    }
                    add_files = true;
                    break;
                case 'd':
                    if (file_list && file_list->data)
                    {
                        tmp = g_path_get_dirname((char*)file_list->data);
                        file = g_shell_quote(tmp);
                        g_free(tmp);
                        g_string_append(cmd, file);
                        g_free(tmp);
                        add_files = true;
                    }
                    break;
                case 'c':
                    g_string_append(cmd, vfs_app_desktop_get_disp_name(app));
                    break;
                case 'i':
                    /* Add icon name */
                    if (vfs_app_desktop_get_icon_name(app))
                    {
                        g_string_append(cmd, "--icon ");
                        g_string_append(cmd, vfs_app_desktop_get_icon_name(app));
                    }
                    break;
                case 'k':
                    /* Location of the desktop file */
                    break;
                case 'v':
                    /* Device name */
                    break;
                case '%':
                    g_string_append_c(cmd, '%');
                    break;
                case '\0':
                    goto _finish;
                    break;
                default:
                    break;
            }
        }
        else /* not % escaped part */
        {
            g_string_append_c(cmd, *pexec);
        }
    }
_finish:
    if (!add_files)
    {
        g_string_append_c(cmd, ' ');
        for (l = file_list; l; l = l->next)
        {
            file = (char*)l->data;
            tmp = g_shell_quote(file);
            g_string_append(cmd, tmp);
            g_string_append_c(cmd, ' ');
            g_free(tmp);
        }
    }

    return g_string_free(cmd, false);
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
vfs_app_desktop_open_files(GdkScreen* screen, const char* working_dir, VFSAppDesktop* app,
                           GList* file_paths, GError** err)
{
    char* exec = nullptr;
    char* cmd;
    GList* l;
    char** argv = nullptr;
    int argc = 0;
    const char* sn_desc;

    if (vfs_app_desktop_get_exec(app))
    {
        if (!strchr(vfs_app_desktop_get_exec(app), '%'))
        { /* No filename parameters */
            exec = g_strconcat(vfs_app_desktop_get_exec(app), " %f", nullptr);
        }
        else
        {
            exec = g_strdup(vfs_app_desktop_get_exec(app));
        }
    }

    if (exec)
    {
        if (!screen)
            screen = gdk_screen_get_default();

        sn_desc = vfs_app_desktop_get_disp_name(app);
        if (!sn_desc)
            sn_desc = exec;

        if (vfs_app_desktop_open_multiple_files(app))
        {
            cmd = translate_app_exec_to_command_line(app, file_paths);
            if (cmd)
            {
                if (vfs_app_desktop_open_in_terminal(app))
                    exec_in_terminal(sn_desc,
                                     app->path && app->path[0] ? app->path : working_dir,
                                     cmd);
                else
                {
                    // LOG_DEBUG("Execute: {}", cmd);
                    if (g_shell_parse_argv(cmd, &argc, &argv, nullptr))
                    {
                        vfs_exec_on_screen(screen,
                                           app->path && app->path[0] ? app->path : working_dir,
                                           argv,
                                           nullptr,
                                           sn_desc,
                                           (GSpawnFlags)VFS_EXEC_DEFAULT_FLAGS,
                                           vfs_app_desktop_uses_startup_notify(app),
                                           err);
                        g_strfreev(argv);
                    }
                }
                g_free(cmd);
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
                cmd = translate_app_exec_to_command_line(app, single);
                g_list_free(single);
                if (cmd)
                {
                    if (vfs_app_desktop_open_in_terminal(app))
                        exec_in_terminal(sn_desc,
                                         app->path && app->path[0] ? app->path : working_dir,
                                         cmd);
                    else
                    {
                        // LOG_DEBUG("Execute: {}", cmd);
                        if (g_shell_parse_argv(cmd, &argc, &argv, nullptr))
                        {
                            vfs_exec_on_screen(screen,
                                               app->path && app->path[0] ? app->path : working_dir,
                                               argv,
                                               nullptr,
                                               sn_desc,
                                               GSpawnFlags(G_SPAWN_SEARCH_PATH |
                                                           G_SPAWN_STDOUT_TO_DEV_NULL |
                                                           G_SPAWN_STDERR_TO_DEV_NULL),
                                               vfs_app_desktop_uses_startup_notify(app),
                                               err);
                            g_strfreev(argv);
                        }
                    }
                    g_free(cmd);
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
                app->file_name);
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
