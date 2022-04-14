/**
 * Copyright (C) 2013-2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2007 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

/*
 * This file handles default applications for MIME types.  For changes it
 * makes to mimeapps.list, it is fully compliant with Freedeskop's:
 *   Association between MIME types and applications 1.0.1
 *   http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 * However, for reading the hierarchy and determining default and associated
 * applications, it uses a best-guess algorithm for better performance and
 * compatibility with older systems, and is NOT fully spec compliant.
 */

#include <string>
#include <filesystem>

#include <vector>

#include <unistd.h>
#include <fcntl.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "utils.hxx"

#include "mime-action.hxx"

static bool
save_to_file(const char* path, const char* data, long len)
{
    int fd = creat(path, 0644);
    if (fd == -1)
        return false;

    if (write(fd, data, len) == -1)
    {
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

static const char group_desktop[] = "Desktop Entry";
static const char key_mime_type[] = "MimeType";

typedef char* (*DataDirFunc)(const char* dir, const char* mime_type, void* user_data);

static char*
data_dir_foreach(DataDirFunc func, const char* mime_type, void* user_data)
{
    char* ret = nullptr;

    std::string dir;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    if ((ret = func(vfs_user_config_dir().c_str(), mime_type, user_data)))
        return ret;

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    dir = Glib::build_filename(vfs_user_data_dir(), "applications");
    if ((ret = func(dir.c_str(), mime_type, user_data)))
        return ret;

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (std::string sys_dir: vfs_system_data_dir())
    {
        dir = Glib::build_filename(sys_dir, "applications");
        if ((ret = func(dir.c_str(), mime_type, user_data)))
            return ret;
    }

    return ret;
}

static char*
apps_dir_foreach(DataDirFunc func, const char* mime_type, void* user_data)
{
    char* ret = nullptr;

    std::string dir = vfs_user_data_dir();

    if ((ret = func(dir.c_str(), mime_type, user_data)))
        return ret;

    for (std::string sys_dir: vfs_system_data_dir())
    {
        if ((ret = func(sys_dir.c_str(), mime_type, user_data)))
            return ret;
    }
    return ret;
}

static void
update_desktop_database()
{
    std::string path = Glib::build_filename(vfs_user_data_dir(), "applications");
    std::string command = fmt::format("update-desktop-database {}", path);
    print_command(command);
    Glib::spawn_command_line_sync(command);
}

static int
strv_index(char** strv, const char* str)
{
    if (G_LIKELY(strv && str))
    {
        char** p;
        for (p = strv; *p; ++p)
        {
            if (!strcmp(*p, str))
                return (p - strv);
        }
    }
    return -1;
}

/* Determine removed associations for this type */
static void
remove_actions(const char* type, GArray* actions)
{ // sfm 0.7.7+ added
    // LOG_INFO("remove_actions( {} )", type);
    GKeyFile* file = g_key_file_new();

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    std::string path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");
    if (!g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, nullptr))
    {
        // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
        path = Glib::build_filename(vfs_user_data_dir(), "applications/mimeapps.list");
        if (!g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, nullptr))
        {
            g_key_file_free(file);
            return;
        }
    }

    unsigned long n_removed = 0;
    char** removed =
        g_key_file_get_string_list(file, "Removed Associations", type, &n_removed, nullptr);
    if (removed)
    {
        unsigned long r;
        for (r = 0; r < n_removed; ++r)
        {
            g_strstrip(removed[r]);
            // LOG_INFO("    {}", removed[r]);
            int i = strv_index((char**)actions->data, removed[r]);
            if (i != -1)
            {
                // LOG_INFO("        ACTION-REMOVED");
                g_array_remove_index(actions, i);
            }
        }
    }
    g_strfreev(removed);
    g_key_file_free(file);
}

/*
 * Get applications associated with this mime-type
 *
 * This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 */
