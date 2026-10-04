/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cstdint>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/app-desktop.hxx"
#include "vfs/error.hxx"
#include "vfs/execute.hxx"
#include "vfs/file.hxx"

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

namespace
{
struct desktop_cache_data final
{
    std::shared_ptr<vfs::desktop> desktop;
    std::chrono::system_clock::time_point mtime;
};

std::mutex desktops_cache_mutex;
std::unordered_map<std::filesystem::path, desktop_cache_data> desktops_cache;
} // namespace

std::shared_ptr<vfs::desktop>
vfs::desktop::create(const std::filesystem::path& path) noexcept
{
    std::scoped_lock lock(desktops_cache_mutex);

    {
        const auto it = desktops_cache.find(path);
        if (it != desktops_cache.cend())
        {
            // logger::trace<logger::vfs>("vfs::desktop({})  cache   {}", logger::utils::ptr(it->second.desktop), path);
            const auto stat = ztd::stat::create(it->second.desktop->path());
            if (stat && stat->mtime() == it->second.mtime)
            {
                return it->second.desktop;
            }
            // logger::trace<logger::vfs>("vfs::desktop({})  changed on disk, reloading", logger::utils::ptr(it->second.desktop));
        }
    }

    struct hack : public vfs::desktop
    {
        hack(const std::filesystem::path& path) : desktop(path) {}
    };

    std::shared_ptr<vfs::desktop> desktop;
    try
    {
        desktop = std::make_shared<hack>(path);
    }
    catch (...)
    {
        return nullptr;
    }

    const auto stat = ztd::stat::create(desktop->path());
    if (!stat)
    {
        return nullptr;
    }

    const auto [it, _] = desktops_cache.insert_or_assign(path,
                                                         desktop_cache_data{
                                                             std::move(desktop),
                                                             stat->mtime(),
                                                         });
    // logger::trace<logger::vfs>("vfs::desktop({})  new     {}", logger::utils::ptr(it->second.desktop), path);
    return it->second.desktop;
}

vfs::desktop::desktop(const std::filesystem::path& path) : filename_(path.filename()), path_(path)
{
    // logger::info<logger::vfs>("vfs::desktop::desktop({})", logger::utils::ptr(this));

    auto result = parse_desktop_file();
    if (result != vfs::error_code::none)
    {
        throw std::runtime_error("Failed to parse");
    }
}

vfs::error_code
vfs::desktop::parse_desktop_file() noexcept
{
    static constexpr auto DESKTOP_ENTRY_GROUP = "Desktop Entry";

    static constexpr auto DESKTOP_ENTRY_KEY_TYPE = "Type";
    static constexpr auto DESKTOP_ENTRY_KEY_NAME = "Name";
    static constexpr auto DESKTOP_ENTRY_KEY_GENERICNAME = "GenericName";
    static constexpr auto DESKTOP_ENTRY_KEY_NODISPLAY = "NoDisplay";
    static constexpr auto DESKTOP_ENTRY_KEY_COMMENT = "Comment";
    static constexpr auto DESKTOP_ENTRY_KEY_ICON = "Icon";
    static constexpr auto DESKTOP_ENTRY_KEY_TRYEXEC = "TryExec";
    static constexpr auto DESKTOP_ENTRY_KEY_EXEC = "Exec";
    static constexpr auto DESKTOP_ENTRY_KEY_PATH = "Path";
    static constexpr auto DESKTOP_ENTRY_KEY_TERMINAL = "Terminal";
    static constexpr auto DESKTOP_ENTRY_KEY_ACTIONS = "Actions";
    static constexpr auto DESKTOP_ENTRY_KEY_MIMETYPE = "MimeType";
    static constexpr auto DESKTOP_ENTRY_KEY_CATEGORIES = "Categories";
    static constexpr auto DESKTOP_ENTRY_KEY_KEYWORDS = "Keywords";
    static constexpr auto DESKTOP_ENTRY_KEY_STARTUPNOTIFY = "StartupNotify";

    bool loaded = false;

    const auto kf = Glib::KeyFile::create();

    if (path_.is_absolute())
    {
        loaded = kf->load_from_file(path_, Glib::KeyFile::Flags::NONE);
    }
    else
    {
        const auto relative_path = std::filesystem::path() / "applications" / filename_;
        std::string relative_full_path;
        try
        {
            loaded = kf->load_from_data_dirs(relative_path, relative_full_path);
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            logger::error<logger::vfs>("Error opening desktop file: {}", path_);
            return vfs::error_code::file_open_failure;
        }
        path_ = relative_full_path;
    }

    if (!loaded)
    {
        logger::error<logger::vfs>("Failed to load desktop file: {}", path_);
        return vfs::error_code::parse_error;
    }

    // Keys not loaded from .desktop files
    // - Hidden
    // - OnlyShowIn
    // - NotShowIn
    // - DBusActivatable
    // - StartupWMClass
    // - URL
    // - PrefersNonDefaultGPU
    // - SingleMainWindow

    // clang-format off

    // Required Keys, must fail if missing

    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE))
    {
        desktop_entry_.type = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME))
    {
        desktop_entry_.name = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    // Optional Keys

    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME))
    {
        desktop_entry_.generic_name = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY))
    {
        desktop_entry_.no_display = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT))
    {
        desktop_entry_.comment = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON))
    {
        desktop_entry_.icon = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC))
    {
        desktop_entry_.try_exec = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC))
    {
        desktop_entry_.exec = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH))
    {
        desktop_entry_.path = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL))
    {
         desktop_entry_.terminal = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS))
    {
        desktop_entry_.actions = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE))
    {
        desktop_entry_.mime_type = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES))
    {
        desktop_entry_.categories = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS))
    {
        desktop_entry_.keywords = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY))
    {
        desktop_entry_.startup_notify = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY);
    }
    // clang-format on

    return loaded ? vfs::error_code::none : vfs::error_code::parse_error;
}

