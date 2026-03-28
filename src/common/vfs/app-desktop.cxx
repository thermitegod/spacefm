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
#include <flat_map>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#if (GTK_MAJOR_VERSION == 3) // TODO
#include "gui/file-task.hxx" // break vfs independence for exec_in_terminal
#endif

#include "vfs/app-desktop.hxx"
#include "vfs/error.hxx"
#include "vfs/execute.hxx"
#include "vfs/file.hxx"

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

struct desktop_cache_data final
{
    vfs::desktop desktop;
    std::chrono::system_clock::time_point mtime;
};

static std::flat_map<std::filesystem::path, desktop_cache_data> desktops_cache;

std::expected<vfs::desktop, std::error_code>
vfs::desktop::create(const std::filesystem::path& desktop_file) noexcept
{
    if (desktops_cache.contains(desktop_file))
    {
        // logger::info<logger::vfs>("vfs::desktop({})  cache   {}", logger::utils::ptr(desktop), desktop_file.string());
        const auto& cache = desktops_cache.at(desktop_file);

        const auto desktop_stat = ztd::stat::create(cache.desktop.path());
        if (desktop_stat && desktop_stat->mtime() == cache.mtime)
        {
            return cache.desktop;
        }
        // logger::info<logger::vfs>("vfs::desktop({}) changed on disk, reloading", logger::utils::ptr(desktop));
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
    // logger::info<logger::vfs>("vfs::desktop({})  new     {}", logger::utils::ptr(desktop), desktop_file.string());
    return desktop;
}

vfs::desktop::desktop(const std::filesystem::path& desktop_file) noexcept
    : filename_(desktop_file.filename()), path_(desktop_file)
{
    // logger::info<logger::vfs>("vfs::desktop::desktop({})", logger::utils::ptr(this));
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

#if (GTK_MAJOR_VERSION == 4)
    const auto kf = Glib::KeyFile::create();
#elif (GTK_MAJOR_VERSION == 3)
    Glib::KeyFile kf;
#endif

    if (path_.is_absolute())
    {
#if (GTK_MAJOR_VERSION == 4)
        loaded = kf->load_from_file(path_, Glib::KeyFile::Flags::NONE);
#elif (GTK_MAJOR_VERSION == 3)
        loaded = kf.load_from_file(path_, Glib::KEY_FILE_NONE);
#endif
    }
    else
    {
        const auto relative_path = std::filesystem::path() / "applications" / filename_;
        std::string relative_full_path;
        try
        {
#if (GTK_MAJOR_VERSION == 4)
            loaded = kf->load_from_data_dirs(relative_path, relative_full_path);
#elif (GTK_MAJOR_VERSION == 3)
            loaded = kf.load_from_data_dirs(relative_path, relative_full_path);
#endif
        }
        catch (...) // Glib::KeyFileError, Glib::FileError
        {
            logger::error<logger::vfs>("Error opening desktop file: {}", path_.string());
            return vfs::error_code::file_open_failure;
        }
        path_ = relative_full_path;
    }

    if (!loaded)
    {
        logger::error<logger::vfs>("Failed to load desktop file: {}", path_.string());
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

#if (GTK_MAJOR_VERSION == 4)

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

#elif (GTK_MAJOR_VERSION == 3)

    // clang-format off

    // Required Keys, must fail if missing

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE))
    {
        desktop_entry_.type = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME))
    {
        desktop_entry_.name = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    // Optional Keys

    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME))
    {
        desktop_entry_.generic_name = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY))
    {
        desktop_entry_.no_display = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT))
    {
        desktop_entry_.comment = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON))
    {
        desktop_entry_.icon = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC))
    {
        desktop_entry_.try_exec = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC))
    {
        desktop_entry_.exec = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH))
    {
        desktop_entry_.path = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL))
    {
         desktop_entry_.terminal = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS))
    {
        desktop_entry_.actions = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE))
    {
        desktop_entry_.mime_type = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES))
    {
        desktop_entry_.categories = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS))
    {
        desktop_entry_.keywords = kf.get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS);
    }
    if (kf.has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY))
    {
        desktop_entry_.startup_notify = kf.get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY);
    }
    // clang-format on

#endif

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

#if (GTK_MAJOR_VERSION == 4)

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

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf*
vfs::desktop::icon(i32 size) const noexcept
{
    GdkPixbuf* desktop_icon = nullptr;

    if (!desktop_entry_.icon.empty())
    {
        desktop_icon = vfs::utils::load_icon(desktop_entry_.icon, size);
    }

    // fallback to generic icon
    if (!desktop_icon)
    {
        desktop_icon = vfs::utils::load_icon("application-x-executable", size);
    }
    return desktop_icon;
}

#endif

std::vector<std::string>
vfs::desktop::supported_mime_types() const noexcept
{
    return ztd::split(desktop_entry_.mime_type, ";");
}

bool
vfs::desktop::open_multiple_files() const noexcept
{
    return desktop_entry_.exec.contains("%F") || desktop_entry_.exec.contains("%U");
}