static char*
get_actions(const char* dir, const char* type, GArray* actions)
{
    // LOG_INFO("get_actions( {}, {} )\n", dir, type);
    char** removed = nullptr;

    const char* names[] = {"mimeapps.list", "mimeinfo.cache"};
    const char* groups[] = {"Default Applications", "Added Associations", "MIME Cache"};
    // LOG_INFO("get_actions( {}/, {} )", dir, type);
    for (unsigned int n = 0; n < G_N_ELEMENTS(names); n++)
    {
        char* path = g_build_filename(dir, names[n], nullptr);
        // LOG_INFO( "    {}", path);
        GKeyFile* file = g_key_file_new();
        bool opened = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, nullptr);
        g_free(path);
        if (G_LIKELY(opened))
        {
            unsigned long n_removed = 0;
            if (n == 0)
            {
                // get removed associations in this dir
                removed = g_key_file_get_string_list(file,
                                                     "Removed Associations",
                                                     type,
                                                     &n_removed,
                                                     nullptr);
            }
            // mimeinfo.cache has only MIME Cache; others don't have it
            int k;
            for (k = (n == 0 ? 0 : 2); k < (n == 0 ? 2 : 3); k++)
            {
                // LOG_INFO("        {} [{}]", groups[k], k);
                unsigned long n_apps = 0;
                bool is_removed;
                char** apps = g_key_file_get_string_list(file, groups[k], type, &n_apps, nullptr);
                unsigned long i;
                for (i = 0; i < n_apps; ++i)
                {
                    g_strstrip(apps[i]);
                    // LOG_INFO("            {}", apps[i]);
                    // check if removed
                    is_removed = false;
                    if (removed && n > 0)
                    {
                        unsigned long r;
                        for (r = 0; r < n_removed; ++r)
                        {
                            g_strstrip(removed[r]);
                            if (!strcmp(removed[r], apps[i]))
                            {
                                // LOG_INFO("                REMOVED");
                                is_removed = true;
                                break;
                            }
                        }
                    }
                    if (!is_removed && -1 == strv_index((char**)actions->data, apps[i]))
                    {
                        /* check for app existence */
                        path = mime_type_locate_desktop_file(nullptr, apps[i]);
                        if (G_LIKELY(path))
                        {
                            // LOG_INFO("                EXISTS");
                            g_array_append_val(actions, apps[i]);
                            g_free(path);
                        }
                        else
                        {
                            // LOG_INFO("                MISSING");
                            g_free(apps[i]);
                        }
                        apps[i] = nullptr; /* steal the string */
                    }
                    else
                    {
                        g_free(apps[i]);
                        apps[i] = nullptr;
                    }
                }
                /* don't call g_strfreev since all strings in the array were
                 * stolen or freed. */
                g_free(apps);
            }
        }
        g_key_file_free(file);
        if (ztd::same(dir, vfs_user_config_dir()))
            break; // no mimeinfo.cache in ~/.config
    }
    g_strfreev(removed);
    return nullptr; /* return nullptr so the for_each operation doesn't stop. */
}

/*
 *  Get a list of applications supporting this mime-type
 * The returned string array was newly allocated, and should be
 * freed with g_strfreev() when no longer used.
 */
char**
mime_type_get_actions(const char* type)
{
    GArray* actions = g_array_sized_new(true, false, sizeof(char*), 10);
    char* default_app = nullptr;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */
    data_dir_foreach((DataDirFunc)get_actions, type, actions);

    /* remove actions for this file type */ // sfm
    remove_actions(type, actions);

    /* ensure default app is in the list */
    if (G_LIKELY((default_app = mime_type_get_default_action(type))))
    {
        int i = strv_index((char**)actions->data, default_app);
        if (i == -1) /* default app is not in the list, add it! */
        {
            g_array_prepend_val(actions, default_app);
        }
        else /* default app is in the list, move it to the first. */
        {
            if (i != 0)
            {
                char** pdata = (char**)actions->data;
                char* tmp = pdata[i];
                g_array_remove_index(actions, i);
                g_array_prepend_val(actions, tmp);
            }
            g_free(default_app);
        }
    }
    return (char**)g_array_free(actions, actions->len == 0);
}

