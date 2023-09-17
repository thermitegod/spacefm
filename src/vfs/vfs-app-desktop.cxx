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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <vector>

#include <map>

#include <optional>

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-app-desktop.hxx"

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

std::map<std::filesystem::path, vfs::desktop> desktops_cache;

vfs::desktop
vfs_get_desktop(const std::filesystem::path& desktop_file)
{
    if (desktops_cache.contains(desktop_file))
    {
        // ztd::logger::info("cached vfs_get_desktop={}", desktop_file.string());
        return desktops_cache.at(desktop_file);
    }
    // ztd::logger::info("new vfs_get_desktop={}", desktop_file.string());

    vfs::desktop desktop = std::make_shared<VFSAppDesktop>(desktop_file);

    desktops_cache.insert({desktop_file, desktop});

    return desktop;
}

VFSAppDesktop::VFSAppDesktop(const std::filesystem::path& desktop_file) noexcept
{
    const auto kf = Glib::KeyFile::create();

    if (desktop_file.is_absolute())
    {
        this->filename_ = desktop_file.filename();
        this->path_ = desktop_file;
        this->loaded_ = kf->load_from_file(desktop_file, Glib::KeyFile::Flags::NONE);
    }
    else
    {
        this->filename_ = desktop_file.filename();
        const auto relative_path = std::filesystem::path() / "applications" / this->filename_;
        std::string relative_full_path;
        this->loaded_ =
            kf->load_from_data_dirs(relative_path, relative_full_path, Glib::KeyFile::Flags::NONE);
        this->path_ = relative_full_path;
    }

    if (!this->loaded_)
    {
        ztd::logger::error("Failed to load desktop file: {}", desktop_file.string());
        return;
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
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE))
    {
        this->desktop_entry_.type = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TYPE);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME))
    {
        this->desktop_entry_.name = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NAME);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME))
    {
        this->desktop_entry_.generic_name = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_GENERICNAME);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY))
    {
        this->desktop_entry_.no_display = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_NODISPLAY);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT))
    {
        this->desktop_entry_.comment = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_COMMENT);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON))
    {
        this->desktop_entry_.icon = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ICON);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC))
    {
        this->desktop_entry_.try_exec = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TRYEXEC);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC))
    {
        this->desktop_entry_.exec = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_EXEC);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH))
    {
        this->desktop_entry_.path = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_PATH);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL))
    {
         this->desktop_entry_.terminal = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_TERMINAL);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS))
    {
        this->desktop_entry_.actions = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_ACTIONS);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE))
    {
        this->desktop_entry_.mime_type = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_MIMETYPE);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES))
    {
        this->desktop_entry_.categories = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_CATEGORIES);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS))
    {
        this->desktop_entry_.keywords = kf->get_string(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_KEYWORDS);
    }
    if (kf->has_key(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY))
    {
        this->desktop_entry_.startup_notify = kf->get_boolean(DESKTOP_ENTRY_GROUP, DESKTOP_ENTRY_KEY_STARTUPNOTIFY);
    }
    // clang-format on
}

const std::string_view
VFSAppDesktop::name() const noexcept
{
    return this->filename_;
}

const std::string_view
VFSAppDesktop::display_name() const noexcept
{
    if (!this->desktop_entry_.name.empty())
    {
        return this->desktop_entry_.name;
    }
    return this->filename_;
}

const std::string_view
VFSAppDesktop::exec() const noexcept
{
    return this->desktop_entry_.exec;
}

bool
VFSAppDesktop::use_terminal() const noexcept
{
    return this->desktop_entry_.terminal;
}

const std::filesystem::path&
VFSAppDesktop::path() const noexcept
{
    return this->path_;
}

const std::string_view
VFSAppDesktop::icon_name() const noexcept
{
    return this->desktop_entry_.icon;
}

GdkPixbuf*
VFSAppDesktop::icon(i32 size) const noexcept
{
    GdkPixbuf* desktop_icon = nullptr;

    if (!this->desktop_entry_.icon.empty())
    {
        desktop_icon = vfs_load_icon(this->desktop_entry_.icon, size);
    }

    // fallback to generic icon
    if (!desktop_icon)
    {
        desktop_icon = vfs_load_icon("application-x-executable", size);
    }
    return desktop_icon;
}

const std::vector<std::string>
VFSAppDesktop::supported_mime_types() const noexcept
{
    return ztd::split(this->desktop_entry_.mime_type, ";");
}

bool
VFSAppDesktop::open_multiple_files() const noexcept
{
    static constexpr std::array<const std::string_view, 2> keys{"%U", "%F"};

    return ztd::contains(this->desktop_entry_.exec, keys);
}

