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

#include <glibmm.h>
#include <glibmm/keyfile.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

// sfm breaks vfs independence for exec_in_terminal
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-app-desktop.hxx"

std::map<std::string, vfs::desktop> desktops_map;

vfs::desktop
vfs_get_desktop(const std::string_view desktop_file)
{
    if (desktops_map.contains(desktop_file.data()))
    {
        // ztd::logger::info("cached vfs_get_desktop={}", desktop_file);
        return desktops_map.at(desktop_file.data());
    }
    // ztd::logger::info("new vfs_get_desktop={}", desktop_file);

    vfs::desktop desktop = std::make_shared<VFSAppDesktop>(desktop_file);

    desktops_map.insert({desktop_file.data(), desktop});

    return desktop;
}

VFSAppDesktop::VFSAppDesktop(const std::filesystem::path& desktop_file) noexcept
{
    bool load;
    const auto kf = Glib::KeyFile::create();

    if (desktop_file.is_absolute())
    {
        this->file_name_ = desktop_file.filename();
        this->full_path_ = desktop_file;
        load = kf->load_from_file(desktop_file, Glib::KeyFile::Flags::NONE);
    }
    else
    {
        this->file_name_ = desktop_file.filename();
        const auto relative_path = std::filesystem::path() / "applications" / this->file_name_;
        std::string relative_full_path;
        load =
            kf->load_from_data_dirs(relative_path, relative_full_path, Glib::KeyFile::Flags::NONE);
        this->full_path_ = relative_full_path;
    }

    if (!load)
    {
        ztd::logger::warn("Failed to load desktop file {}", desktop_file.string());
        this->exec_ = this->file_name_;
        return;
    }

    static const std::string desktop_entry = "Desktop Entry";

    // Keys not loaded from .desktop files
    // - Hidden
    // - OnlyShowIn
    // - NotShowIn
    // - DBusActivatable
    // - URL
    // - PrefersNonDefaultGPU
    // - SingleMainWindow

    try
    {
        const Glib::ustring g_type = kf->get_string(desktop_entry, "Type");
        if (!g_type.empty())
        {
            this->type_ = g_type;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_name = kf->get_string(desktop_entry, "Name");
        if (!g_name.empty())
        {
            this->name_ = g_name;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_generic_name = kf->get_string(desktop_entry, "GenericName");
        if (!g_generic_name.empty())
        {
            this->generic_name_ = g_generic_name;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        this->no_display_ = kf->get_boolean(desktop_entry, "NoDisplay");
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_comment = kf->get_string(desktop_entry, "Comment");
        if (!g_comment.empty())
        {
            this->comment_ = g_comment;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_icon = kf->get_string(desktop_entry, "Icon");
        if (!g_icon.empty())
        {
            this->icon_ = g_icon;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_try_exec = kf->get_string(desktop_entry, "TryExec");
        if (!g_try_exec.empty())
        {
            this->try_exec_ = g_try_exec;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_exec = kf->get_string(desktop_entry, "Exec");
        if (!g_exec.empty())
        {
            this->exec_ = g_exec;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_path = kf->get_string(desktop_entry, "Path");
        if (!g_path.empty())
        {
            this->path_ = g_path;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        this->terminal_ = kf->get_boolean(desktop_entry, "Terminal");
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_actions = kf->get_string(desktop_entry, "Actions");
        if (!g_actions.empty())
        {
            this->actions_ = g_actions;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_mime_type = kf->get_string(desktop_entry, "MimeType");
        if (!g_mime_type.empty())
        {
            this->mime_type_ = g_mime_type; // TODO vector
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_categories = kf->get_string(desktop_entry, "Categories");
        if (!g_categories.empty())
        {
            this->categories_ = g_categories;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_keywords = kf->get_string(desktop_entry, "Keywords");
        if (!g_keywords.empty())
        {
            this->keywords_ = g_keywords;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        this->startup_notify_ = kf->get_boolean(desktop_entry, "StartupNotify");
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }

    try
    {
        const Glib::ustring g_startup_wm_class = kf->get_string(desktop_entry, "StartupWMClass");
        if (!g_startup_wm_class.empty())
        {
            this->startup_wm_class_ = g_startup_wm_class;
        }
    }
    catch (const Glib::KeyFileError& e)
    { /* Desktop Missing Key, Use Default init */
    }
}

const std::string_view
VFSAppDesktop::name() const noexcept
{
    return this->file_name_;
}

const std::string_view
VFSAppDesktop::display_name() const noexcept
{
    if (!this->name_.empty())
    {
        return this->name_;
    }
    return this->file_name_;
}

const std::string_view
VFSAppDesktop::exec() const noexcept
{
    return this->exec_;
}

bool
VFSAppDesktop::use_terminal() const noexcept
{
    return this->terminal_;
}

const std::filesystem::path&
VFSAppDesktop::full_path() const noexcept
{
    return this->full_path_;
}

const std::string_view
VFSAppDesktop::icon_name() const noexcept
{
    return this->icon_;
}

GdkPixbuf*
VFSAppDesktop::icon(i32 size) const noexcept
{
    GdkPixbuf* desktop_icon = nullptr;

    if (!this->icon_.empty())
    {
        desktop_icon = vfs_load_icon(this->icon_, size);
    }

    // fallback to generic icon
    if (!desktop_icon)
    {
        desktop_icon = vfs_load_icon("application-x-executable", size);
        // fallback to generic icon
        if (!desktop_icon)
        {
            desktop_icon = vfs_load_icon("gnome-mime-application-x-executable", size);
        }
    }
    return desktop_icon;
}

bool
VFSAppDesktop::open_multiple_files() const noexcept
{
    if (this->exec_.empty())
    {
        return false;
    }

    static constexpr std::array<const std::string_view, 2> keys{"%U", "%F"};

    return ztd::contains(this->exec_, keys);
}

const std::optional<std::vector<std::vector<std::string>>>
VFSAppDesktop::app_exec_generate_desktop_argv(
    const std::span<const std::filesystem::path> file_list, bool quote_file_list) const noexcept
{
    // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables

    std::vector<std::vector<std::string>> commands = {{ztd::split(this->exec_, " ")}};

    bool add_files = false;

    static constexpr std::array<const std::string_view, 2> open_files_keys{"%F", "%U"};
    if (ztd::contains(this->exec_, open_files_keys))
    {
        // %F and %U must always be at the end
        if (!ztd::endswith(this->exec_, open_files_keys))
        {
            ztd::logger::error("Malformed desktop file, %F and %U must always be at the end: {}",
                               this->full_path_.string());
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
    if (ztd::contains(this->exec_, open_file_keys))
    {
        // %f and %u must always be at the end
        if (!ztd::endswith(this->exec_, open_file_keys))
        {
            ztd::logger::error("Malformed desktop file, %f and %u must always be at the end: {}",
                               this->full_path_.string());
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
                           this->full_path_.string());
    }

    if (ztd::contains(this->exec_, "%c"))
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

    if (ztd::contains(this->exec_, "%k"))
    {
        for (auto& argv : commands)
        {
            for (const auto [index, arg] : ztd::enumerate(argv))
            {
                if (ztd::same(arg, "%k"))
                {
                    argv[index] = this->full_path();
                    break;
                }
            }
        }
    }

    if (ztd::contains(this->exec_, "%i"))
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
VFSAppDesktop::open_files(const std::filesystem::path& working_dir,
                          const std::span<const std::filesystem::path> file_paths) const
{
    if (this->exec_.empty())
    {
        const std::string msg = std::format("Command not found\n\n{}", this->file_name_);
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
            this->exec_in_terminal(!this->path_.empty() ? this->path_ : working_dir.string(),
                                   command);
        }
    }
    else
    {
        for (const auto& argv : desktop_commands)
        {
            Glib::spawn_async_with_pipes(!this->path_.empty() ? this->path_ : working_dir.string(),
                                         argv,
                                         Glib::SpawnFlags::SEARCH_PATH |
                                             Glib::SpawnFlags::STDOUT_TO_DEV_NULL |
                                             Glib::SpawnFlags::STDERR_TO_DEV_NULL,
                                         Glib::SlotSpawnChildSetup(),
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         nullptr);
        }
    }
}
