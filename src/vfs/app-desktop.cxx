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
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "gui/file-task.hxx" // break vfs independence for exec_in_terminal

#include "vfs/app-desktop.hxx"
#include "vfs/error.hxx"
#include "vfs/execute.hxx"

#include "vfs/utils/utils.hxx"

#include "logger.hxx"

struct desktop_cache_data final
{
    vfs::desktop desktop;
    std::chrono::system_clock::time_point mtime;
};

static std::unordered_map<std::filesystem::path, desktop_cache_data> desktops_cache;

std::expected<vfs::desktop, std::error_code>
vfs::desktop::create(const std::filesystem::path& desktop_file) noexcept
{
    if (desktops_cache.contains(desktop_file))
    {
        // logger::info<logger::domain::vfs>("vfs::desktop({})  cache   {}", logger::utils::ptr(desktop), desktop_file.string());
        const auto& cache = desktops_cache.at(desktop_file);

        const auto desktop_stat = ztd::stat::create(cache.desktop.path());
        if (desktop_stat && desktop_stat->mtime() == cache.mtime)
        {
            return cache.desktop;
        }
        // logger::info<logger::domain::vfs>("vfs::desktop({}) changed on disk, reloading", logger::utils::ptr(desktop));
    }

    auto desktop = vfs::desktop(desktop_file);
    const auto result = desktop.parse_desktop_file();
    if (result != vfs::error_code::none)
    {
        return std::unexpected(result);
    }

    const auto stat = ztd::stat::create(desktop.path());
    if (!stat)
    {
        return std::unexpected(vfs::error_code::file_not_found);
    }

    desktops_cache.insert({desktop_file, {desktop, stat->mtime()}});
    // logger::info<logger::domain::vfs>("vfs::desktop({})  new     {}", logger::utils::ptr(desktop), desktop_file.string());
    return desktop;
}

vfs::desktop::desktop(const std::filesystem::path& desktop_file) noexcept
    : filename_(desktop_file.filename()), path_(desktop_file)
{
    // logger::info<logger::domain::vfs>("vfs::desktop::desktop({})", logger::utils::ptr(this));
}

vfs::error_code
vfs::desktop::parse_desktop_file() noexcept
{
    static constexpr std::string DESKTOP_ENTRY_GROUP = "Desktop Entry";

    static constexpr std::string DESKTOP_ENTRY_KEY_TYPE = "Type";
    static constexpr std::string DESKTOP_ENTRY_KEY_NAME = "Name";
    static constexpr std::string DESKTOP_ENTRY_KEY_GENERICNAME = "GenericName";
    static constexpr std::string DESKTOP_ENTRY_KEY_NODISPLAY = "NoDisplay";
    static constexpr std::string DESKTOP_ENTRY_KEY_COMMENT = "Comment";
    static constexpr std::string DESKTOP_ENTRY_KEY_ICON = "Icon";
    static constexpr std::string DESKTOP_ENTRY_KEY_TRYEXEC = "TryExec";
    static constexpr std::string DESKTOP_ENTRY_KEY_EXEC = "Exec";
    static constexpr std::string DESKTOP_ENTRY_KEY_PATH = "Path";
    static constexpr std::string DESKTOP_ENTRY_KEY_TERMINAL = "Terminal";
    static constexpr std::string DESKTOP_ENTRY_KEY_ACTIONS = "Actions";
    static constexpr std::string DESKTOP_ENTRY_KEY_MIMETYPE = "MimeType";
    static constexpr std::string DESKTOP_ENTRY_KEY_CATEGORIES = "Categories";
    static constexpr std::string DESKTOP_ENTRY_KEY_KEYWORDS = "Keywords";
    static constexpr std::string DESKTOP_ENTRY_KEY_STARTUPNOTIFY = "StartupNotify";

    bool loaded = false;

    Glib::KeyFile kf;

    if (this->path_.is_absolute())
    {
        loaded = kf.load_from_file(this->path_, Glib::KEY_FILE_NONE);
    }
    else
    {
        const auto relative_path = std::filesystem::path() / "applications" / this->filename_;
        std::string relative_full_path;
        try
        {
            loaded = kf.load_from_data_dirs(relative_path, relative_full_path);
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            logger::error<logger::domain::vfs>("Error opening desktop file: {}",
                                               this->path_.string());
            return vfs::error_code::file_open_failure;
        }
        this->path_ = relative_full_path;
    }

    if (!loaded)
    {
        logger::error<logger::domain::vfs>("Failed to load desktop file: {}", this->path_.string());
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

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE))
    {
        this->desktop_entry_.type = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME))
    {
        this->desktop_entry_.name = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    // Optional Keys

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME))
    {
        this->desktop_entry_.generic_name = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY))
    {
        this->desktop_entry_.no_display = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT))
    {
        this->desktop_entry_.comment = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON))
    {
        this->desktop_entry_.icon = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC))
    {
        this->desktop_entry_.try_exec = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC))
    {
        this->desktop_entry_.exec = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH))
    {
        this->desktop_entry_.path = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL))
    {
         this->desktop_entry_.terminal = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS))
    {
        this->desktop_entry_.actions = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE))
    {
        this->desktop_entry_.mime_type = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES))
    {
        this->desktop_entry_.categories = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS))
    {
        this->desktop_entry_.keywords = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY))
    {
        this->desktop_entry_.startup_notify = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY);
    }
    // clang-format on

    return loaded ? vfs::error_code::none : vfs::error_code::parse_error;
}