/*
 * NOTE:
 * This API is very time consuming, but unfortunately, due to the damn poor design of
 * Freedesktop.org spec, all the insane checks here are necessary.  Sigh...  :-(
 */
static bool
mime_type_has_action(const char* type, const char* desktop_id)
{
    char* cmd = nullptr;
    char* name = nullptr;
    bool found = false;
    bool is_desktop = g_str_has_suffix(desktop_id, ".desktop");

    if (is_desktop)
    {
        char** types;
        GKeyFile* kf = g_key_file_new();
        char* filename = mime_type_locate_desktop_file(nullptr, desktop_id);
        if (filename && g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, nullptr))
        {
            types = g_key_file_get_string_list(kf, group_desktop, key_mime_type, nullptr, nullptr);
            if (-1 != strv_index(types, type))
            {
                /* our mime-type is already found in the desktop file. no further check is needed */
                found = true;
            }
            g_strfreev(types);

            if (!found) /* get the content of desktop file for comparison */
            {
                cmd = g_key_file_get_string(kf, group_desktop, "Exec", nullptr);
                name = g_key_file_get_string(kf, group_desktop, "Name", nullptr);
            }
        }
        g_free(filename);
        g_key_file_free(kf);
    }
    else
    {
        cmd = (char*)desktop_id;
    }

    char** actions = mime_type_get_actions(type);
    if (actions)
    {
        char** action;
        for (action = actions; !found && *action; ++action)
        {
            /* Try to match directly by desktop_id first */
            if (is_desktop && !strcmp(*action, desktop_id))
            {
                found = true;
            }
            else /* Then, try to match by "Exec" and "Name" keys */
            {
                char* name2 = nullptr;
                char* cmd2 = nullptr;
                char* filename = nullptr;
                GKeyFile* kf = g_key_file_new();
                filename = mime_type_locate_desktop_file(nullptr, *action);
                if (filename && g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, nullptr))
                {
                    cmd2 = g_key_file_get_string(kf, group_desktop, "Exec", nullptr);
                    if (cmd && cmd2 && !strcmp(cmd, cmd2)) /* 2 desktop files have same "Exec" */
                    {
                        if (is_desktop)
                        {
                            name2 = g_key_file_get_string(kf, group_desktop, "Name", nullptr);
                            /* Then, check if the "Name" keys of 2 desktop files are the same. */
                            if (name && name2 && !strcmp(name, name2))
                            {
                                /* Both "Exec" and "Name" keys of the 2 desktop files are
                                 *  totally the same. So, despite having different desktop id
                                 *  They actually refer to the same application. */
                                found = true;
                            }
                            g_free(name2);
                        }
                        else
                            found = true;
                    }
                }
                g_free(filename);
                g_free(cmd2);
                g_key_file_free(kf);
            }
        }
        g_strfreev(actions);
    }
    if (is_desktop)
    {
        g_free(cmd);
        g_free(name);
    }
    return found;
}