const std::optional<std::vector<std::vector<std::string>>>
VFSAppDesktop::app_exec_generate_desktop_argv(
    const std::span<const std::filesystem::path> file_list, bool quote_file_list) const noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::vector<std::vector<std::string>> commands = {{ztd::split(this->desktop_entry_.exec, " ")}};

    bool add_files = false;

    static constexpr std::array<const std::string_view, 2> open_files_keys{"%F", "%U"};
    if (ztd::contains(this->desktop_entry_.exec, open_files_keys))
    {
        // %F and %U must always be at the end
        if (!ztd::endswith(this->desktop_entry_.exec, open_files_keys))
        {
            ztd::logger::error("Malformed desktop file, %F and %U must always be at the end: {}",
                               this->path_.string());
            return std::nullopt;
        }

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_files_key
            for (const auto& file : file_list)
            {
                if (quote_file_list)
                {
                    argv.emplace_back(ztd::shell::quote(file.string()));
                }
                else
                {
                    argv.emplace_back(file.string());
                }
            }
        }

        add_files = true;
    }

    static constexpr std::array<const std::string_view, 2> open_file_keys{"%f", "%u"};
    if (ztd::contains(this->desktop_entry_.exec, open_file_keys))
    {
        // %f and %u must always be at the end
        if (!ztd::endswith(this->desktop_entry_.exec, open_file_keys))
        {
            ztd::logger::error("Malformed desktop file, %f and %u must always be at the end: {}",
                               this->path_.string());
            return std::nullopt;
        }

        // desktop files with these keys can only open one file.
        // spawn multiple copies of the program for each selected file
        commands.insert(commands.begin(), file_list.size() - 1, commands.front());

        for (auto& argv : commands)
        {
            argv.pop_back(); // remove open_file_key
            for (const auto& file : file_list)
            {
                if (quote_file_list)
                {
                    argv.emplace_back(ztd::shell::quote(file.string()));
                }
                else
                {
                    argv.emplace_back(file);
                }
            }
        }
        add_files = true;
    }

    if (!add_files && !file_list.empty())
    {
        ztd::logger::error("Malformed desktop file, trying to open a desktop file without file/url "
                           "keys with a file list: {}",
                           this->path_.string());
    }

    if (ztd::contains(this->desktop_entry_.exec, "%c"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : ztd::enumerate(argv))
            {
                if (!ztd::same(arg, "%c"))
                {
                    argv[index] = this->display_name();
                    break;
                }
            }
        }
    }

    if (ztd::contains(this->desktop_entry_.exec, "%k"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : ztd::enumerate(argv))
            {
                if (ztd::same(arg, "%k"))
                {
                    argv[index] = this->path_;
                    break;
                }
            }
        }
    }

    if (ztd::contains(this->desktop_entry_.exec, "%i"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : ztd::enumerate(argv))
            {
                if (ztd::same(arg, "%i"))
                {
                    argv[index] = std::format("--icon {}", this->icon_name());
                    break;
                }
            }
        }
    }

    return commands;
}

void
VFSAppDesktop::exec_in_terminal(const std::filesystem::path& cwd,
                                const std::string_view command) const noexcept
{
    // task
    PtkFileTask* ptask = ptk_file_exec_new(this->display_name(), cwd, nullptr, nullptr);

    ptask->task->exec_command = command;

    ptask->task->exec_terminal = true;
    // ptask->task->exec_keep_terminal = true;  // for test only
    ptask->task->exec_sync = false;
    ptask->task->exec_export = false;

    ptk_file_task_run(ptask);
}

void
VFSAppDesktop::open_file(const std::filesystem::path& working_dir,
                         const std::filesystem::path& file_path) const
{
    if (this->desktop_entry_.exec.empty())
    {
        const std::string msg =
            std::format("Desktop Exec is empty, command not found\n\n{}", this->filename_);
        throw VFSAppDesktopException(msg);
    }

    const std::vector<std::filesystem::path> file_paths{file_path};
    this->exec_desktop(working_dir, file_paths);
}

void
VFSAppDesktop::open_files(const std::filesystem::path& working_dir,
                          const std::span<const std::filesystem::path> file_paths) const
{
    if (this->desktop_entry_.exec.empty())
    {
        const std::string msg =
            std::format("Desktop Exec is empty, command not found\n\n{}", this->filename_);
        throw VFSAppDesktopException(msg);
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
}

void
VFSAppDesktop::exec_desktop(const std::filesystem::path& working_dir,
                            const std::span<const std::filesystem::path> file_paths) const noexcept
{
    const auto check_desktop_commands =
        this->app_exec_generate_desktop_argv(file_paths, this->use_terminal());
    if (!check_desktop_commands)
    {
        return;
    }
    const auto& desktop_commands = check_desktop_commands.value();

    if (this->use_terminal())
    {
        for (const auto& argv : desktop_commands)
        {
            const std::string command = ztd::join(argv, " ");
            this->exec_in_terminal(!this->desktop_entry_.path.empty() ? this->desktop_entry_.path
                                                                      : working_dir.string(),
                                   command);
        }
    }
    else
    {
        for (const auto& argv : desktop_commands)
        {
            Glib::spawn_async_with_pipes(
                !this->desktop_entry_.path.empty() ? this->desktop_entry_.path
                                                   : working_dir.string(),
                argv,
                Glib::SpawnFlags::SEARCH_PATH | Glib::SpawnFlags::STDOUT_TO_DEV_NULL |
                    Glib::SpawnFlags::STDERR_TO_DEV_NULL,
                Glib::SlotSpawnChildSetup(),
                nullptr,
                nullptr,
                nullptr,
                nullptr);
        }
    }
}