std::optional<std::vector<std::vector<std::string>>>
vfs::desktop::app_exec_generate_desktop_argv(
    const std::span<const std::shared_ptr<vfs::file>> files, bool quote_file_list) const noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::vector<std::vector<std::string>> commands = {{ztd::split(desktop_entry_.exec, " ")}};

    bool add_files = false;

    if (desktop_entry_.exec.contains("%F") || desktop_entry_.exec.contains("%U"))
    {
        // %F and %U must always be at the end
        // if (!desktop_entry_.exec.ends_with("%F") ||
        //     !desktop_entry_.exec.ends_with("%U"))
        // {
        //     logger::error<logger::vfs>("Malformed desktop file, %F and %U must always be at the end: {}", path_.string());
        //     return std::nullopt;
        // }

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_files_key
            for (const auto& file : files)
            {
                if (quote_file_list)
                {
                    argv.push_back(vfs::execute::quote(file->path().string()));
                }
                else
                {
                    argv.push_back(file->path().string());
                }
            }
        }

        add_files = true;
    }

    if (desktop_entry_.exec.contains("%f") || desktop_entry_.exec.contains("%e"))
    {
        // %f and %u must always be at the end
        // if (!desktop_entry_.exec.ends_with("%f") ||
        //     !desktop_entry_.exec.ends_with("%u"))
        // {
        //     logger::error<logger::vfs>("Malformed desktop file, %f and %u must always be at the end: {}", path_.string());
        //     return std::nullopt;
        // }

        // desktop files with these keys can only open one file.
        // spawn multiple copies of the program for each selected file
        commands.insert(commands.cbegin(), files.size() - 1, commands.front());

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_file_key
            for (const auto& file : files)
            {
                if (quote_file_list)
                {
                    argv.push_back(vfs::execute::quote(file->path().string()));
                }
                else
                {
                    argv.push_back(file->path());
                }
            }
        }
        add_files = true;
    }

    logger::warn_if<logger::vfs>(
        !add_files && !files.empty(),
        "Malformed desktop file, trying to open a desktop file without file/url "
        "keys with a file list: {}",
        path_.string());

    if (desktop_entry_.exec.contains("%c"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg != "%c")
                {
                    argv[static_cast<std::size_t>(index)] = display_name();
                    break;
                }
            }
        }
    }

    if (desktop_entry_.exec.contains("%k"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg == "%k")
                {
                    argv[static_cast<std::size_t>(index)] = path_;
                    break;
                }
            }
        }
    }

    if (desktop_entry_.exec.contains("%i"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : std::views::enumerate(argv))
            {
                if (arg == "%i")
                {
                    argv[static_cast<std::size_t>(index)] = std::format("--icon {}", icon_name());
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
#if (GTK_MAJOR_VERSION == 4)
    (void)cwd;
    (void)command;
    ztd::panic("Not Implemented");
#elif (GTK_MAJOR_VERSION == 3)
    // task
    gui::file_task* ptask = gui_file_exec_new(display_name(), cwd, nullptr, nullptr);

    ptask->task->exec_command = command;

    ptask->task->exec_terminal = true;
    ptask->task->exec_sync = false;

    ptask->run();
#endif
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

    exec_desktop(working_dir, {file});

    return true;
}

bool
vfs::desktop::open_files(const std::filesystem::path& working_dir,
                         const std::span<const std::shared_ptr<vfs::file>> files) const
{
    if (desktop_entry_.exec.empty())
    {
        logger::error<logger::vfs>("Desktop Exec is empty, command not found: {}", filename_);
        return false;
    }

    if (open_multiple_files())
    {
        exec_desktop(working_dir, files);
    }
    else
    {
        // app does not accept multiple files, so run multiple times
        for (const auto& file : files)
        {
            exec_desktop(working_dir, {file});
        }
    }

    return true;
}

void
vfs::desktop::exec_desktop(const std::filesystem::path& working_dir,
                           const std::span<const std::shared_ptr<vfs::file>> files) const noexcept
{
    const auto desktop_commands = app_exec_generate_desktop_argv(files, use_terminal());
    if (!desktop_commands)
    {
        return;
    }

    if (use_terminal())
    {
        for (const auto& argv : desktop_commands.value())
        {
            const std::string command = ztd::join(argv, " ");
            exec_in_terminal(!desktop_entry_.path.empty() ? desktop_entry_.path
                                                          : working_dir.string(),
                             command);
        }
    }
    else
    {
        for (const auto& argv : desktop_commands.value())
        {
            Glib::spawn_async_with_pipes(
                !desktop_entry_.path.empty() ? desktop_entry_.path : working_dir.string(),
                argv,
#if (GTK_MAJOR_VERSION == 4)
                Glib::SpawnFlags::SEARCH_PATH | Glib::SpawnFlags::STDOUT_TO_DEV_NULL |
                    Glib::SpawnFlags::STDERR_TO_DEV_NULL,
#elif (GTK_MAJOR_VERSION == 3)
                Glib::SpawnFlags::SPAWN_SEARCH_PATH | Glib::SpawnFlags::SPAWN_STDOUT_TO_DEV_NULL |
                    Glib::SpawnFlags::SPAWN_STDERR_TO_DEV_NULL,
#endif
                Glib::SlotSpawnChildSetup(),
                nullptr,
                nullptr,
                nullptr,
                nullptr);
        }
    }
}