std::string_view
vfs::desktop::name() const noexcept
{
    return this->filename_;
}

std::string_view
vfs::desktop::display_name() const noexcept
{
    if (!this->desktop_entry_.name.empty())
    {
        return this->desktop_entry_.name;
    }
    return this->filename_;
}

std::string_view
vfs::desktop::exec() const noexcept
{
    return this->desktop_entry_.exec;
}

bool
vfs::desktop::use_terminal() const noexcept
{
    return this->desktop_entry_.terminal;
}

const std::filesystem::path&
vfs::desktop::path() const noexcept
{
    return this->path_;
}

std::string_view
vfs::desktop::icon_name() const noexcept
{
    return this->desktop_entry_.icon;
}

GdkPixbuf*
vfs::desktop::icon(i32 size) const noexcept
{
    GdkPixbuf* desktop_icon = nullptr;

    if (!this->desktop_entry_.icon.empty())
    {
        desktop_icon = vfs::utils::load_icon(this->desktop_entry_.icon, size);
    }

    // fallback to generic icon
    if (!desktop_icon)
    {
        desktop_icon = vfs::utils::load_icon("application-x-executable", size);
    }
    return desktop_icon;
}

std::vector<std::string>
vfs::desktop::supported_mime_types() const noexcept
{
    return ztd::split(this->desktop_entry_.mime_type, ";");
}

bool
vfs::desktop::open_multiple_files() const noexcept
{
    return this->desktop_entry_.exec.contains("%F") || this->desktop_entry_.exec.contains("%U");
}

std::optional<std::vector<std::vector<std::string>>>
vfs::desktop::app_exec_generate_desktop_argv(const std::span<const std::filesystem::path> file_list,
                                             bool quote_file_list) const noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::vector<std::vector<std::string>> commands = {{ztd::split(this->desktop_entry_.exec, " ")}};

    bool add_files = false;

    if (this->desktop_entry_.exec.contains("%F") || this->desktop_entry_.exec.contains("%U"))
    {
        // %F and %U must always be at the end
        // if (!this->desktop_entry_.exec.ends_with("%F") ||
        //     !this->desktop_entry_.exec.ends_with("%U"))
        // {
        //     logger::error<logger::domain::vfs>("Malformed desktop file, %F and %U must always be at the end: {}", this->path_.string());
        //     return std::nullopt;
        // }

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_files_key
            for (const auto& file : file_list)
            {
                if (quote_file_list)
                {
                    argv.push_back(vfs::execute::quote(file.string()));
                }
                else
                {
                    argv.push_back(file.string());
                }
            }
        }

        add_files = true;
    }

    if (this->desktop_entry_.exec.contains("%f") || this->desktop_entry_.exec.contains("%e"))
    {
        // %f and %u must always be at the end
        // if (!this->desktop_entry_.exec.ends_with("%f") ||
        //     !this->desktop_entry_.exec.ends_with("%u"))
        // {
        //     logger::error<logger::domain::vfs>("Malformed desktop file, %f and %u must always be at the end: {}", this->path_.string());
        //     return std::nullopt;
        // }

        // desktop files with these keys can only open one file.
        // spawn multiple copies of the program for each selected file
        commands.insert(commands.cbegin(), file_list.size() - 1, commands.front());

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_file_key
            for (const auto& file : file_list)
            {
                if (quote_file_list)
                {
                    argv.push_back(vfs::execute::quote(file.string()));
                }
                else
                {
                    argv.push_back(file);
                }
            }
        }
        add_files = true;
    }

    if (!add_files && !file_list.empty())
    {
        logger::error<logger::domain::vfs>(
            "Malformed desktop file, trying to open a desktop file without file/url "
            "keys with a file list: {}",
            this->path_.string());
    }

    if (this->desktop_entry_.exec.contains("%c"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg != "%c")
                {
                    argv[static_cast<std::size_t>(index)] = this->display_name();
                    break;
                }
            }
        }
    }

    if (this->desktop_entry_.exec.contains("%k"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg == "%k")
                {
                    argv[static_cast<std::size_t>(index)] = this->path_;
                    break;
                }
            }
        }
    }

    if (this->desktop_entry_.exec.contains("%i"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg == "%i")
                {
                    argv[static_cast<std::size_t>(index)] =
                        std::format("--icon {}", this->icon_name());
                    break;
                }
            }
        }
    }

    return commands;
}

