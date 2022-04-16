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

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <fcntl.h>

#include <glibmm.h>
#include <glibmm/keyfile.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "utils.hxx"

#include "mime-action.hxx"

static void
save_to_file(const std::string& path, const std::string& data)
{
    std::ofstream file(path);
    if (file.is_open())
        file << data;
    file.close();
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

static const std::string group_desktop = "Desktop Entry";
static const std::string key_mime_type = "MimeType";

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

/* Determine removed associations for this type */
static void
remove_actions(const std::string mime_type, std::vector<std::string> actions)
{
    // LOG_INFO("remove_actions( {} )", type);
    const auto kf = Glib::KeyFile::create();

    std::string path;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");
    try
    {
        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (Glib::FileError)
    {
        // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
        path = Glib::build_filename(vfs_user_data_dir(), "applications/mimeapps.list");
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (Glib::FileError)
        {
            return;
        }
    }

    std::vector<Glib::ustring> removed;
    try
    {
        removed = kf->get_string_list("Removed Associations", mime_type);
        if (removed.empty())
            return;
    }
    catch (...) // Glib::KeyFileError, Glib::FileError
    {
        return;
    }

    unsigned long r;
    for (r = 0; r < removed.size(); ++r)
    {
        // LOG_INFO("    {}", removed[r]);
        std::string rem = removed.at(r);
        if (ztd::contains(actions, rem))
        {
            ztd::remove(actions, rem);
            // LOG_INFO("        ACTION-REMOVED");
        }
    }
}

/*
 * Get applications associated with this mime-type
 *
 * This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 */
static void
get_actions(const std::string& dir, const std::string& type, std::vector<std::string>& actions)
{
    // LOG_INFO("get_actions( {}, {} )\n", dir, type);
    std::vector<Glib::ustring> removed;

    const char* names[] = {"mimeapps.list", "mimeinfo.cache"};
    const char* groups[] = {"Default Applications", "Added Associations", "MIME Cache"};
    // LOG_INFO("get_actions( {}/, {} )", dir, type);
    for (unsigned int n = 0; n < G_N_ELEMENTS(names); n++)
    {
        std::string path = Glib::build_filename(dir, names[n]);
        // LOG_INFO( "    {}", path);
        const auto kf = Glib::KeyFile::create();
        bool opened;

        try
        {
            opened = kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (Glib::FileError)
        {
            opened = false;
        }

        if (opened)
        {
            if (n == 0)
            {
                // get removed associations in this dir
                try
                {
                    removed = kf->get_string_list("Removed Associations", type);
                    // if (removed.empty())
                    //     continue;
                }
                catch (...) // Glib::KeyFileError, Glib::FileError
                {
                    continue;
                }
            }
            // mimeinfo.cache has only MIME Cache; others don't have it
            int k;
            for (k = (n == 0 ? 0 : 2); k < (n == 0 ? 2 : 3); k++)
            {
                // LOG_INFO("        {} [{}]", groups[k], k);
                bool is_removed;
                std::vector<Glib::ustring> apps;
                try
                {
                    apps = kf->get_string_list(groups[k], type);
                    // if (apps.empty())
                    //     return nullptr;
                }
                catch (...) // Glib::KeyFileError, Glib::FileError
                {
                    continue;
                }
                unsigned long i;
                for (i = 0; i < apps.size(); ++i)
                {
                    //  LOG_INFO("            {}", apps[i]);
                    //  check if removed
                    is_removed = false;
                    if (!removed.empty() && n > 0)
                    {
                        unsigned long r;
                        for (r = 0; r < removed.size(); ++r)
                        {
                            if (ztd::same(removed.at(r).data(), apps.at(i).data()))
                            {
                                // LOG_INFO("                REMOVED");
                                is_removed = true;
                                break;
                            }
                        }
                    }
                    std::string app = apps.at(i);
                    if (!is_removed && !ztd::contains(actions, app))
                    {
                        /* check for app existence */
                        if (mime_type_locate_desktop_file(nullptr, apps.at(i).c_str()))
                        {
                            // LOG_INFO("                EXISTS");
                            actions.push_back(app);
                        }
                        else
                        {
                            // LOG_INFO("                MISSING");
                        }
                    }
                }
            }
        }
        if (ztd::same(dir, vfs_user_config_dir()))
            break; // no mimeinfo.cache in ~/.config
    }
}

std::vector<std::string>
mime_type_get_actions(const std::string& mime_type)
{
    std::vector<std::string> actions;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */
    std::string dir;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    get_actions(vfs_user_config_dir(), mime_type, actions);

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    dir = Glib::build_filename(vfs_user_data_dir(), "applications");
    get_actions(dir, mime_type, actions);

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (std::string sys_dir: vfs_system_data_dir())
    {
        dir = Glib::build_filename(sys_dir, "applications");
        get_actions(dir, mime_type, actions);
    }

    /* remove actions for this file type */ // sfm
    remove_actions(mime_type, actions);

    /* ensure default app is in the list */
    std::string default_app = ztd::null_check(mime_type_get_default_action(mime_type));
    if (!default_app.empty())
    {
        if (!ztd::contains(actions, default_app))
        {
            // default app is not in the list, add it!
            actions.push_back(default_app);
        }
        else /* default app is in the list, move it to the first. */
        {
            if (std::size_t index = ztd::index(actions, default_app) != 0)
            {
                ztd::move(actions, index, 0);
            }
        }
    }
    return actions;
}

/*
 * NOTE:
 * This API is very time consuming, but unfortunately, due to the damn poor design of
 * Freedesktop.org spec, all the insane checks here are necessary.  Sigh...  :-(
 */
static bool
mime_type_has_action(const char* type, const char* desktop_id)
{
    Glib::ustring cmd;
    Glib::ustring name;

    bool found = false;
    bool is_desktop = Glib::str_has_suffix(desktop_id, ".desktop");

    if (is_desktop)
    {
        const auto kf = Glib::KeyFile::create();
        Glib::ustring filename = mime_type_locate_desktop_file(nullptr, desktop_id);

        try
        {
            kf->load_from_file(filename, Glib::KeyFile::Flags::NONE);
        }
        catch (Glib::FileError)
        {
            return false;
        }

        std::vector<Glib::ustring> types;
        try
        {
            types = kf->get_string_list(group_desktop, key_mime_type);
            if (types.empty())
                return false;
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            return false;
        }

        for (Glib::ustring known_type: types)
        {
            if (ztd::same(known_type.data(), type))
            {
                // our mime-type is already found in the desktop file.
                // no further check is needed
                found = true;
                break;
            }
        }

        if (!found) /* get the content of desktop file for comparison */
        {
            cmd = kf->get_string(group_desktop, "Exec");
            name = kf->get_string(group_desktop, "Name");
        }
    }
    else
    {
        cmd = desktop_id;
    }

    std::vector<std::string> actions = mime_type_get_actions(type);
    if (!actions.empty())
    {
        for (std::string action: actions)
        {
            /* Try to match directly by desktop_id first */
            if (is_desktop && ztd::same(action, desktop_id))
            {
                found = true;
                break;
            }
            else /* Then, try to match by "Exec" and "Name" keys */
            {
                Glib::ustring name2;
                Glib::ustring cmd2;
                Glib::ustring filename = mime_type_locate_desktop_file(nullptr, action.c_str());
                const auto kf = Glib::KeyFile::create();

                try
                {
                    kf->load_from_file(filename, Glib::KeyFile::Flags::NONE);
                }
                catch (Glib::FileError)
                {
                    return false;
                }

                cmd2 = kf->get_string(group_desktop, "Exec");
                if (ztd::same(cmd.data(), cmd2.data())) /* 2 desktop files have same "Exec" */
                {
                    if (is_desktop)
                    {
                        name2 = kf->get_string(group_desktop, "Name");
                        /* Then, check if the "Name" keys of 2 desktop files are the same. */
                        if (ztd::same(name.data(), name2.data()))
                        {
                            /* Both "Exec" and "Name" keys of the 2 desktop files are
                             *  totally the same. So, despite having different desktop id
                             *  They actually refer to the same application. */
                            found = true;
                            break;
                        }
                    }
                    else
                    {
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    return found;
}

static char*
make_custom_desktop_file(const char* desktop_id, const char* mime_type)
{
    Glib::ustring filename;
    char* name = nullptr;
    char* cust_template = nullptr;
    char* cust = nullptr;
    Glib::ustring file_content;

    if (Glib::str_has_suffix(desktop_id, ".desktop"))
    {
        const auto kf = Glib::KeyFile::create();
        filename = mime_type_locate_desktop_file(nullptr, desktop_id);

        try
        {
            kf->load_from_file(filename, Glib::KeyFile::Flags::KEEP_TRANSLATIONS);
        }
        catch (Glib::FileError)
        {
            return nullptr;
        }

        // FIXME: If the source desktop_id refers to a custom desktop file, and
        //  value of the MimeType key equals to our mime-type, there is no
        //  need to generate a new desktop file.
        //
        //  if(is_custom_desktop_file(desktop_id))
        //

        const std::vector<Glib::ustring> mime_types{mime_type};
        /* set our mime-type */
        kf->set_string_list(group_desktop, key_mime_type, mime_types);
        /* store id of original desktop file, for future use. */
        kf->set_string(group_desktop, "X-MimeType-Derived", desktop_id);
        kf->set_string(group_desktop, "NoDisplay", "true");

        name = g_strndup(desktop_id, std::strlen(desktop_id) - 8);
        cust_template = g_strdup_printf("%s-usercustom-%%d.desktop", name);
        free(name);

        file_content = kf->to_data();
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
                                  "Terminal=false\n"
                                  "NoDisplay=true\n";
        /* Make a user-created desktop file for the command */
        name = g_path_get_basename(desktop_id);
        if ((p = strchr(name, ' '))) /* FIXME: skip command line arguments. is this safe? */
            *p = '\0';
        file_content = g_strdup_printf(file_templ, name, desktop_id, mime_type);
        cust_template = g_strdup_printf("%s-usercreated-%%d.desktop", name);
        free(name);
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
            free(cust);
        else /* this generated filename can be used */
            break;
    }

    save_to_file(path, file_content);

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
            *custom_desktop = ztd::strdup(desktop_id);
        return;
    }

    char* cust = make_custom_desktop_file(desktop_id, type);
    if (custom_desktop)
        *custom_desktop = cust;
    else
        free(cust);
}

static char*
_locate_desktop_file_recursive(const char* path, const char* desktop_id, bool first)
{
    // if first is true, just search for subdirs not desktop_id (already searched)

    char* found = nullptr;

    if (std::filesystem::is_directory(path))
    {
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(path))
        {
            file_name = std::filesystem::path(file).filename();

            std::string sub_path = Glib::build_filename(path, file_name);
            if (std::filesystem::is_directory(sub_path))
            {
                found = _locate_desktop_file_recursive(sub_path.c_str(), desktop_id, false);
                if (found)
                    break;
            }
            else if (!first && ztd::same(file_name, desktop_id) &&
                     std::filesystem::is_regular_file(sub_path))
            {
                found = ztd::strdup(sub_path);
                break;
            }
        }
    }

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
    free(path);

    // sfm 0.8.7 some desktop files listed by the app chooser are in subdirs
    path = g_build_filename(dir, "applications", nullptr);
    sep = _locate_desktop_file_recursive(path, (const char*)desktop_id, true);
    free(path);
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
        std::string path = Glib::build_filename(dir, names[n]);
        // LOG_INFO("    path = {}", path);
        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (Glib::FileError)
        {
            return nullptr;
        }

        for (unsigned int k = 0; k < G_N_ELEMENTS(groups); k++)
        {
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(groups[k], type);
                if (apps.empty())
                    break;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                break;
            }

            unsigned long i;
            for (i = 0; i < apps.size(); ++i)
            {
                if (apps[i][0] != '\0')
                {
                    // LOG_INFO("        {}", apps[i]);
                    if (mime_type_locate_desktop_file(nullptr, apps.at(i).c_str()))
                    {
                        // LOG_INFO("            EXISTS");
                        return ztd::strdup(apps.at(i));
                    }
                }
            }

            if (n == 1)
                break; // defaults.list doesn't have Added Associations
        }

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
mime_type_get_default_action(const std::string& mime_type)
{
    /* FIXME: need to check parent types if default action of current type is not set. */
    return data_dir_foreach((DataDirFunc)get_default_action, mime_type.c_str(), nullptr);
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
    const auto kf = Glib::KeyFile::create();
    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    std::string path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");

    try
    {
        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (Glib::FileError)
    {
        return;
    }

    for (unsigned int k = 0; k < G_N_ELEMENTS(groups); k++)
    {
        char* str;
        char* new_action = nullptr;
        bool is_present = false;

        std::vector<Glib::ustring> apps;
        try
        {
            apps = kf->get_string_list(groups[k], type);
            if (apps.empty())
                return;
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            return;
        }

        unsigned long i;
        for (i = 0; i < apps.size(); ++i)
        {
            if (apps[i][0] != '\0')
            {
                if (ztd::same(apps.at(i).data(), desktop_id))
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
                new_action = g_strdup_printf("%s%s;", str ? str : "", apps.at(i).c_str());
                free(str);
            }
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
                    free(str);
                }
                if (new_action)
                    kf->set_string(groups[k], type, new_action);
                else
                    kf->remove_key(groups[k], type);
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
                    free(str);
                }
                if (new_action)
                    kf->set_string(groups[k], type, new_action);
                else
                    kf->remove_key(groups[k], type);
                data_changed = true;
            }
        }
        free(new_action);
    }

    // save updated mimeapps.list
    if (data_changed)
    {
        Glib::ustring data = kf->to_data();
        save_to_file(path, data);
    }
}