static char*
make_custom_desktop_file(const char* desktop_id, const char* mime_type)
{
    char* name = nullptr;
    char* cust_template = nullptr;
    char* cust = nullptr;
    char* file_content = nullptr;
    unsigned long len = 0;

    if (G_LIKELY(g_str_has_suffix(desktop_id, ".desktop")))
    {
        GKeyFile* kf = g_key_file_new();
        name = mime_type_locate_desktop_file(nullptr, desktop_id);
        if (G_UNLIKELY(!name ||
                       !g_key_file_load_from_file(kf, name, G_KEY_FILE_KEEP_TRANSLATIONS, nullptr)))
        {
            g_free(name);
            return nullptr; /* not a valid desktop file */
        }
        g_free(name);
        /*
                FIXME: If the source desktop_id refers to a custom desktop file, and
                            value of the MimeType key equals to our mime-type, there is no
                            need to generate a new desktop file.
                if( G_UNLIKELY( is_custom_desktop_file( desktop_id ) ) )
                {
                }
        */
        /* set our mime-type */
        g_key_file_set_string_list(kf, group_desktop, key_mime_type, &mime_type, 1);
        /* store id of original desktop file, for future use. */
        g_key_file_set_string(kf, group_desktop, "X-MimeType-Derived", desktop_id);
        g_key_file_set_string(kf, group_desktop, "NoDisplay", "true");

        name = g_strndup(desktop_id, strlen(desktop_id) - 8);
        cust_template = g_strdup_printf("%s-usercustom-%%d.desktop", name);
        g_free(name);

        file_content = g_key_file_to_data(kf, &len, nullptr);
        g_key_file_free(kf);
    }
    else /* it's not a desktop_id, but a command */
    {
        char* p;
        const char file_templ[] = "[Desktop Entry]\n"
                                  "Encoding=UTF-8\n"
                                  "Name=%s\n"
                                  "Exec=%s\n"
                                  "MimeType=%s\n"
                                  "Icon=exec\n"
                                  "NoDisplay=true\n"; /* FIXME: Terminal? */
        /* Make a user-created desktop file for the command */
        name = g_path_get_basename(desktop_id);
        if ((p = strchr(name, ' '))) /* FIXME: skip command line arguments. is this safe? */
            *p = '\0';
        file_content = g_strdup_printf(file_templ, name, desktop_id, mime_type);
        len = strlen(file_content);
        cust_template = g_strdup_printf("%s-usercreated-%%d.desktop", name);
        g_free(name);
    }

    /* generate unique file name */
    std::string dir = Glib::build_filename(vfs_user_data_dir(), "applications");
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    std::string path;
    unsigned int i;
    for (i = 0;; ++i)
    {
        /* generate the basename */
        cust = g_strdup_printf(cust_template, i);
        path = Glib::build_filename(dir, cust); /* test if the filename already exists */
        if (std::filesystem::exists(path))
            g_free(cust);
        else /* this generated filename can be used */
            break;
    }

    save_to_file(path.c_str(), file_content, len);

    /* execute update-desktop-database" to update mimeinfo.cache */
    update_desktop_database();

    return cust;
}

/*
 * Add an applications used to open this mime-type
 * desktop_id is the name of *.desktop file.
 *
 * custom_desktop: used to store name of the newly created user-custom desktop file, can be nullptr.
 */
void
mime_type_add_action(const char* type, const char* desktop_id, char** custom_desktop)
{
    if (mime_type_has_action(type, desktop_id))
    {
        if (custom_desktop)
            *custom_desktop = g_strdup(desktop_id);
        return;
    }

    char* cust = make_custom_desktop_file(desktop_id, type);
    if (custom_desktop)
        *custom_desktop = cust;
    else
        g_free(cust);
}

static char*
_locate_desktop_file_recursive(const char* path, const char* desktop_id, bool first)
{ // if first is true, just search for subdirs not desktop_id (already searched)
    const char* name;

    GDir* dir = g_dir_open(path, 0, nullptr);
    if (!dir)
        return nullptr;

    char* found = nullptr;
    while ((name = g_dir_read_name(dir)))
    {
        char* sub_path = g_build_filename(path, name, nullptr);
        if (std::filesystem::is_directory(sub_path))
        {
            if ((found = _locate_desktop_file_recursive(sub_path, desktop_id, false)))
            {
                g_free(sub_path);
                break;
            }
        }
        else if (!first && !strcmp(name, desktop_id) && std::filesystem::is_regular_file(sub_path))
        {
            found = sub_path;
            break;
        }
        g_free(sub_path);
    }
    g_dir_close(dir);
    return found;
}

