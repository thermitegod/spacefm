/**
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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "utils/permissions.hxx"

#include "xset/xset.hxx"

#include "gui/archiver.hxx"
#include "gui/file-browser.hxx"

#include "gui/action/open.hxx"

#include "gui/dialog/app-chooser.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/app-desktop.hxx"
#include "vfs/execute.hxx"
#include "vfs/file.hxx"

#include "logger.hxx"

struct ParentInfo
{
    gui::browser* browser{nullptr};
    std::filesystem::path cwd;
};

static bool
open_archives(const std::shared_ptr<ParentInfo>& parent,
              const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    const auto is_archive = [](const auto& file) { return file->mime_type()->is_archive(); };
    if (!std::ranges::all_of(selected_files, is_archive))
    {
        return false;
    }

    if (xset_get_b(xset::name::archive_default_open_with_app))
    { // user has open archives with app option enabled, do not handle these files
        return false;
    }

    const bool extract_here = xset_get_b(xset::name::archive_default_extract);

    // determine default archive action in this dir
    if (extract_here &&
        (::utils::has_read_permission(parent->cwd) && ::utils::has_write_permission(parent->cwd)))
    {
        // Extract Here
        gui::archiver::extract(parent->browser, selected_files, parent->cwd);
        return true;
    }
    else if (extract_here || xset_get_b(xset::name::archive_default_extract_to))
    {
        // Extract Here but no write access or Extract To option
        gui::archiver::extract(parent->browser, selected_files, "");
        return true;
    }
    else if (xset_get_b(xset::name::archive_default_open_with_archiver))
    {
        gui::archiver::open(parent->browser, selected_files);
        return true;
    }

    // do not handle these files
    return false;
}

static bool
open_files_with_app(const std::shared_ptr<ParentInfo>& parent,
                    const std::span<const std::filesystem::path> open_files,
                    const std::string_view app_desktop) noexcept
{
    if (app_desktop.empty())
    {
        return false;
    }

    const auto desktop = vfs::desktop::create(app_desktop);
    if (!desktop)
    {
        return false;
    }

    logger::info<logger::domain::gui>("EXEC({})={}", desktop->path().string(), desktop->exec());

    const auto opened = desktop->open_files(parent->cwd, open_files);
    if (!opened)
    {
        std::string file_list;
        for (const auto& file : open_files)
        {
            file_list.append(file.string());
            file_list.append("\n");
        }
        gui::dialog::error(
            "Error",
            std::format("Unable to use '{}' to open files:\n{}", app_desktop, file_list));
    }

    return true;
}

void
gui::action::open_files_with_app(const std::filesystem::path& cwd,
                                 const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                 const std::string_view app_desktop, gui::browser* browser,
                                 const bool xforce, const bool xnever) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    const auto parent = std::make_shared<ParentInfo>(browser, cwd);

    if (!app_desktop.empty())
    {
        std::vector<std::filesystem::path> files_to_open;
        files_to_open.reserve(selected_files.size());
        for (const auto& file : selected_files)
        {
            files_to_open.push_back(file->path());
        }

        open_files_with_app(parent, files_to_open, app_desktop);
        return;
    }

    // No app specified - Use default app for each file

    std::vector<std::filesystem::path> dirs_to_open;
    std::unordered_map<std::string, std::vector<std::filesystem::path>> files_to_open;
    for (const auto& file : selected_files)
    {
        // Is a dir?  Open in browser
        if (file->is_directory())
        {
            dirs_to_open.push_back(file->path());
            continue;
        }

        // If this file is an executable file, run it.
        if (!xnever && file->mime_type()->is_executable() &&
            (browser->settings_->click_executes || xforce))
        {
            vfs::execute::command_line_async(file->path().string());
            if (browser)
            {
                browser->signal_open_file().emit(browser,
                                                 file->path(),
                                                 gui::browser::open_action::file);
            }
            continue;
        }

        // Find app to open this file
        std::optional<std::string> alloc_desktop = std::nullopt;

        auto mime_type = file->mime_type();

        // archive has special handling
        if (!selected_files.empty() && open_archives(parent, selected_files))
        { // all files were handled by open_archives
            break;
        }

        // The file itself is a desktop entry file.
        if (!alloc_desktop)
        {
            if (file->is_desktop_entry() && (browser->settings_->click_executes || xforce))
            {
                alloc_desktop = file->path();
            }
            else
            {
                alloc_desktop = mime_type->default_action();
            }
        }

        if (!alloc_desktop && mime_type->is_text())
        {
            // FIXME: special handling for plain text file
            mime_type = vfs::mime_type::create_from_type(vfs::constants::mime_type::plain_text);
            alloc_desktop = mime_type->default_action();
        }

        if (!alloc_desktop && file->is_symlink())
        {
            // broken link?
            try
            {
                const auto target_path = std::filesystem::read_symlink(file->path());

                if (!std::filesystem::exists(target_path))
                {
                    gui::dialog::error(
                        "Broken Link",
                        std::format("This symlink's target is missing or you do not "
                                    "have permission to access it:\n{}\n\nTarget: {}",
                                    file->path().string(),
                                    target_path.string()));
                    continue;
                }
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                logger::warn<logger::domain::gui>("{}", e.what());
                continue;
            }
        }
        if (!alloc_desktop)
        {
            const auto gui_app = gui::dialog::app_chooser(mime_type, true, true, true, !browser);

            if (gui_app)
            {
                alloc_desktop = gui_app;
            }
        }
        if (!alloc_desktop)
        {
            continue;
        }

        const auto desktop = alloc_desktop.value();
        if (files_to_open.contains(desktop))
        {
            files_to_open[desktop].push_back(file->path());
        }
        else
        {
            files_to_open[desktop] = {file->path()};
        }
    }

    for (const auto& [desktop, open_files] : files_to_open)
    {
        open_files_with_app(parent, open_files, desktop);
    }

    if (browser && !dirs_to_open.empty())
    {
        if (dirs_to_open.size() == 1)
        {
            browser->signal_open_file().emit(browser,
                                             dirs_to_open.front(),
                                             gui::browser::open_action::dir);
        }
        else
        {
            for (const auto& dir : dirs_to_open)
            {
                browser->signal_open_file().emit(browser, dir, gui::browser::open_action::new_tab);
            }
        }
    }
}