void
vfs::desktop::exec_in_terminal(const std::filesystem::path& cwd,
                               const std::string_view command) const noexcept
{
    // task
    gui::file_task* ptask = gui_file_exec_new(this->display_name(), cwd, nullptr, nullptr);

    ptask->task->exec_command = command;

    ptask->task->exec_terminal = true;
    ptask->task->exec_sync = false;

    ptask->run();
}

bool
vfs::desktop::open_file(const std::filesystem::path& working_dir,
                        const std::filesystem::path& file_path) const
{
    if (this->desktop_entry_.exec.empty())
    {
        logger::error<logger::domain::vfs>(
            std::format("Desktop Exec is empty, command not found: {}", this->filename_));
        return false;
    }

    const std::vector<std::filesystem::path> file_paths{file_path};
    this->exec_desktop(working_dir, file_paths);

    return true;
}

bool
vfs::desktop::open_files(const std::filesystem::path& working_dir,
                         const std::span<const std::filesystem::path> file_paths) const
{
    if (this->desktop_entry_.exec.empty())
    {
        logger::error<logger::domain::vfs>(
            std::format("Desktop Exec is empty, command not found: {}", this->filename_));
        return false;
    }

    if (this->open_multiple_files())
    {
        this->exec_desktop(working_dir, file_paths);
    }
    else
    {
        // app does not accept multiple files, so run multiple times
        for (const auto& open_file : file_paths)
        {
            const std::vector<std::filesystem::path> open_files{open_file};
            this->exec_desktop(working_dir, open_files);
        }
    }

    return true;
}

void
vfs::desktop::exec_desktop(const std::filesystem::path& working_dir,
                           const std::span<const std::filesystem::path> file_paths) const noexcept
{
    const auto desktop_commands =
        this->app_exec_generate_desktop_argv(file_paths, this->use_terminal());
    if (!desktop_commands)
    {
        return;
    }

    if (this->use_terminal())
    {
        for (const auto& argv : desktop_commands.value())
        {
            const std::string command = ztd::join(argv, " ");
            this->exec_in_terminal(!this->desktop_entry_.path.empty() ? this->desktop_entry_.path
                                                                      : working_dir.string(),
                                   command);
        }
    }
    else
    {
        for (const auto& argv : desktop_commands.value())
        {
            Glib::spawn_async_with_pipes(
                !this->desktop_entry_.path.empty() ? this->desktop_entry_.path
                                                   : working_dir.string(),
                argv,
                Glib::SpawnFlags::SPAWN_SEARCH_PATH | Glib::SpawnFlags::SPAWN_STDOUT_TO_DEV_NULL |
                    Glib::SpawnFlags::SPAWN_STDERR_TO_DEV_NULL,
                Glib::SlotSpawnChildSetup(),
                nullptr,
                nullptr,
                nullptr,
                nullptr);
        }
    }
}