static char*
_locate_desktop_file(const char* dir, const char* unused, const void* desktop_id)
{ // sfm 0.7.8 modified + 0.8.7 modified
    (void)unused;
    bool found = false;

    char* path = g_build_filename(dir, "applications", (const char*)desktop_id, nullptr);

    char* sep = (char*)strchr((const char*)desktop_id, '-');
    if (sep)
        sep = strrchr(path, '-');

    do
    {
        if (std::filesystem::is_regular_file(path))
        {
            found = true;
            break;
        }
        if (sep)
        {
            *sep = '/';
            sep = strchr(sep + 1, '-');
        }
        else
            break;
    } while (!found);

    if (found)
        return path;
    g_free(path);

    // sfm 0.8.7 some desktop files listed by the app chooser are in subdirs
    path = g_build_filename(dir, "applications", nullptr);
    sep = _locate_desktop_file_recursive(path, (const char*)desktop_id, true);
    g_free(path);
    return sep;
}

char*
mime_type_locate_desktop_file(const char* dir, const char* desktop_id)
{
    if (dir)
        return _locate_desktop_file(dir, nullptr, (void*)desktop_id);
    return apps_dir_foreach((DataDirFunc)_locate_desktop_file, nullptr, (void*)desktop_id);
}

static char*
get_default_action(const char* dir, const char* type, void* user_data)
{
    (void)user_data;
    // LOG_INFO("get_default_action( {}, {} )", dir, type);
    // search these files in dir for the first existing default app
    const char* names[] = {"mimeapps.list", "defaults.list"};
    const char* groups[] = {"Default Applications", "Added Associations"};

    for (unsigned int n = 0; n < G_N_ELEMENTS(names); n++)
    {
        char* path = g_build_filename(dir, names[n], nullptr);
        // LOG_INFO("    path = {}", path);
        GKeyFile* file = g_key_file_new();
        bool opened = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, nullptr);
        g_free(path);
        if (opened)
        {
            for (unsigned int k = 0; k < G_N_ELEMENTS(groups); k++)
            {
                unsigned long n_apps;
                char** apps = g_key_file_get_string_list(file, groups[k], type, &n_apps, nullptr);
                if (apps)
                {
                    unsigned long i;
                    for (i = 0; i < n_apps; ++i)
                    {
                        g_strstrip(apps[i]);
                        if (apps[i][0] != '\0')
                        {
                            // LOG_INFO("        {}", apps[i]);
                            if ((path = mime_type_locate_desktop_file(nullptr, apps[i])))
                            {
                                // LOG_INFO("            EXISTS");
                                g_free(path);
                                path = g_strdup(apps[i]);
                                g_strfreev(apps);
                                g_key_file_free(file);
                                return path;
                            }
                        }
                    }
                    g_strfreev(apps);
                }
                if (n == 1)
                    break; // defaults.list doesn't have Added Associations
            }
        }
        g_key_file_free(file);
        if (ztd::same(dir, vfs_user_config_dir()))
            break; // no defaults.list in ~/.config
    }
    return nullptr;
}

/*
 * Get default applications used to open this mime-type
 *
 * The returned string was newly allocated, and should be freed when no longer
 * used.  If nullptr is returned, that means a default app is not set for this
 * mime-type.  This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 * The old defaults.list is also checked.
 */
char*
mime_type_get_default_action(const char* type)
{
    /* FIXME: need to check parent types if default action of current type is not set. */
    return data_dir_foreach((DataDirFunc)get_default_action, type, nullptr);
}

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     MIME_TYPE_ACTION_DEFAULT - make desktop_id the default app
 *     MIME_TYPE_ACTION_APPEND  - add desktop_id to Default and Added apps
 *     MIME_TYPE_ACTION_REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void