std::string_view
vfs::desktop::name() const noexcept
{
    return filename_;
}

std::string_view
vfs::desktop::display_name() const noexcept
{
    if (!desktop_entry_.name.empty())
    {
        return desktop_entry_.name;
    }
    return filename_;
}

std::string_view
vfs::desktop::exec() const noexcept
{
    return desktop_entry_.exec;
}

bool
vfs::desktop::use_terminal() const noexcept
{
    return desktop_entry_.terminal;
}

const std::filesystem::path&
vfs::desktop::path() const noexcept
{
    return path_;
}

std::string_view
vfs::desktop::icon_name() const noexcept
{
    return desktop_entry_.icon;
}

Glib::RefPtr<Gtk::IconPaintable>
vfs::desktop::icon(i32 size) const noexcept
{
    Glib::RefPtr<Gtk::IconPaintable> desktop_icon = nullptr;

    if (!desktop_entry_.icon.empty())
    {
        desktop_icon = vfs::utils::load_icon(desktop_entry_.icon, size, "application-x-executable");
    }
    return desktop_icon;
}

std::vector<std::string>
vfs::desktop::supported_mime_types() const noexcept
{
    return ztd::split(desktop_entry_.mime_type, ";");
}

bool
vfs::desktop::is_opening_multiple_files() const noexcept
{
    return desktop_entry_.exec.contains("%F") || desktop_entry_.exec.contains("%U");
}

void
vfs::desktop::expand_list(std::vector<std::string>& commands,
                          std::span<const std::shared_ptr<vfs::file>> files) const noexcept
{
    for (auto& command : commands)
    {
        {
            const auto pos = command.find("%F");
            if (pos != std::string::npos)
            {
                std::string file_list;

                for (const auto& file : files)
                {
                    if (!file_list.empty())
                    {
                        file_list += ' ';
                    }

                    file_list += vfs::execute::quote(file->path());
                }

                command.replace(pos, 2, file_list);
            }
        }

        {
            const auto pos = command.find("%U");
            if (pos != std::string::npos)
            {
                std::string url_list;

                for (const auto& file : files)
                {
                    if (!url_list.empty())
                    {
                        url_list += ' ';
                    }

                    url_list += vfs::execute::quote(file->uri());
                }

                command.replace(pos, 2, url_list);
            }
        }
    }
}

void
vfs::desktop::expand_single(std::vector<std::string>& commands,
                            std::span<const std::shared_ptr<vfs::file>> files) const noexcept
{
    std::vector<std::string> expanded;
    expanded.reserve(commands.size() * files.size());

    for (const auto& command : commands)
    {
        for (const auto& file : files)
        {
            auto result = command;

            {
                const auto pos = result.find("%f");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2, vfs::execute::quote(file->path()));
                }
            }

            {
                const auto pos = result.find("%u");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2, vfs::execute::quote(file->uri()));
                }
            }

            expanded.push_back(result);
        }
    }

    commands = expanded;
}

