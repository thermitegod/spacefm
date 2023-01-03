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
#include <string_view>

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

#include "vfs/vfs-user-dirs.hxx"

#include "write.hxx"
#include "utils.hxx"

#include "mime-type/mime-action.hxx"

static void
save_to_file(std::string_view path, const Glib::ustring& data)
{
    write_file(path, data);

    if (std::filesystem::exists(path))
        std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

static void
update_desktop_database()
{
    const std::string path = Glib::build_filename(vfs::user_dirs->data_dir(), "applications");
    const std::string command = fmt::format("update-desktop-database {}", path);
    LOG_INFO("COMMAND={}", command);
    Glib::spawn_command_line_sync(command);
}

/* Determine removed associations for this type */
static void
remove_actions(std::string_view mime_type, std::vector<std::string>& actions)
{
    // LOG_INFO("remove_actions( {} )", type);

    const auto kf = Glib::KeyFile::create();
    try
    {
        // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
        const std::string path =
            Glib::build_filename(vfs::user_dirs->config_dir(), "mimeapps.list");

        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (Glib::FileError)
    {
        try
        {
            // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
            const std::string path =
                Glib::build_filename(vfs::user_dirs->data_dir(), "applications/mimeapps.list");

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
        removed = kf->get_string_list("Removed Associations", mime_type.data());
        if (removed.empty())
            return;
    }
    catch (...) // Glib::KeyFileError, Glib::FileError
    {
        return;
    }

    for (auto& r : removed)
    {
        const std::string rem = r;
        if (ztd::contains(actions, rem))
        {
            ztd::remove(actions, rem);
            // LOG_INFO("        ACTION-REMOVED {}", rem);
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
get_actions(std::string_view dir, std::string_view type, std::vector<std::string>& actions)
{
    // LOG_INFO("get_actions( {}, {} )\n", dir, type);
    std::vector<Glib::ustring> removed;

    static constexpr std::array<std::string_view, 2> names{
        "mimeapps.list",
        "mimeinfo.cache",
    };
    static constexpr std::array<std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
        "MIME Cache",
    };

    // LOG_INFO("get_actions( {}/, {} )", dir, type);
    for (usize n = 0; n < names.size(); ++n)
    {
        const std::string path = Glib::build_filename(dir.data(), names.at(n).data());
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
                removed = kf->get_string_list("Removed Associations", type.data());
                // if (removed.empty())
                //     continue;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                continue;
            }
        }

        // mimeinfo.cache has only MIME Cache; others do not have it
        for (i32 k = (n == 0 ? 0 : 2); k < (n == 0 ? 2 : 3); ++k)
        {
            // LOG_INFO("        {} [{}]", groups[k], k);
            bool is_removed;
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(groups.at(k).data(), type.data());
                // if (apps.empty())
                //     return nullptr;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                continue;
            }
            for (auto& a : apps)
            {
                //  LOG_INFO("            {}", apps[i]);
                //  check if removed
                is_removed = false;
                if (!removed.empty() && n > 0)
                {
                    for (auto& r : removed)
                    {
                        if (ztd::same(r.data(), a.data()))
                        {
                            // LOG_INFO("                REMOVED");
                            is_removed = true;
                            break;
                        }
                    }
                }
                const std::string app = a;
                if (!is_removed && !ztd::contains(actions, app))
                {
                    /* check for app existence */
                    if (mime_type_locate_desktop_file(app))
                    {
                        // LOG_INFO("                EXISTS");
                        actions.emplace_back(app);
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

const std::vector<std::string>
mime_type_get_actions(std::string_view mime_type)
{
    std::vector<std::string> actions;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    get_actions(vfs::user_dirs->config_dir(), mime_type, actions);

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    const std::string dir = Glib::build_filename(vfs::user_dirs->data_dir(), "applications");
    get_actions(dir, mime_type, actions);

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const std::string sdir = Glib::build_filename(sys_dir.data(), "applications");
        get_actions(sdir, mime_type, actions);
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
            actions.emplace_back(default_app);
        }
        else /* default app is in the list, move it to the first. */
        {
            if (usize index = ztd::index(actions, default_app) != 0)
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
 *
 * Check if an applications currently set to open this mime-type
 * desktop_id is the name of *.desktop file.
 */
static bool
mime_type_has_action(std::string_view type, std::string_view desktop_id)
{
    Glib::ustring cmd;
    Glib::ustring name;

    bool found = false;
    const bool is_desktop = ztd::endswith(desktop_id.data(), ".desktop");

    if (is_desktop)
    {
        const Glib::ustring filename = mime_type_locate_desktop_file(desktop_id);

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

        for (Glib::ustring known_type : types)
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
        cmd = desktop_id.data();
    }

    const std::vector<std::string> actions = mime_type_get_actions(type);
    if (actions.empty())
        return found;

    for (std::string_view action : actions)
    {
        /* Try to match directly by desktop_id first */
        if (is_desktop && ztd::same(action, desktop_id))
        {
            found = true;
            break;
        }
        else /* Then, try to match by "Exec" and "Name" keys */
        {
            const Glib::ustring filename = mime_type_locate_desktop_file(action);

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
    return found;
}

static const std::string
make_custom_desktop_file(std::string_view desktop_id, std::string_view mime_type)
{
    std::string cust_template;
    Glib::ustring file_content;

    static constexpr std::string_view desktop_ext{".desktop"};
    static constexpr std::string_view replace_txt{"<REPLACE_TXT>"};

    if (ztd::endswith(desktop_id, desktop_ext))
    {
        const Glib::ustring filename = mime_type_locate_desktop_file(desktop_id);

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

        const std::vector<Glib::ustring> mime_types{mime_type.data()};
        /* set our mime-type */
        kf->set_string_list("Desktop Entry", "MimeType", mime_types);
        /* store id of original desktop file, for future use. */
        kf->set_string("Desktop Entry", "X-MimeType-Derived", desktop_id.data());
        kf->set_string("Desktop Entry", "NoDisplay", "true");

        const std::string name = ztd::removesuffix(desktop_id, desktop_ext);
        cust_template = fmt::format("{}-usercustom-{}.desktop", name, replace_txt);

        file_content = kf->to_data();
    }
    else /* it is not a desktop_id, but a command */
    {
        /* Make a user-created desktop file for the command */
        const std::string name = Glib::path_get_basename(desktop_id.data());
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
    const std::string dir = Glib::build_filename(vfs::user_dirs->data_dir(), "applications");
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    std::string cust;

    for (i32 i = 0;; ++i)
    {
        /* generate the basename */
        cust = ztd::replace(cust_template, replace_txt, std::to_string(i));
        /* test if the filename already exists */
        const std::string path = Glib::build_filename(dir, cust);
        if (!std::filesystem::exists(path))
        { /* this generated filename can be used */
            save_to_file(path, file_content);
            break;
        }
    }

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
const std::string
mime_type_add_action(std::string_view type, std::string_view desktop_id)
{
    if (mime_type_has_action(type, desktop_id))
        return desktop_id.data();

    return make_custom_desktop_file(desktop_id, type);
}

static char*
_locate_desktop_file(std::string_view dir, std::string_view desktop_id)
{
    const std::string desktop_path =
        Glib::build_filename(dir.data(), "applications", desktop_id.data());
    if (std::filesystem::is_regular_file(desktop_path))
        return ztd::strdup(desktop_path);

    // LOG_INFO("desktop_id={}", desktop_id);

    // mime encodes directory separators as '-'.
    // so the desktop_id 'mime-mime-mime.desktop' could be on-disk
    // mime-mime-mime.desktop, mime/mime-mime.desktop, or mime/mime/mime.desktop.
    // not supported is mime-mime/mime.desktop
    std::string new_desktop_id = desktop_id.data();
    while (ztd::contains(new_desktop_id, "-"))
    {
        new_desktop_id = ztd::replace(new_desktop_id, "-", "/", 1);
        const std::string new_desktop_path =
            Glib::build_filename(dir.data(), "applications", new_desktop_id);
        // LOG_INFO("new_desktop_id={}", new_desktop_id);
        if (std::filesystem::is_regular_file(new_desktop_path))
            return ztd::strdup(new_desktop_path);
    }

    return nullptr;
}

const char*
mime_type_locate_desktop_file(std::string_view dir, std::string_view desktop_id)
{
    return _locate_desktop_file(dir, desktop_id);
}

const char*
mime_type_locate_desktop_file(std::string_view desktop_id)
{
    const std::string& data_dir = vfs::user_dirs->data_dir();

    const char* data_desktop = _locate_desktop_file(data_dir, desktop_id);
    if (data_desktop)
        return data_desktop;

    for (std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const char* sys_desktop = _locate_desktop_file(sys_dir, desktop_id);
        if (sys_desktop)
            return sys_desktop;
    }

    return nullptr;
}

static char*
get_default_action(std::string_view dir, std::string_view type)
{
    // LOG_INFO("get_default_action( {}, {} )", dir, type);
    // search these files in dir for the first existing default app
    static constexpr std::array<std::string_view, 2> names{
        "mimeapps.list",
        "defaults.list",
    };
    static constexpr std::array<std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
    };

    for (std::string_view name : names)
    {
        const std::string path = Glib::build_filename(dir.data(), name.data());
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

        for (std::string_view group : groups)
        {
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(group.data(), type.data());
                if (apps.empty())
                    break;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                break;
            }

            for (const Glib::ustring& app : apps)
            {
                if (app.empty())
                    continue;

                // LOG_INFO("        {}", apps[i]);
                if (mime_type_locate_desktop_file(app.data()))
                {
                    // LOG_INFO("            EXISTS");
                    return ztd::strdup(app);
                }
            }

            if (ztd::same(name, "defaults.list"))
                break; // defaults.list does not have Added Associations
        }

        if (ztd::same(dir, vfs::user_dirs->config_dir()))
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
const char*
mime_type_get_default_action(std::string_view mime_type)
{
    /* FIXME: need to check parent types if default action of current type is not set. */

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    const char* home_default_action =
        get_default_action(vfs::user_dirs->config_dir(), mime_type.data());
    if (home_default_action)
        return home_default_action;

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    const std::string data_app_dir =
        Glib::build_filename(vfs::user_dirs->data_dir(), "applications");
    const char* data_default_action = get_default_action(data_app_dir, mime_type.data());
    if (data_default_action)
        return data_default_action;

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const std::string sys_app_dir = Glib::build_filename(sys_dir.data(), "applications");
        const char* sys_default_action = get_default_action(sys_app_dir, mime_type.data());
        if (sys_default_action)
            return sys_default_action;
    }

    return nullptr;
}

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     MimeTypeAction::DEFAULT - make desktop_id the default app
 *     MimeTypeAction::APPEND  - add desktop_id to Default and Added apps
 *     MimeTypeAction::REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void
mime_type_update_association(std::string_view type, std::string_view desktop_id,
                             MimeTypeAction action)
{
    if (type.empty() || desktop_id.empty())
    {
        LOG_WARN("mime_type_update_association invalid type or desktop_id");
        return;
    }

    bool data_changed = false;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    const std::string path = Glib::build_filename(vfs::user_dirs->config_dir(), "mimeapps.list");

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

    static constexpr std::array<std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
        "Removed Associations",
    };

    for (std::string_view group : groups)
    {
        std::string new_action;
        bool is_present = false;

        std::vector<Glib::ustring> apps;
        try
        {
            apps = kf->get_string_list(group.data(), type.data());
            if (apps.empty())
                return;
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            return;
        }

        MimeTypeAction group_block;
        if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::DEFAULT;
        else if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::APPEND;
        else // if (ztd::same(group, "Default Applications"))
            group_block = MimeTypeAction::REMOVE;

        for (usize i = 0; i < apps.size(); ++i)
        {
            if (apps[i].empty())
                continue;

            if (ztd::same(apps.at(i).data(), desktop_id))
            {
                switch (action)
                {
                    case MimeTypeAction::DEFAULT:
                        // found desktop_id already in group list
                        if (group_block == MimeTypeAction::REMOVE)
                        {
                            // Removed Associations - remove it
                            is_present = true;
                            continue;
                        }
                        else
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
                        break;
                    case MimeTypeAction::APPEND:
                        if (group_block == MimeTypeAction::REMOVE)
                        {
                            // Removed Associations - remove it
                            is_present = true;
                            continue;
                        }
                        else
                        {
                            // Default or Added - already present, skip change
                            is_present = true;
                            break;
                        }
                        break;
                    case MimeTypeAction::REMOVE:
                        if (group_block == MimeTypeAction::REMOVE)
                        {
                            // Removed Associations - already present
                            is_present = true;
                            break;
                        }
                        else
                        {
                            // Default or Added - remove it
                            is_present = true;
                            continue;
                        }
                        break;
                    default:
                        break;
                }
            }
            // copy other apps to new list preserving order
            new_action = fmt::format("{}{};", new_action, apps.at(i).data());
        }

        // update key string if needed
        if (action < MimeTypeAction::REMOVE)
        {
            if (((group_block == MimeTypeAction::DEFAULT ||
                  group_block == MimeTypeAction::APPEND) &&
                 !is_present) ||
                (group_block == MimeTypeAction::REMOVE && is_present))
            {
                if (group_block < MimeTypeAction::REMOVE)
                {
                    // add to front of Default or Added list
                    if (action == MimeTypeAction::DEFAULT)
                        new_action = fmt::format("{};{}", desktop_id, new_action);
                    else // if ( action == MimeTypeAction::APPEND )
                        new_action = fmt::format("{}{};", new_action, desktop_id);
                }
                if (!new_action.empty())
                    kf->set_string(group.data(), type.data(), new_action);
                else
                    kf->remove_key(group.data(), type.data());
                data_changed = true;
            }
        }
        else // if ( action == MimeTypeAction::REMOVE )
        {
            if (((group_block == MimeTypeAction::DEFAULT ||
                  group_block == MimeTypeAction::APPEND) &&
                 is_present) ||
                (group_block == MimeTypeAction::REMOVE && !is_present))
            {
                if (group_block == MimeTypeAction::REMOVE)
                {
                    // add to end of Removed list
                    new_action = fmt::format("{}{};", new_action, desktop_id);
                }
                if (!new_action.empty())
                    kf->set_string(group.data(), type.data(), new_action);
                else
                    kf->remove_key(group.data(), type.data());
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