mime_type_update_association(const char* type, const char* desktop_id, int action)
{
    const char* groups[] = {"Default Applications", "Added Associations", "Removed Associations"};
    bool data_changed = false;

    if (!(type && type[0] != '\0' && desktop_id && desktop_id[0] != '\0'))
    {
        LOG_WARN("mime_type_update_association invalid type or desktop_id");
        return;
    }
    if (action > MIME_TYPE_ACTION_REMOVE || action < MIME_TYPE_ACTION_DEFAULT)
    {
        LOG_WARN("mime_type_update_association invalid action");
        return;
    }

    // Load current mimeapps.list content, if available
    GKeyFile* file = g_key_file_new();
    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    std::string path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");
    g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, nullptr);

    for (unsigned int k = 0; k < G_N_ELEMENTS(groups); k++)
    {
        char* str;
        char* new_action = nullptr;
        bool is_present = false;
        unsigned long n_apps;
        char** apps = g_key_file_get_string_list(file, groups[k], type, &n_apps, nullptr);
        if (apps)
        {
            unsigned long i;
            for (i = 0; i < n_apps; ++i)
            {
                g_strstrip(apps[i]);
                if (apps[i][0] != '\0')
                {
                    if (!strcmp(apps[i], desktop_id))
                    {
                        switch (action)
                        {
                            case MIME_TYPE_ACTION_DEFAULT:
                                // found desktop_id already in groups[k] list
                                if (k < 2)
                                {
                                    // Default Applications or Added Associations
                                    if (i == 0)
                                    {
                                        // is already first - skip change
                                        is_present = true;
                                        break;
                                    }
                                    // in later position - remove it
                                    continue;
                                }
                                else
                                {
                                    // Removed Associations - remove it
                                    is_present = true;
                                    continue;
                                }
                                break;
                            case MIME_TYPE_ACTION_APPEND:
                                if (k < 2)
                                {
                                    // Default or Added - already present, skip change
                                    is_present = true;
                                    break;
                                }
                                else
                                {
                                    // Removed Associations - remove it
                                    is_present = true;
                                    continue;
                                }
                                break;
                            case MIME_TYPE_ACTION_REMOVE:
                                if (k < 2)
                                {
                                    // Default or Added - remove it
                                    is_present = true;
                                    continue;
                                }
                                else
                                {
                                    // Removed Associations - already present
                                    is_present = true;
                                    break;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    // copy other apps to new list preserving order
                    str = new_action;
                    new_action = g_strdup_printf("%s%s;", str ? str : "", apps[i]);
                    g_free(str);
                }
            }
            g_strfreev(apps);
        }

        // update key string if needed
        if (action < MIME_TYPE_ACTION_REMOVE)
        {
            if ((k < 2 && !is_present) || (k == 2 && is_present))
            {
                if (k < 2)
                {
                    // add to front of Default or Added list
                    str = new_action;
                    if (action == MIME_TYPE_ACTION_DEFAULT)
                        new_action =
                            g_strdup_printf("%s;%s", desktop_id, new_action ? new_action : "");
                    else // if ( action == MIME_TYPE_ACTION_APPEND )
                        new_action =
                            g_strdup_printf("%s%s;", new_action ? new_action : "", desktop_id);
                    g_free(str);
                }
                if (new_action)
                    g_key_file_set_string(file, groups[k], type, new_action);
                else
                    g_key_file_remove_key(file, groups[k], type, nullptr);
                data_changed = true;
            }
        }
        else // if ( action == MIME_TYPE_ACTION_REMOVE )
        {
            if ((k < 2 && is_present) || (k == 2 && !is_present))
            {
                if (k == 2)
                {
                    // add to end of Removed list
                    str = new_action;
                    new_action = g_strdup_printf("%s%s;", new_action ? new_action : "", desktop_id);
                    g_free(str);
                }
                if (new_action)
                    g_key_file_set_string(file, groups[k], type, new_action);
                else
                    g_key_file_remove_key(file, groups[k], type, nullptr);
                data_changed = true;
            }
        }
        g_free(new_action);
    }

    // save updated mimeapps.list
    if (data_changed)
    {
        unsigned long len = 0;
        char* data = g_key_file_to_data(file, &len, nullptr);
        save_to_file(path.c_str(), data, len);
        g_free(data);
    }
    g_key_file_free(file);
}