std::optional<std::vector<std::string>>
vfs::desktop::expand_exec(std::span<const std::shared_ptr<vfs::file>> files) const noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
    // Code   Description
    // ------------------
    // %f     single file
    // %F     list of files
    // %u     single URL
    // %U     list of URLs
    // %d     Deprecated
    // %D     Deprecated
    // %n     Deprecated
    // %N     Deprecated
    // %i     icon
    // %c     translated name
    // %k     desktop file location
    // %v     Deprecated
    // %m     Deprecated

    std::vector<std::string> commands{desktop_entry_.exec};

    bool add_files = false;
    bool multiple_files = false;
    bool single_file = false;

    for (auto& command : commands)
    {
        std::string result;
        bool skip_next = false;

        for (const auto pos : std::views::iota(0uz, command.size()))
        {
            if (skip_next)
            {
                skip_next = false;
                continue;
            }

            if (command[pos] != '%')
            {
                result += command[pos];
                continue;
            }

            if (pos + 1 == command.size())
            {
                result += '%';
                break;
            }

            skip_next = true;

            const auto key = command[pos + 1];
            switch (key)
            {
                case 'F':
                case 'U':
                {
                    add_files = true;
                    multiple_files = true;

                    result += '%';
                    result += key;
                    break;
                }
                case 'f':
                case 'u':
                {
                    add_files = true;
                    single_file = true;

                    result += '%';
                    result += key;
                    break;
                }
                case 'c':
                {
                    result += display_name();
                    break;
                }
                case 'k':
                {
                    result += path_;
                    break;
                }
                case 'i':
                {
                    const auto icon = icon_name();

                    if (!icon.empty())
                    {
                        result += "--icon ";
                        result += vfs::execute::quote(icon);
                    }

                    break;
                }
                case 'd':
                case 'D':
                case 'n':
                case 'N':
                case 'v':
                case 'm':
                {
                    logger::warn<logger::vfs>("Deprecated desktop Exec key '%{}' in: {}",
                                              key,
                                              path_);

                    result += '%';
                    result += key;
                    break;
                }
                case '%':
                {
                    result += '%';
                    break;
                }
                default:
                {
                    result += '%';
                    result += key;
                    break;
                }
            }
        }

        command = result;
    }

    logger::warn_if<logger::vfs>(
        !add_files && !files.empty(),
        "Malformed desktop file, trying to open a desktop file without file/url "
        "keys with a file list: {}",
        path_);

    logger::warn_if<logger::vfs>(multiple_files && single_file,
                                 "Malformed desktop file, Exec contains both single-file and "
                                 "multiple-file keys: {}",
                                 path_);

    if (multiple_files)
    {
        expand_list(commands, files);
    }
    else if (single_file)
    {
        expand_single(commands, files);
    }

    return commands;
}

bool
vfs::desktop::open_file(const std::filesystem::path& working_dir,
                        const std::shared_ptr<vfs::file>& file) const
{
    if (desktop_entry_.exec.empty())
    {
        logger::error<logger::vfs>("Desktop Exec is empty, command not found: {}", filename_);
        return false;
    }

    std::vector<std::shared_ptr<vfs::file>> files = {file};

    exec_desktop(working_dir, files);

    return true;
}

bool
vfs::desktop::open_files(const std::filesystem::path& working_dir,
                         std::span<const std::shared_ptr<vfs::file>> files) const
{
    if (desktop_entry_.exec.empty())
    {
        logger::error<logger::vfs>("Desktop Exec is empty, command not found: {}", filename_);
        return false;
    }

    if (is_opening_multiple_files())
    {
        exec_desktop(working_dir, files);
    }
    else
    {
        // app does not accept multiple files, so run multiple times
        for (const auto& file : files)
        {
            std::vector<std::shared_ptr<vfs::file>> files = {file};

            exec_desktop(working_dir, files);
        }
    }

    return true;
}

void
vfs::desktop::exec_desktop(const std::filesystem::path& working_dir,
                           std::span<const std::shared_ptr<vfs::file>> files) const noexcept
{
    const auto commands = expand_exec(files);
    if (!commands)
    {
        return;
    }

    const auto cwd = !desktop_entry_.path.empty() ? desktop_entry_.path : working_dir.string();

    for (const auto& command : *commands)
    {
        std::vector<std::string> args = Glib::shell_parse_argv(command);

        if (use_terminal())
        {
            // TODO, prepend terminal exec args
            logger::warn<logger::vfs>("desktop terminal exec is not implemented");
            continue;
        }

        try
        {
            Glib::spawn_async(cwd,
                              args,
                              Glib::SpawnFlags::SEARCH_PATH | Glib::SpawnFlags::STDOUT_TO_DEV_NULL |
                                  Glib::SpawnFlags::STDERR_TO_DEV_NULL);
        }
        catch (const Glib::Error& ex)
        {
            logger::error<logger::vfs>("failed to spawn process for desktop file {}: {}",
                                       path_,
                                       ex.what());
        }
    }
}
