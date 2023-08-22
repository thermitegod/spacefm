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

#include <format>

#include <filesystem>

#include <optional>

#include <array>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "write.hxx"
#include "utils.hxx"

#include "mime-type/mime-action.hxx"

static void
save_to_file(const std::filesystem::path& path, const Glib::ustring& data)
{
    write_file(path, data);

    if (std::filesystem::exists(path))
    {
        std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    }
}

static void
update_desktop_database()
{
    const auto path = vfs::user_dirs->data_dir() / "applications";
    const std::string command = std::format("update-desktop-database {}", path.string());
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_sync(command);
}

/* Determine removed associations for this type */
static void
remove_actions(const std::string_view mime_type, std::vector<std::string>& actions)
{
    // ztd::logger::info("remove_actions( {} )", type);

    const auto kf = Glib::KeyFile::create();
    try
    {
        // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
        const auto path = vfs::user_dirs->config_dir() / "mimeapps.list";

        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
    }
    catch (const Glib::FileError& e)
    {
        try
        {
            // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
            const auto path = vfs::user_dirs->data_dir() / "applications/mimeapps.list";

            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (const Glib::FileError& e)
        {
            return;
        }
    }

    std::vector<Glib::ustring> removed;
    try
    {
        removed = kf->get_string_list("Removed Associations", mime_type.data());
        if (removed.empty())
        {
            return;
        }
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
            // ztd::logger::info("        ACTION-REMOVED {}", rem);
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
get_actions(const std::filesystem::path& dir, const std::string_view type,
            std::vector<std::string>& actions)
{
    // ztd::logger::info("get_actions( {}, {} )\n", dir, type);
    std::vector<Glib::ustring> removed;

    static constexpr std::array<const std::string_view, 2> names{
        "mimeapps.list",
        "mimeinfo.cache",
    };
    static constexpr std::array<const std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
        "MIME Cache",
    };

    // ztd::logger::info("get_actions( {}/, {} )", dir, type);
    for (const auto n : ztd::range(names.size()))
    {
        const auto path = dir / names.at(n);
        // ztd::logger::info( "    {}", path);
        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (const Glib::FileError& e)
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
            // ztd::logger::info("        {} [{}]", groups[k], k);
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
                //  ztd::logger::info("            {}", apps[i]);
                //  check if removed
                is_removed = false;
                if (!removed.empty() && n > 0)
                {
                    for (auto& r : removed)
                    {
                        if (ztd::same(r.data(), a.data()))
                        {
                            // ztd::logger::info("                REMOVED");
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
                        // ztd::logger::info("                EXISTS");
                        actions.emplace_back(app);
                    }
                    else
                    {
                        // ztd::logger::info("                MISSING");
                    }
                }
            }
        }
    }
}

const std::vector<std::string>
mime_type_get_actions(const std::string_view mime_type)
{
    std::vector<std::string> actions;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    get_actions(vfs::user_dirs->config_dir(), mime_type, actions);

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    const auto dir = vfs::user_dirs->data_dir() / "applications";
    get_actions(dir, mime_type, actions);

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const auto sdir = std::filesystem::path() / sys_dir / "applications";
        get_actions(sdir, mime_type, actions);
    }

    /* remove actions for this file type */ // sfm
    remove_actions(mime_type, actions);

    /* ensure default app is in the list */
    const auto check_default_app = mime_type_get_default_action(mime_type);
    if (check_default_app)
    {
        const auto& default_app = check_default_app.value();
        if (!ztd::contains(actions, default_app))
        {
            // default app is not in the list, add it!
            actions.emplace_back(default_app);
        }
        else /* default app is in the list, move it to the first. */
        {
            if (const usize index = ztd::index(actions, default_app) != 0)
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
mime_type_has_action(const std::string_view type, const std::string_view desktop_id)
{
    Glib::ustring cmd;
    Glib::ustring name;

    bool found = false;
    const bool is_desktop = ztd::endswith(desktop_id.data(), ".desktop");

    if (is_desktop)
    {
        const auto check_filename = mime_type_locate_desktop_file(desktop_id);
        if (!check_filename)
        {
            return false;
        }
        const auto& filename = check_filename.value();

        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(filename, Glib::KeyFile::Flags::NONE);
        }
        catch (const Glib::FileError& e)
        {
            return false;
        }

        std::vector<Glib::ustring> types;
        try
        {
            types = kf->get_string_list("Desktop Entry", "MimeType");
            if (types.empty())
            {
                return false;
            }
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            return false;
        }

        for (const Glib::ustring& known_type : types)
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
    {
        return found;
    }

    for (const std::string_view action : actions)
    {
        /* Try to match directly by desktop_id first */
        if (is_desktop && ztd::same(action, desktop_id))
        {
            found = true;
            break;
        }
        else /* Then, try to match by "Exec" and "Name" keys */
        {
            const auto check_filename = mime_type_locate_desktop_file(action);
            if (!check_filename)
            {
                return false;
            }
            const auto& filename = check_filename.value();

            const auto kf = Glib::KeyFile::create();
            try
            {
                kf->load_from_file(filename, Glib::KeyFile::Flags::NONE);
            }
            catch (const Glib::FileError& e)
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
make_custom_desktop_file(const std::string_view desktop_id, const std::string_view mime_type)
{
    std::string cust_template;
    Glib::ustring file_content;

    static constexpr const std::string_view desktop_ext{".desktop"};
    static constexpr const std::string_view replace_txt{"<REPLACE_TXT>"};

    if (ztd::endswith(desktop_id, desktop_ext))
    {
        const auto check_filename = mime_type_locate_desktop_file(desktop_id);
        if (!check_filename)
        {
            return "";
        }
        const auto& filename = check_filename.value();

        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(filename, Glib::KeyFile::Flags::KEEP_TRANSLATIONS);
        }
        catch (const Glib::FileError& e)
        {
            return "";
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
        cust_template = std::format("{}-usercustom-{}.desktop", name, replace_txt);

        file_content = kf->to_data();
    }
    else /* it is not a desktop_id, but a command */
    {
        /* Make a user-created desktop file for the command */
        const auto name = std::filesystem::path(desktop_id).filename();
        cust_template = std::format("{}-usercreated-{}.desktop", name.string(), replace_txt);

        file_content = std::format("[Desktop Entry]\n"
                                   "Type=Application"
                                   "Name={}\n"
                                   "Exec={}\n"
                                   "MimeType={}\n"
                                   "Icon=exec\n"
                                   "Terminal=false\n"
                                   "NoDisplay=true\n",
                                   name.string(),
                                   desktop_id,
                                   mime_type);
    }

    /* generate unique file name */
    const auto dir = vfs::user_dirs->data_dir() / "applications";
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    std::string cust;

    for (i32 i = 0;; ++i)
    {
        /* generate the basename */
        cust = ztd::replace(cust_template, replace_txt, std::to_string(i));
        /* test if the filename already exists */
        const auto path = dir / cust;
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
mime_type_add_action(const std::string_view type, const std::string_view desktop_id)
{
    if (mime_type_has_action(type, desktop_id))
    {
        return desktop_id.data();
    }

    return make_custom_desktop_file(desktop_id, type);
}

static const std::optional<std::filesystem::path>
locate_desktop_file(const std::filesystem::path& dir, const std::string_view desktop_id)
{
    const auto desktop_path = dir / "applications" / desktop_id;
    if (std::filesystem::is_regular_file(desktop_path))
    {
        return desktop_path;
    }

    // ztd::logger::info("desktop_id={}", desktop_id);

    // mime encodes directory separators as '-'.
    // so the desktop_id 'mime-mime-mime.desktop' could be on-disk
    // mime-mime-mime.desktop, mime/mime-mime.desktop, or mime/mime/mime.desktop.
    // not supported is mime-mime/mime.desktop
    std::string new_desktop_id = desktop_id.data();
    while (ztd::contains(new_desktop_id, "-"))
    {
        new_desktop_id = ztd::replace(new_desktop_id, "-", "/", 1);
        const auto new_desktop_path = dir / "applications" / new_desktop_id;
        // ztd::logger::info("new_desktop_id={}", new_desktop_id);
        if (std::filesystem::is_regular_file(new_desktop_path))
        {
            return new_desktop_path;
        }
    }

    return std::nullopt;
}

const std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::filesystem::path& dir, const std::string_view desktop_id)
{
    return locate_desktop_file(dir, desktop_id);
}

const std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::string_view desktop_id)
{
    const auto data_dir = vfs::user_dirs->data_dir();

    const auto data_desktop = locate_desktop_file(data_dir, desktop_id);
    if (data_desktop)
    {
        return data_desktop.value();
    }

    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const auto sys_desktop = locate_desktop_file(sys_dir, desktop_id);
        if (sys_desktop)
        {
            return sys_desktop.value();
        }
    }

    return std::nullopt;
}

static const std::optional<std::string>
get_default_action(const std::filesystem::path& dir, const std::string_view type)
{
    // ztd::logger::info("get_default_action( {}, {} )", dir, type);
    // search these files in dir for the first existing default app
    static constexpr std::array<const std::string_view, 2> names{
        "mimeapps.list",
        "defaults.list",
    };
    static constexpr std::array<const std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
    };

    for (const std::string_view name : names)
    {
        const auto path = dir / name;
        // ztd::logger::info("    path = {}", path);
        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (const Glib::FileError& e)
        {
            return std::nullopt;
        }

        for (const std::string_view group : groups)
        {
            std::vector<Glib::ustring> apps;
            try
            {
                apps = kf->get_string_list(group.data(), type.data());
                if (apps.empty())
                {
                    break;
                }
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                break;
            }

            for (const Glib::ustring& app : apps)
            {
                if (app.empty())
                {
                    continue;
                }

                // ztd::logger::info("        {}", apps[i]);
                if (mime_type_locate_desktop_file(app.data()))
                {
                    // ztd::logger::info("            EXISTS");
                    return app.data();
                }
            }

            if (ztd::same(name, "defaults.list"))
            {
                break; // defaults.list does not have Added Associations
            }
        }

        if (std::filesystem::equivalent(dir, vfs::user_dirs->config_dir()))
        {
            break; // no defaults.list in ~/.config
        }
    }
    return std::nullopt;
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
const std::optional<std::string>
mime_type_get_default_action(const std::string_view mime_type)
{
    /* FIXME: need to check parent types if default action of current type is not set. */

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    auto home_default_action = get_default_action(vfs::user_dirs->config_dir(), mime_type.data());
    if (home_default_action)
    {
        return home_default_action;
    }

    // $XDG_DATA_HOME=[~/.local]/applications/mimeapps.list
    const auto data_app_dir = vfs::user_dirs->data_dir() / "applications";
    auto data_default_action = get_default_action(data_app_dir, mime_type.data());
    if (data_default_action)
    {
        return data_default_action;
    }

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (const std::string_view sys_dir : vfs::user_dirs->system_data_dirs())
    {
        const auto sys_app_dir = std::filesystem::path() / sys_dir / "applications";
        auto sys_default_action = get_default_action(sys_app_dir, mime_type.data());
        if (sys_default_action)
        {
            return sys_default_action;
        }
    }

    return std::nullopt;
}

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     mime_type::action::DEFAULT - make desktop_id the default app
 *     mime_type::action::APPEND  - add desktop_id to Default and Added apps
 *     mime_type::action::REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void
mime_type_update_association(const std::string_view type, const std::string_view desktop_id,
                             mime_type::action action)
{
    if (type.empty() || desktop_id.empty())
    {
        ztd::logger::warn("mime_type_update_association invalid type or desktop_id");
        return;
    }

    bool data_changed = false;

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    const auto mimeapps = vfs::user_dirs->config_dir() / "mimeapps.list";

    if (!std::filesystem::exists(mimeapps))
    {
        ztd::logger::info("Creating empty mime list: {}", mimeapps);
        save_to_file(mimeapps, "[Default Applications]\n");
    }

    // Load current mimeapps.list content, if available
    const auto kf = Glib::KeyFile::create();
    try
    {
        kf->load_from_file(mimeapps, Glib::KeyFile::Flags::NONE);
    }
    catch (const Glib::FileError& e)
    {
        return;
    }

    static constexpr std::array<const std::string_view, 3> groups{
        "Default Applications",
        "Added Associations",
        "Removed Associations",
    };

    for (const std::string_view group : groups)
    {
        std::string new_action;
        bool is_present = false;

        std::vector<Glib::ustring> apps;
        try
        {
            apps = kf->get_string_list(group.data(), type.data());
            if (apps.empty())
            {
                continue;
            }
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            continue;
        }

        mime_type::action group_block;
        if (ztd::same(group, groups[0]))
        {
            group_block = mime_type::action::DEFAULT;
        }
        else if (ztd::same(group, groups[1]))
        {
            group_block = mime_type::action::append;
        }
        else
        { // if (ztd::same(group, groups[2]))
            group_block = mime_type::action::remove;
        }

        for (const auto i : ztd::range(apps.size()))
        {
            if (apps[i].empty())
            {
                continue;
            }

            if (ztd::same(apps.at(i).data(), desktop_id))
            {
                switch (action)
                {
                    case mime_type::action::DEFAULT:
                        // found desktop_id already in group list
                        if (group_block == mime_type::action::remove)
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
                    case mime_type::action::append:
                        if (group_block == mime_type::action::remove)
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
                    case mime_type::action::remove:
                        if (group_block == mime_type::action::remove)
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
                }
            }
            // copy other apps to new list preserving order
            new_action = std::format("{}{};", new_action, apps.at(i).data());
        }

        // update key string if needed
        if (action < mime_type::action::remove)
        {
            if (((group_block == mime_type::action::DEFAULT ||
                  group_block == mime_type::action::append) &&
                 !is_present) ||
                (group_block == mime_type::action::remove && is_present))
            {
                if (group_block < mime_type::action::remove)
                {
                    // add to front of Default or Added list
                    if (action == mime_type::action::DEFAULT)
                    {
                        new_action = std::format("{};{}", desktop_id, new_action);
                    }
                    else
                    { // if ( action == mime_type::action::APPEND )
                        new_action = std::format("{}{};", new_action, desktop_id);
                    }
                }
                if (!new_action.empty())
                {
                    kf->set_string(group.data(), type.data(), new_action);
                }
                else
                {
                    kf->remove_key(group.data(), type.data());
                }
                data_changed = true;
            }
        }
        else // if ( action == mime_type::action::REMOVE )
        {
            if (((group_block == mime_type::action::DEFAULT ||
                  group_block == mime_type::action::append) &&
                 is_present) ||
                (group_block == mime_type::action::remove && !is_present))
            {
                if (group_block == mime_type::action::remove)
                {
                    // add to end of Removed list
                    new_action = std::format("{}{};", new_action, desktop_id);
                }
                if (!new_action.empty())
                {
                    kf->set_string(group.data(), type.data(), new_action);
                }
                else
                {
                    kf->remove_key(group.data(), type.data());
                }
                data_changed = true;
            }
        }
    }

    // save updated mimeapps.list
    if (data_changed)
    {
        const Glib::ustring data = kf->to_data();
        save_to_file(mimeapps, data);
    }
}
