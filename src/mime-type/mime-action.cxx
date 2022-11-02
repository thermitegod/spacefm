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

#include <array>
#include <vector>

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/keyfile.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "write.hxx"
#include "utils.hxx"

#include "mime-action.hxx"

static void
save_to_file(const std::string& path, const std::string& data)
{
    write_file(path, data);

    if (std::filesystem::exists(path))
        std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

static void
update_desktop_database()
{
    const std::string path = Glib::build_filename(vfs_user_data_dir(), "applications");
    const std::string command = fmt::format("update-desktop-database {}", path);
    print_command(command);
    Glib::spawn_command_line_sync(command);
}

/* Determine removed associations for this type */
static void
remove_actions(const std::string mime_type, std::vector<std::string> actions)
{
    // LOG_INFO("remove_actions( {} )", type);

    const auto kf = Glib::KeyFile::create();
    try
    {
        // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
        const std::string path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");

        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (Glib::FileError)
    {
        try
        {
            // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
            const std::string path =
                Glib::build_filename(vfs_user_data_dir(), "applications/mimeapps.list");

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

    for (std::size_t r = 0; r < removed.size(); ++r)
    {
        // LOG_INFO("    {}", removed[r]);
        const std::string rem = removed.at(r);
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

    const std::array<std::string, 2> names{"mimeapps.list", "mimeinfo.cache"};
    const std::array<std::string, 3> groups{"Default Applications",
                                            "Added Associations",
                                            "MIME Cache"};

    // LOG_INFO("get_actions( {}/, {} )", dir, type);
    for (std::size_t n = 0; n < names.size(); ++n)
    {
        const std::string path = Glib::build_filename(dir, names.at(n));
        // LOG_INFO( "    {}", path);
        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (Glib::FileError)
        {
            continue;
        }

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

        // mimeinfo.cache has only MIME Cache; others do not have it
        for (int k = (n == 0 ? 0 : 2); k < (n == 0 ? 2 : 3); ++k)
        {
            // LOG_INFO("        {} [{}]", groups[k], k);
            bool is_removed;
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(groups.at(k), type);
                // if (apps.empty())
                //     return nullptr;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                continue;
            }
            for (std::size_t i = 0; i < apps.size(); ++i)
            {
                //  LOG_INFO("            {}", apps[i]);
                //  check if removed
                is_removed = false;
                if (!removed.empty() && n > 0)
                {
                    for (std::size_t r = 0; r < removed.size(); ++r)
                    {
                        if (ztd::same(removed[r].data(), apps[i].data()))
                        {
                            // LOG_INFO("                REMOVED");
                            is_removed = true;
                            break;
                        }
                    }
                }
                const std::string app = apps.at(i);
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
    for (const std::string& sys_dir: vfs_system_data_dir())
    {
        dir = Glib::build_filename(sys_dir, "applications");
        get_actions(dir, mime_type, actions);
    }

    /* remove actions for this file type */ // sfm
    remove_actions(mime_type, actions);

    /* ensure default app is in the list */
    const std::string default_app = ztd::null_check(mime_type_get_default_action(mime_type));
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
    bool is_desktop = ztd::endswith(desktop_id, ".desktop");

    if (is_desktop)
    {
        const Glib::ustring filename = mime_type_locate_desktop_file(nullptr, desktop_id);

        const auto kf = Glib::KeyFile::create();
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
            types = kf->get_string_list("Desktop Entry", "MimeType");
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
            cmd = kf->get_string("Desktop Entry", "Exec");
            name = kf->get_string("Desktop Entry", "Name");
        }
    }
    else
    {
        cmd = desktop_id;
    }

    std::vector<std::string> actions = mime_type_get_actions(type);
    if (!actions.empty())
    {
        for (const std::string& action: actions)
        {
            /* Try to match directly by desktop_id first */
            if (is_desktop && ztd::same(action, desktop_id))
            {
                found = true;
                break;
            }
            else /* Then, try to match by "Exec" and "Name" keys */
            {
                const Glib::ustring filename =
                    mime_type_locate_desktop_file(nullptr, action.c_str());

                const auto kf = Glib::KeyFile::create();
                try
                {
                    kf->load_from_file(filename, Glib::KeyFile::Flags::NONE);
                }
                catch (Glib::FileError)
                {
                    return false;
                }

                const Glib::ustring cmd2 = kf->get_string("Desktop Entry", "Exec");
                if (ztd::same(cmd.data(), cmd2.data())) /* 2 desktop files have same "Exec" */
                {
                    if (is_desktop)
                    {
                        const Glib::ustring name2 = kf->get_string("Desktop Entry", "Name");
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
    std::string name;
    std::string cust_template;
    std::string cust;
    Glib::ustring file_content;

    static const std::string desktop_ext = ".desktop";
    static const std::string replace_txt = "<REPLACE_TXT>";

    if (ztd::endswith(desktop_id, desktop_ext))
    {
        const Glib::ustring filename = mime_type_locate_desktop_file(nullptr, desktop_id);

        const auto kf = Glib::KeyFile::create();
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
        kf->set_string_list("Desktop Entry", "MimeType", mime_types);
        /* store id of original desktop file, for future use. */
        kf->set_string("Desktop Entry", "X-MimeType-Derived", desktop_id);
        kf->set_string("Desktop Entry", "NoDisplay", "true");

        name = ztd::removesuffix(desktop_id, desktop_ext);
        cust_template = fmt::format("{}-usercustom-{}.desktop", name, replace_txt);

        file_content = kf->to_data();
    }
    else /* it is not a desktop_id, but a command */
    {
        /* Make a user-created desktop file for the command */
        name = Glib::path_get_basename(desktop_id);
        cust_template = fmt::format("{}-usercreated-{}.desktop", name, replace_txt);

        file_content = fmt::format("[Desktop Entry]\n"
                                   "Encoding=UTF-8\n"
                                   "Name={}\n"
                                   "Exec={}\n"
                                   "MimeType={}\n"
                                   "Icon=exec\n"
                                   "Terminal=false\n"
                                   "NoDisplay=true\n",
                                   name,
                                   desktop_id,
                                   mime_type);
    }

    /* generate unique file name */
    const std::string dir = Glib::build_filename(vfs_user_data_dir(), "applications");
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    std::string path;
    for (int i = 0;; ++i)
    {
        /* generate the basename */
        cust = ztd::replace(cust_template, replace_txt, std::to_string(i));
        path = Glib::build_filename(dir, cust); /* test if the filename already exists */
        if (!std::filesystem::exists(path))     /* this generated filename can be used */
            break;
    }
    save_to_file(path, file_content);

    /* execute update-desktop-database" to update mimeinfo.cache */
    update_desktop_database();

    return ztd::strdup(cust);
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
        for (const auto& file: std::filesystem::directory_iterator(path))
        {
            const std::string file_name = std::filesystem::path(file).filename();

            const std::string sub_path = Glib::build_filename(path, file_name);
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
_locate_desktop_file(const char* dir, const void* desktop_id)
{ // sfm 0.7.8 modified + 0.8.7 modified
    bool found = false;

    const std::string desktop_path =
        Glib::build_filename(dir, "applications", (const char*)desktop_id);
    char* path = ztd::strdup(desktop_path);

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
        {
            break;
        }
    } while (!found);

    if (found)
        return path;
    free(path);

    // sfm 0.8.7 some desktop files listed by the app chooser are in subdirs
    const std::string desktop_recursive_path = Glib::build_filename(dir, "applications");
    sep = _locate_desktop_file_recursive(desktop_recursive_path.c_str(),
                                         (const char*)desktop_id,
                                         true);
    return sep;
}

char*
mime_type_locate_desktop_file(const char* dir, const char* desktop_id)
{
    if (dir)
        return _locate_desktop_file(dir, (void*)desktop_id);

    char* ret = nullptr;

    const std::string data_dir = vfs_user_data_dir();

    if ((ret = _locate_desktop_file(data_dir.c_str(), (void*)desktop_id)))
        return ret;

    for (const std::string& sys_dir: vfs_system_data_dir())
    {
        if ((ret = _locate_desktop_file(sys_dir.c_str(), (void*)desktop_id)))
            return ret;
    }
    return ret;
}

static char*
get_default_action(const char* dir, const char* type)
{
    // LOG_INFO("get_default_action( {}, {} )", dir, type);
    // search these files in dir for the first existing default app
    const std::array<std::string, 2> names{"mimeapps.list", "defaults.list"};
    const std::array<std::string, 3> groups{"Default Applications", "Added Associations"};

    for (std::size_t n = 0; n < names.size(); ++n)
    {
        const std::string path = Glib::build_filename(dir, names.at(n));
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

        for (std::size_t k = 0; k < groups.size(); ++k)
        {
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(groups.at(k), type);
                if (apps.empty())
                    break;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                break;
            }

            for (std::size_t i = 0; i < apps.size(); ++i)
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
                break; // defaults.list does not have Added Associations
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

    char* ret = nullptr;

    std::string dir;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    if ((ret = get_default_action(vfs_user_config_dir().c_str(), mime_type.c_str())))
        return ret;

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    dir = Glib::build_filename(vfs_user_data_dir(), "applications");
    if ((ret = get_default_action(dir.c_str(), mime_type.c_str())))
        return ret;

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (const std::string& sys_dir: vfs_system_data_dir())
    {
        dir = Glib::build_filename(sys_dir, "applications");
        if ((ret = get_default_action(dir.c_str(), mime_type.c_str())))
            return ret;
    }

    return ret;
}

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     MimeTypeAction::MIME_TYPE_ACTION_DEFAULT - make desktop_id the default app
 *     MimeTypeAction::MIME_TYPE_ACTION_APPEND  - add desktop_id to Default and Added apps
 *     MimeTypeAction::MIME_TYPE_ACTION_REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void
mime_type_update_association(const char* type, const char* desktop_id, int action)
{
    bool data_changed = false;

    if (!(type && type[0] != '\0' && desktop_id && desktop_id[0] != '\0'))
    {
        LOG_WARN("mime_type_update_association invalid type or desktop_id");
        return;
    }
    if (action > MimeTypeAction::MIME_TYPE_ACTION_REMOVE ||
        action < MimeTypeAction::MIME_TYPE_ACTION_DEFAULT)
    {
        LOG_WARN("mime_type_update_association invalid action");
        return;
    }

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    const std::string path = Glib::build_filename(vfs_user_config_dir(), "mimeapps.list");

    // Load current mimeapps.list content, if available
    const auto kf = Glib::KeyFile::create();
    try
    {
        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (Glib::FileError)
    {
        return;
    }

    std::array<std::string, 3> groups{"Default Applications",
                                      "Added Associations",
                                      "Removed Associations"};

    for (const std::string& group: groups)
    {
        std::string new_action;
        bool is_present = false;

        std::vector<Glib::ustring> apps;
        try
        {
            apps = kf->get_string_list(group, type);
            if (apps.empty())
                return;
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            return;
        }

        MimeTypeAction group_block;
        if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::MIME_TYPE_ACTION_DEFAULT;
        else if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::MIME_TYPE_ACTION_APPEND;
        else // if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::MIME_TYPE_ACTION_REMOVE;

        for (std::size_t i = 0; i < apps.size(); ++i)
        {
            if (apps[i][0] != '\0')
            {
                if (ztd::same(apps.at(i).data(), desktop_id))
                {
                    switch (action)
                    {
                        case MimeTypeAction::MIME_TYPE_ACTION_DEFAULT:
                            // found desktop_id already in group list
                            if (group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
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
                        case MimeTypeAction::MIME_TYPE_ACTION_APPEND:
                            if (group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
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
                        case MimeTypeAction::MIME_TYPE_ACTION_REMOVE:
                            if (group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
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
                new_action = fmt::format("{}{};", new_action, apps.at(i).c_str());
            }
        }

        // update key string if needed
        if (action < MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
        {
            if ((group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE && !is_present) ||
                (group_block == MimeTypeAction::MIME_TYPE_ACTION_REMOVE && is_present))
            {
                if (group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
                {
                    // add to front of Default or Added list
                    if (action == MimeTypeAction::MIME_TYPE_ACTION_DEFAULT)
                        new_action = fmt::format("{};{}", desktop_id, new_action);
                    else // if ( action == MimeTypeAction::MIME_TYPE_ACTION_APPEND )
                        new_action = fmt::format("{}{};", new_action, desktop_id);
                }
                if (!new_action.empty())
                    kf->set_string(group, type, new_action);
                else
                    kf->remove_key(group, type);
                data_changed = true;
            }
        }
        else // if ( action == MimeTypeAction::MIME_TYPE_ACTION_REMOVE )
        {
            if ((group_block < MimeTypeAction::MIME_TYPE_ACTION_REMOVE && is_present) ||
                (group_block == MimeTypeAction::MIME_TYPE_ACTION_REMOVE && !is_present))
            {
                if (group_block == MimeTypeAction::MIME_TYPE_ACTION_REMOVE)
                {
                    // add to end of Removed list
                    new_action = fmt::format("{}{};", new_action, desktop_id);
                }
                if (!new_action.empty())
                    kf->set_string(group, type, new_action);
                else
                    kf->remove_key(group, type);
                data_changed = true;
            }
        }
    }

    // save updated mimeapps.list
    if (data_changed)
    {
        Glib::ustring data = kf->to_data();
        save_to_file(path, data);
    }
}
