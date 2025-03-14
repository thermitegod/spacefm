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

#include <filesystem>
#include <flat_map>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>

#include "settings/settings.hxx"

#include "gui/action/open.hxx"

#include "vfs/app-desktop.hxx"
#include "vfs/execute.hxx"
#include "vfs/file.hxx"

#include "logger.hxx"

static bool
open_files(Gtk::ApplicationWindow& parent, const std::filesystem::path& cwd,
           const std::span<const std::shared_ptr<vfs::file>> files,
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

    logger::info<logger::gui>("EXEC({})={}", desktop->path().string(), desktop->exec());

    const auto opened = desktop->open_files(cwd, files);
    if (!opened)
    {
        std::string file_list;
        for (const auto& file : files)
        {
            file_list.append(file->path().string());
            file_list.append("\n");
        }

        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(
            std::format("Unable to use '{}' to open files:\n{}", app_desktop, file_list));
        alert->set_modal(true);
        alert->show(parent);
    }

    return true;
}

void
gui::action::open_files_with_app(Gtk::ApplicationWindow& parent, const std::filesystem::path& cwd,
                                 const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                 const std::string_view app_desktop,
                                 const std::shared_ptr<config::settings>& settings) noexcept

{
    (void)settings;

    if (selected_files.empty())
    {
        return;
    }

    if (app_desktop.empty())
    {
        auto alert = Gtk::AlertDialog::create("Missing App");
        alert->set_detail(
            "Trying to open files using a know desktop file that is missing or empty");
        alert->set_modal(true);
        alert->show(parent);
    }

    open_files(parent, cwd, selected_files, app_desktop);
}

void
gui::action::open_files_auto(Gtk::ApplicationWindow& parent, const std::filesystem::path& cwd,
                             const std::span<const std::shared_ptr<vfs::file>> selected_files,
                             const bool xforce, const bool xnever,
                             const std::shared_ptr<config::settings>& settings) noexcept
{
    if (selected_files.empty())
    {
        return;
    }

    std::flat_map<std::string, std::vector<std::shared_ptr<vfs::file>>> files_to_open;
    for (const auto& file : selected_files)
    {
        if (file->is_directory())
        { // directories do not get handled here
            logger::warn("open_files_with_app() directory {}", file->path().string());
            continue;
        }

        // If this file is an executable file, run it.
        if (!xnever && file->mime_type()->is_executable() &&
            (settings->general.click_executes || xforce))
        {
            vfs::execute::command_line_async(file->path().string());
            continue;
        }

        // Find app to open this file
        std::optional<std::string> app_desktop = std::nullopt;

        auto mime_type = file->mime_type();

        // The file itself is a desktop entry file.
        if (!app_desktop)
        {
            if (file->is_desktop_entry() && (settings->general.click_executes || xforce))
            {
                app_desktop = file->path();
            }
            else
            {
                app_desktop = mime_type->default_action();
            }
        }

        if (!app_desktop && mime_type->is_text())
        {
            // FIXME: special handling for plain text file
            mime_type = vfs::mime_type::create_from_type(vfs::constants::mime_type::plain_text);
            app_desktop = mime_type->default_action();
        }

        if (!app_desktop && file->is_symlink())
        {
            // broken link?
            std::error_code ec;
            const auto target_path = std::filesystem::read_symlink(file->path(), ec);
            if (ec)
            {
                logger::warn<logger::gui>("{}", ec.message());
                continue;
            }

            if (!std::filesystem::exists(target_path))
            {
                auto alert = Gtk::AlertDialog::create("Broken Link");
                alert->set_detail(std::format("This symlink's target is missing or you do not "
                                              "have permission to access it:\n{}\n\nTarget: {}",
                                              file->path().string(),
                                              target_path.string()));
                alert->set_modal(true);
                alert->show(parent);

                continue;
            }
        }

        if (!app_desktop)
        {
            // since this is in a loop I do not want to open the app chooser here
            auto alert = Gtk::AlertDialog::create("Choose App");
            alert->set_detail(
                std::format("The mimetype '{}' does not have a default program set, Use the App "
                            "Chooser option to set a default program to open this file type",
                            mime_type->type()));
            alert->set_modal(true);
            alert->show(parent);

            return;
        }

        files_to_open[*app_desktop].push_back(file);
    }

    for (const auto& [desktop, files] : files_to_open)
    {
        open_files(parent, cwd, files, desktop);
    }
}

void
gui::action::open_files_execute(Gtk::ApplicationWindow& parent, const std::filesystem::path& cwd,
                                const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                const std::shared_ptr<config::settings>& settings) noexcept
{
    (void)cwd;
    (void)settings;

    for (const auto& file : selected_files)
    {
        if (file->is_executable())
        {
            vfs::execute::command_line_async(file->path().string());
        }
        else
        {
            auto alert = Gtk::AlertDialog::create("Cannot Execute");
            alert->set_detail(
                std::format("This file is not an executable: '{}'", file->path().string()));
            alert->set_modal(true);
            alert->show(parent);
        }
    }
}
