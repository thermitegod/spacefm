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

#include <ranges>
#include <algorithm>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "vfs/utils/file-ops.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/mime-type/mime-action.hxx"

static void
update_desktop_database() noexcept
{
    const auto path = vfs::user::data() / "applications";
    const std::string command = std::format("update-desktop-database {}", path.string());
    logger::info<logger::domain::vfs>("COMMAND({})", command);
    Glib::spawn_command_line_sync(command);
}

/* Determine removed associations for this type */
static void
remove_actions(const std::string_view mime_type, std::vector<std::string>& actions) noexcept
{
    // logger::info<logger::domain::vfs>("remove_actions( {} )", type);

#if (GTK_MAJOR_VERSION == 4)
    const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
    Glib::KeyFile kf;
#endif
    try
    {
        // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
        const auto path = vfs::user::config() / "mimeapps.list";

#if (GTK_MAJOR_VERSION == 4)
        kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
        kf.load_from_file(path, Glib::KEY_FILE_NONE);
#endif
    }
    catch (const Glib::FileError& e)
    {
        try
        {
            // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
            const auto path = vfs::user::data() / "applications/mimeapps.list";

#if (GTK_MAJOR_VERSION == 4)
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
            kf.load_from_file(path, Glib::KEY_FILE_NONE);
#endif
        }
        catch (const Glib::FileError& e)
        {
            return;
        }
    }

    std::vector<Glib::ustring> removed;
    try
    {
#if (GTK_MAJOR_VERSION == 4)
        removed = kf->get_string_list("Removed Associations", mime_type.data());
#elif (GTK_MAJOR_VERSION == 3)
        removed = kf.get_string_list("Removed Associations", mime_type.data());
#endif
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
        if (std::ranges::contains(actions, rem))
        {
            std::ranges::remove(actions, rem);
            // logger::info<logger::domain::vfs>("        ACTION-REMOVED {}", rem);
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
get_actions(const std::filesystem::path& dir, const std::string_view mime_type,
            std::vector<std::string>& actions) noexcept
{
    // logger::info<logger::domain::vfs>("get_actions( {}, {} )\n", dir, mime_type);
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

    // logger::info<logger::domain::vfs>("get_actions( {}/, {} )", dir, mime_type);
    for (const auto n : std::views::iota(0uz, names.size()))
    {
        const auto path = dir / names.at(n);
        // logger::info<logger::domain::vfs>( "    {}", path);
#if (GTK_MAJOR_VERSION == 4)
        const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
        Glib::KeyFile kf;
#endif
        try
        {
#if (GTK_MAJOR_VERSION == 4)
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
            kf.load_from_file(path, Glib::KEY_FILE_NONE);
#endif
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
#if (GTK_MAJOR_VERSION == 4)
                removed = kf->get_string_list("Removed Associations", mime_type.data());
#elif (GTK_MAJOR_VERSION == 3)
                removed = kf.get_string_list("Removed Associations", mime_type.data());
#endif
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
            // logger::info<logger::domain::vfs>("        {} [{}]", groups[k], k);
            bool is_removed = false;
            std::vector<Glib::ustring> apps;
            try
            {
#if (GTK_MAJOR_VERSION == 4)
                apps = kf->get_string_list(groups.at(k).data(), mime_type.data());
#elif (GTK_MAJOR_VERSION == 3)
                apps = kf.get_string_list(groups.at(k).data(), mime_type.data());
#endif
                // if (apps.empty())
                //     return nullptr;
            }
            catch (...) // Glib::KeyFileError, Glib::FileError
            {
                continue;
            }
            for (auto& a : apps)
            {
                //  logger::info<logger::domain::vfs>("            {}", apps[i]);
                //  check if removed
                is_removed = false;
                if (!removed.empty() && n > 0)
                {
                    for (auto& r : removed)
                    {
                        if (r == a)
                        {
                            // logger::info<logger::domain::vfs>("                REMOVED");
                            is_removed = true;
                            break;
                        }
                    }
                }
                const std::string app = a;
                if (!is_removed && !std::ranges::contains(actions, app))
                {
                    /* check for app existence */
                    if (vfs::detail::mime_type::locate_desktop_file(app))
                    {
                        // logger::info<logger::domain::vfs>("                EXISTS");
                        actions.push_back(app);
                    }
                    else
                    {
                        // logger::info<logger::domain::vfs>("                MISSING");
                    }
                }
            }
        }
    }
}

std::vector<std::string>
vfs::detail::mime_type::get_actions(const std::string_view mime_type) noexcept
{
    std::vector<std::string> actions;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */

    // $XDG_CONFIG_HOME=[~/.config]/mimeapps.list
    ::get_actions(vfs::user::config(), mime_type, actions);

    // $XDG_DATA_HOME=[~/.local]/share/applications/mimeapps.list
    const auto dir = vfs::user::data() / "applications";
    ::get_actions(dir, mime_type, actions);

    // $XDG_DATA_DIRS=[/usr/[local/]share]/applications/mimeapps.list
    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        const auto sdir = sys_dir / "applications";
        ::get_actions(sdir, mime_type, actions);
    }

    /* remove actions for this file type */
    remove_actions(mime_type, actions);

    /* ensure default app is in the list */
    const auto default_app = get_default_action(mime_type);
    if (default_app)
    {
        if (!std::ranges::contains(actions, default_app.value()))
        {
            // default app is not in the list, add it!
            actions.push_back(default_app.value());
        }
        else /* default app is in the list, move it to the first. */
        {
            const auto it = std::ranges::find(actions, default_app.value());
            const auto index = std::ranges::distance(actions.cbegin(), it);
            if (index != 0)
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
[[nodiscard]] static bool
mime_type_has_action(const std::string_view type, const std::string_view desktop_id) noexcept
{
    Glib::ustring cmd;
    Glib::ustring name;

    bool found = false;
    const bool is_desktop = desktop_id.ends_with(".desktop");

    if (is_desktop)
    {
        const auto filename = vfs::detail::mime_type::locate_desktop_file(desktop_id);
        if (!filename)
        {
            return false;
        }

#if (GTK_MAJOR_VERSION == 4)
        const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
        Glib::KeyFile kf;
#endif
        try
        {
#if (GTK_MAJOR_VERSION == 4)
            kf->load_from_file(filename.value(), Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
            kf.load_from_file(filename.value(), Glib::KEY_FILE_NONE);
#endif
        }
        catch (const Glib::FileError& e)
        {
            return false;
        }

        std::vector<Glib::ustring> types;
        try
        {
#if (GTK_MAJOR_VERSION == 4)
            types = kf->get_string_list("Desktop Entry", "MimeType");
#elif (GTK_MAJOR_VERSION == 3)
            types = kf.get_string_list("Desktop Entry", "MimeType");
#endif
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
            if (known_type.data() == type)
            {
                // our mime-type is already found in the desktop file.
                // no further check is needed
                found = true;
                break;
            }
        }

        if (!found) /* get the content of desktop file for comparison */
        {
#if (GTK_MAJOR_VERSION == 4)
            cmd = kf->get_string("Desktop Entry", "Exec");
            name = kf->get_string("Desktop Entry", "Name");
#elif (GTK_MAJOR_VERSION == 3)
            cmd = kf.get_string("Desktop Entry", "Exec");
            name = kf.get_string("Desktop Entry", "Name");
#endif
        }
    }
    else
    {
        cmd = desktop_id.data();
    }

    const std::vector<std::string> actions = vfs::detail::mime_type::get_actions(type);
    if (actions.empty())
    {
        return found;
    }

    for (const std::string_view action : actions)
    {
        /* Try to match directly by desktop_id first */
        if (is_desktop && action == desktop_id)
        {
            found = true;
            break;
        }
        else /* Then, try to match by "Exec" and "Name" keys */
        {
            const auto filename = vfs::detail::mime_type::locate_desktop_file(action);
            if (!filename)
            {
                return false;
            }

#if (GTK_MAJOR_VERSION == 4)
            const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
            Glib::KeyFile kf;
#endif
            try
            {
#if (GTK_MAJOR_VERSION == 4)
                kf->load_from_file(filename.value(), Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
                kf.load_from_file(filename.value(), Glib::KEY_FILE_NONE);
#endif
            }
            catch (const Glib::FileError& e)
            {
                return false;
            }

#if (GTK_MAJOR_VERSION == 4)
            const Glib::ustring cmd2 = kf->get_string("Desktop Entry", "Exec");
#elif (GTK_MAJOR_VERSION == 3)
            const Glib::ustring cmd2 = kf.get_string("Desktop Entry", "Exec");
#endif
            if (cmd == cmd2) /* 2 desktop files have same "Exec" */
            {
                if (is_desktop)
                {
#if (GTK_MAJOR_VERSION == 4)
                    const Glib::ustring name2 = kf->get_string("Desktop Entry", "Name");
#elif (GTK_MAJOR_VERSION == 3)
                    const Glib::ustring name2 = kf.get_string("Desktop Entry", "Name");
#endif
                    /* Then, check if the "Name" keys of 2 desktop files are the same. */
                    if (name == name2)
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

[[nodiscard]] static std::string
make_custom_desktop_file(const std::string_view desktop_id,
                         const std::string_view mime_type) noexcept
{
    std::string cust_template;
    Glib::ustring file_content;

    static constexpr const std::string_view desktop_ext{".desktop"};
    static constexpr const std::string_view replace_txt{"<REPLACE_TXT>"};

    if (desktop_id.ends_with(desktop_ext))
    {
        const auto filename = vfs::detail::mime_type::locate_desktop_file(desktop_id);
        if (!filename)
        {
            return "";
        }

#if (GTK_MAJOR_VERSION == 4)
        const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
        Glib::KeyFile kf;
#endif
        try
        {
#if (GTK_MAJOR_VERSION == 4)
            kf->load_from_file(filename.value(), Glib::KeyFile::Flags::KEEP_TRANSLATIONS);
#elif (GTK_MAJOR_VERSION == 3)
            kf.load_from_file(filename.value(), Glib::KEY_FILE_KEEP_TRANSLATIONS);
#endif
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

#if (GTK_MAJOR_VERSION == 4)
        /* set our mime-type */
        kf->set_string_list("Desktop Entry", "MimeType", mime_types);
        /* store id of original desktop file, for future use. */
        kf->set_string("Desktop Entry", "X-MimeType-Derived", desktop_id.data());
        kf->set_string("Desktop Entry", "NoDisplay", "true");
#elif (GTK_MAJOR_VERSION == 3)
        /* set our mime-type */
        kf.set_string_list("Desktop Entry", "MimeType", mime_types);
        /* store id of original desktop file, for future use. */
        kf.set_string("Desktop Entry", "X-MimeType-Derived", desktop_id.data());
        kf.set_string("Desktop Entry", "NoDisplay", "true");
#endif

        const std::string name = ztd::removesuffix(desktop_id, desktop_ext);
        cust_template = std::format("{}-usercustom-{}.desktop", name, replace_txt);

#if (GTK_MAJOR_VERSION == 4)
        file_content = kf->to_data();
#elif (GTK_MAJOR_VERSION == 3)
        file_content = kf.to_data();
#endif
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
    const auto dir = vfs::user::data() / "applications";
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    std::string cust;

    for (i32 i = 0;; ++i)
    {
        /* generate the basename */
        cust = ztd::replace(cust_template, replace_txt, std::format("{}", i));
        /* test if the filename already exists */
        const auto path = dir / cust;
        if (!std::filesystem::exists(path))
        { /* this generated filename can be used */
            [[maybe_unused]] auto ec = vfs::utils::write_file(path, file_content);
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
std::string
vfs::detail::mime_type::add_action(const std::string_view type,
                                   const std::string_view desktop_id) noexcept
{
    if (mime_type_has_action(type, desktop_id))
    {
        return desktop_id.data();
    }
    return make_custom_desktop_file(desktop_id, type);
}

[[nodiscard]] static std::optional<std::filesystem::path>
locate_desktop_file(const std::filesystem::path& dir, const std::string_view desktop_id) noexcept
{
    auto desktop_path = dir / "applications" / desktop_id;
    if (std::filesystem::is_regular_file(desktop_path))
    {
        return desktop_path;
    }

    // logger::info<logger::domain::vfs>("desktop_id={}", desktop_id);

    // mime encodes directory separators as '-'.
    // so the desktop_id 'mime-mime-mime.desktop' could be on-disk
    // mime-mime-mime.desktop, mime/mime-mime.desktop, or mime/mime/mime.desktop.
    // not supported is mime-mime/mime.desktop
    std::string new_desktop_id = desktop_id.data();
    while (new_desktop_id.contains('-'))
    {
        new_desktop_id = ztd::replace(new_desktop_id, "-", "/", 1);
        auto new_desktop_path = dir / "applications" / new_desktop_id;
        // logger::info<logger::domain::vfs>("new_desktop_id={}", new_desktop_id);
        if (std::filesystem::is_regular_file(new_desktop_path))
        {
            return new_desktop_path;
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path>
vfs::detail::mime_type::locate_desktop_file(const std::filesystem::path& dir,
                                            const std::string_view desktop_id) noexcept
{
    return ::locate_desktop_file(dir, desktop_id);
}

std::optional<std::filesystem::path>
vfs::detail::mime_type::locate_desktop_file(const std::string_view desktop_id) noexcept
{
    const auto data_dir = vfs::user::data();

    auto data_desktop = ::locate_desktop_file(data_dir, desktop_id);
    if (data_desktop)
    {
        return data_desktop;
    }

    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        auto sys_desktop = ::locate_desktop_file(sys_dir, desktop_id);
        if (sys_desktop)
        {
            return sys_desktop;
        }
    }

    return std::nullopt;
}

std::optional<std::string>
vfs::detail::mime_type::get_default_action(const std::string_view mime_type) noexcept
{
    assert(mime_type.empty() != true);

    const auto command = std::format("xdg-mime query default {}", mime_type);
    // logger::debug<logger::domain::vfs>("COMMAND({})", command);
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, nullptr);
    if (standard_output.empty())
    {
        return std::nullopt;
    }
    // Need to remove '\n'
    return ztd::strip(standard_output);
}

void
vfs::detail::mime_type::set_default_action(const std::string_view mime_type,
                                           const std::string_view desktop_id) noexcept
{
    assert(mime_type.empty() != true);
    assert(desktop_id.empty() != true);

    const auto command = std::format("xdg-mime default {} {}", desktop_id, mime_type);
    logger::debug<logger::domain::vfs>("COMMAND({})", command);
    Glib::spawn_command_line_sync(command);
}
