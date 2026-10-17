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

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/execute.hxx"
#include "vfs/mime-action.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

// https://specifications.freedesktop.org/mime-apps/latest/

namespace
{
[[nodiscard]] std::optional<std::filesystem::path>
locate_desktop_file(std::string_view desktop_id) noexcept
{
    if (desktop_id.empty())
    {
        return std::nullopt;
    }

    const auto find_in =
        [desktop_id](const std::filesystem::path& dir) -> std::optional<std::filesystem::path>
    {
        const auto path = dir / "applications" / desktop_id;
        if (std::filesystem::is_regular_file(path))
        {
            return path;
        }
        return std::nullopt;
    };

    auto path = find_in(vfs::user::data());
    if (path)
    {
        return path;
    }

    for (const auto& dir : Glib::get_system_data_dirs())
    {
        auto path = find_in(dir);
        if (path)
        {
            return path;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::vector<std::filesystem::path>
mime_apps_files() noexcept
{
    const auto config_dirs = Glib::get_system_config_dirs();
    const auto data_dirs = Glib::get_system_data_dirs();

    std::vector<std::filesystem::path> files;
    files.reserve(2 + config_dirs.size() + data_dirs.size());

    constexpr auto mimeapps_list = "mimeapps.list";

    files.emplace_back(vfs::user::config() / mimeapps_list);
    for (const std::filesystem::path dir : config_dirs)
    {
        files.emplace_back(dir / mimeapps_list);
    }

    files.emplace_back(vfs::user::data() / "applications" / mimeapps_list);
    for (const std::filesystem::path dir : data_dirs)
    {
        files.emplace_back(dir / "applications" / mimeapps_list);
    }

    return files;
}

[[nodiscard]] std::optional<std::string>
resolve_default(std::string_view mime_type) noexcept
{
    const std::string key{mime_type};

    for (const auto& path : mime_apps_files())
    {
        std::vector<Glib::ustring> values;

        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
            values = kf->get_string_list("Default Applications", key);
        }
        catch (...)
        {
            continue;
        }

        for (const auto& value : values)
        {
            auto desktop_id{value.raw()};
            if (locate_desktop_file(desktop_id))
            {
                return desktop_id;
            }
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::vector<std::string>
mime_parent_types(std::string_view mime_type) noexcept
{
    std::vector<std::string> parents;
    std::unordered_set<std::string> seen;
    for (const auto& parent_mime : Gio::content_types_get_registered())
    {
        if (Gio::content_type_equals(parent_mime, mime_type.data()) ||
            !Gio::content_type_is_a(mime_type.data(), parent_mime))
        {
            continue;
        }

        const auto [_, inserted] = seen.emplace(parent_mime);
        if (inserted)
        {
            parents.emplace_back(parent_mime);
        }
    }
    return parents;
}

[[nodiscard]] std::vector<std::string>
actions_for_type(std::string_view mime_type) noexcept
{
    std::vector<std::string> actions;

    const std::string key{mime_type};
    const auto files = mime_apps_files();

    struct action_data final
    {
        std::vector<std::string> removed;
        std::vector<std::string> defaults;
        std::vector<std::string> added;
    };

    std::vector<action_data> associations;
    associations.reserve(files.size());

    for (const auto& path : files)
    {
        action_data entry;

        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);
        }
        catch (...)
        {
            continue;
        }

        const auto read_list = [&kf, &key](const char* group, std::vector<std::string>& destination)
        {
            try
            {
                const auto values = kf->get_string_list(group, key);
                destination.reserve(values.size());
                for (const auto& value : values)
                {
                    destination.emplace_back(value);
                }
            }
            catch (...)
            {
                // missing group/key
            }
        };

        read_list("Removed Applications", entry.removed);
        read_list("Default Applications", entry.defaults);
        read_list("Added Associations", entry.added);

        associations.emplace_back(std::move(entry));
    }

    std::unordered_set<std::string> removed;
    std::unordered_set<std::string> seen;

    for (const auto& entry : associations)
    {
        removed.insert(entry.removed.begin(), entry.removed.end());
    }

    const auto append_id = [&actions, &removed, &seen](const std::string& desktop_id)
    {
        if (removed.contains(desktop_id) || seen.contains(desktop_id))
        {
            return;
        }

        if (locate_desktop_file(desktop_id))
        {
            seen.insert(desktop_id);
            actions.push_back(desktop_id);
        }
    };

    for (const auto& entry : associations)
    {
        for (const auto& desktop_id : entry.defaults)
        {
            append_id(desktop_id);
        }

        for (const auto& desktop_id : entry.added)
        {
            append_id(desktop_id);
        }
    }

    const auto append_cache = [&key, &append_id](const std::filesystem::path& dir)
    {
        const auto path = dir / "mimeinfo.cache";
        const auto kf = Glib::KeyFile::create();

        try
        {
            kf->load_from_file(path, Glib::KeyFile::Flags::NONE);

            const auto values = kf->get_string_list("MIME Cache", key);
            for (const auto& value : values)
            {
                append_id(value.raw());
            }
        }
        catch (...)
        {
            // missing group/key
        }
    };

    append_cache(vfs::user::data() / "applications");

    for (const std::filesystem::path dir : Glib::get_system_data_dirs())
    {
        append_cache(dir / "applications");
    }

    if (const auto default_id = resolve_default(mime_type))
    {
        const auto it = std::ranges::find(actions, *default_id);
        if (it != actions.cend())
        {
            actions.erase(it);
        }

        actions.insert(actions.begin(), *default_id);
    }

    return actions;
}

[[nodiscard]] static bool
mime_type_has_action(std::string_view type, std::string_view desktop_id) noexcept
{
    const bool is_desktop = desktop_id.ends_with(".desktop");

    std::string cmd;
    std::string name;

    if (is_desktop)
    {
        const auto filename = locate_desktop_file(desktop_id);
        if (!filename)
        {
            return false;
        }

        const auto kf = Glib::KeyFile::create();
        try
        {
            kf->load_from_file(*filename, Glib::KeyFile::Flags::NONE);

            const auto types = kf->get_string_list("Desktop Entry", "MimeType");
            for (const auto& value : types)
            {
                if (value.raw() == type)
                {
                    return true;
                }
            }

            cmd = kf->get_string("Desktop Entry", "Exec");
            name = kf->get_string("Desktop Entry", "Name");
        }
        catch (...)
        {
            return false;
        }
    }
    else
    {
        cmd = desktop_id;
    }

    const auto actions = vfs::mime_action::actions(type);

    for (const std::string_view action : actions)
    {
        if (is_desktop && action == desktop_id)
        {
            return true;
        }

        const auto filename = locate_desktop_file(action);
        if (!filename)
        {
            continue;
        }

        const auto kf = Glib::KeyFile::create();

        try
        {
            kf->load_from_file(*filename, Glib::KeyFile::Flags::NONE);

            const auto cmd2 = kf->get_string("Desktop Entry", "Exec");
            if (cmd != cmd2)
            {
                continue;
            }

            if (!is_desktop)
            {
                return true;
            }

            const auto name2 = kf->get_string("Desktop Entry", "Name");
            if (name == name2)
            {
                return true;
            }
        }
        catch (...)
        {
            continue;
        }
    }

    return false;
}

[[nodiscard]] static std::optional<std::string>
make_custom_desktop_file(std::string_view desktop_id, std::string_view mime_type) noexcept
{
    static constexpr auto DESKTOP_ENTRY_GROUP = "Desktop Entry";

    const bool is_desktop = desktop_id.ends_with(".desktop");

    const std::string mime{mime_type};
    const std::string id{desktop_id};

    std::string prefix;

    const auto kf = Glib::KeyFile::create();

    if (is_desktop)
    {
        const auto filename = locate_desktop_file(desktop_id);
        if (!filename)
        {
            return std::nullopt;
        }

        try
        {
            kf->load_from_file(*filename, Glib::KeyFile::Flags::KEEP_TRANSLATIONS);
        }
        catch (...)
        {
            return std::nullopt;
        }

        kf->set_string_list(DESKTOP_ENTRY_GROUP, "MimeType", std::vector<Glib::ustring>{mime});
        kf->set_string(DESKTOP_ENTRY_GROUP, "X-MimeType-Derived", id);
        kf->set_string(DESKTOP_ENTRY_GROUP, "NoDisplay", "true");

        const std::string name = ztd::remove_suffix(id, ".desktop");
        prefix = std::format("{}-usercustom", name);
    }
    else
    {
        const auto executable = std::filesystem::path(id).filename().string();

        kf->set_string(DESKTOP_ENTRY_GROUP, "Type", "Application");
        kf->set_string(DESKTOP_ENTRY_GROUP, "Name", executable);
        kf->set_string(DESKTOP_ENTRY_GROUP, "Exec", id);
        kf->set_string_list(DESKTOP_ENTRY_GROUP, "MimeType", std::vector<Glib::ustring>{mime});
        kf->set_string(DESKTOP_ENTRY_GROUP, "Icon", "exec");
        kf->set_string(DESKTOP_ENTRY_GROUP, "Terminal", "false");
        kf->set_string(DESKTOP_ENTRY_GROUP, "NoDisplay", "true");

        prefix = std::format("{}-usercreated-", executable);
    }

    const auto file_content = kf->to_data();

    const auto dir = vfs::user::data() / "applications";

    if (!std::filesystem::exists(dir))
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec || !std::filesystem::is_directory(dir, ec) || ec)
        {
            logger::error<logger::vfs>("Failed to create directory {}", dir);
            return std::nullopt;
        }
    }

    for (const auto i : std::views::iota(0, 1024))
    {
        const auto custom = std::format("{}-{}.desktop", prefix, i);
        const auto path = dir / custom;

        if (std::filesystem::exists(path))
        {
            continue;
        }

        const auto ec = vfs::utils::write_file(path, file_content.raw());
        if (!ec)
        {
            vfs::execute::command_line_async("update-desktop-database {}", dir);

            return custom;
        }
    }

    return std::nullopt;
}
} // namespace

std::vector<std::string>
vfs::mime_action::actions(std::string_view mime_type) noexcept
{
    std::vector<std::string> result;

    if (mime_type.empty())
    {
        return result;
    }

    std::unordered_set<std::string> seen;

    const auto append = [&](std::string_view type)
    {
        for (auto& id : actions_for_type(type))
        {
            const auto [_, inserted] = seen.emplace(id);
            if (inserted)
            {
                result.push_back(std::move(id));
            }
        }
    };

    append(mime_type);
    for (const auto& parent : mime_parent_types(mime_type))
    {
        append(parent);
    }

    return result;
}

std::optional<std::string>
vfs::mime_action::add_action(std::string_view mime_type, std::string_view desktop_id) noexcept
{
    if (mime_type.empty())
    {
        return std::nullopt;
    }

    if (mime_type_has_action(mime_type, desktop_id))
    {
        return std::string{desktop_id};
    }
    return make_custom_desktop_file(desktop_id, mime_type);
}

std::optional<std::string>
vfs::mime_action::default_action(std::string_view mime_type) noexcept
{
    if (mime_type.empty())
    {
        return std::nullopt;
    }
    return resolve_default(mime_type);
}

void
vfs::mime_action::set_default_action(std::string_view mime_type,
                                     std::string_view desktop_id) noexcept
{
    if (mime_type.empty() || desktop_id.empty())
    {
        return;
    }

    vfs::execute::command_line_async("xdg-mime default {} {}", desktop_id, mime_type);
}
