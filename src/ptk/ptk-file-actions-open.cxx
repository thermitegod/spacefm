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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <vector>

#include <memory>

#include <optional>

#include <magic_enum.hpp>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "ptk/ptk-dialog.hxx"

#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-file-archiver.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "ptk/ptk-handler.hxx"

#include "settings/app.hxx"

#include "utils.hxx"

#include "ptk/ptk-file-actions-open.hxx"

struct ParentInfo
{
    PtkFileBrowser* file_browser{nullptr};
    std::filesystem::path cwd{};
};

static bool
open_archives_with_handler(const std::shared_ptr<ParentInfo>& parent,
                           const std::span<const vfs::file_info> selected_files,
                           const std::filesystem::path& full_path, const vfs::mime_type& mime_type)
{
    if (xset_get_b(xset::name::arc_def_open))
    {                 // user has open archives with app option enabled
        return false; // do not handle these files
    }

    const bool extract_here = xset_get_b(xset::name::arc_def_ex);
    std::filesystem::path dest_dir;
    ptk::handler::archive cmd;

    // determine default archive action in this dir
    if (extract_here && have_rw_access(parent->cwd))
    {
        // Extract Here
        cmd = ptk::handler::archive::extract;
        dest_dir = parent->cwd;
    }
    else if (extract_here || xset_get_b(xset::name::arc_def_exto))
    {
        // Extract Here but no write access or Extract To option
        cmd = ptk::handler::archive::extract;
    }
    else if (xset_get_b(xset::name::arc_def_list))
    {
        // List contents
        cmd = ptk::handler::archive::list;
    }
    else
    {
        return false; // do not handle these files
    }

    // type or pathname has archive handler? - do not test command non-empty
    // here because only applies to first file
    const std::vector<xset_t> handlers =
        ptk_handler_file_has_handlers(ptk::handler::mode::arc,
                                      magic_enum::enum_integer(cmd),
                                      full_path,
                                      mime_type,
                                      false,
                                      false,
                                      true);
    if (handlers.empty())
    {
        return false; // do not handle these files
    }

    ptk_file_archiver_extract(parent->file_browser,
                              selected_files,
                              parent->cwd,
                              dest_dir,
                              magic_enum::enum_integer(cmd),
                              true);
    return true; // all files handled
}

static void
open_files_with_handler(const std::shared_ptr<ParentInfo>& parent,
                        const std::span<const std::filesystem::path> open_files, xset_t handler_set)
{
    std::string str;
    std::string command_final;

    ztd::logger::info("Selected File Handler '{}'", handler_set->menu_label.value());

    // get command - was already checked as non-empty
    std::string error_message;
    std::string command;
    const bool error = ptk_handler_load_script(ptk::handler::mode::file,
                                               ptk::handler::mount::mount,
                                               handler_set,
                                               nullptr,
                                               command,
                                               error_message);
    if (error)
    {
        ptk_show_message(GTK_WINDOW(parent->file_browser),
                         GtkMessageType::GTK_MESSAGE_ERROR,
                         "Error Loading Handler",
                         GtkButtonsType::GTK_BUTTONS_OK,
                         error_message);
        return;
    }

    /* prepare fish vars for just the files being opened by this handler,
     * not necessarily all selected */
    std::string fm_filenames = "fm_filenames=(\n";
    std::string fm_files = "fm_files=(\n";
    // command looks like it handles multiple files ?
    static constexpr std::array<const std::string_view, 4> keys{"%N",
                                                                "%F",
                                                                "fm_files[",
                                                                "fm_filenames["};
    const bool multiple = ztd::contains(command, keys);
    if (multiple)
    {
        for (const auto& file : open_files)
        {
            // filename
            const auto name = file.filename();
            fm_filenames.append(std::format("{}\n", ztd::shell::quote(name.string())));
            // file path
            fm_filenames.append(std::format("{}\n", ztd::shell::quote(file.string())));
        }
    }
    fm_filenames.append(")\nfm_filename=\"$fm_filenames[0]\"\n");
    fm_files.append(")\nfm_file=\"$fm_files[0]\"\n");
    // replace standard sub vars
    command = replace_line_subs(command);

    // start task(s)

    for (const auto& file : open_files)
    {
        if (multiple)
        {
            command_final = std::format("{}{}{}", fm_filenames, fm_files, command);
        }
        else
        {
            // add sub vars for single file
            // filename
            std::string quoted;

            const auto name = file.filename();
            quoted = ztd::shell::quote(name.string());
            str = std::format("fm_filename={}\n", quoted);
            // file path
            quoted = ztd::shell::quote(file.string());
            command_final =
                std::format("{}{}{}fm_file={}\n{}", fm_filenames, fm_files, str, quoted, command);
        }

        // Run task
        PtkFileTask* ptask =
            ptk_file_exec_new(handler_set->menu_label.value(),
                              parent->cwd,
                              parent->file_browser ? GTK_WIDGET(parent->file_browser) : nullptr,
                              parent->file_browser ? parent->file_browser->task_view() : nullptr);
        // do not free cwd!
        ptask->task->exec_browser = parent->file_browser;
        ptask->task->exec_command = command_final;
        if (handler_set->icon)
        {
            ptask->task->exec_icon = handler_set->icon.value();
        }
        ptask->task->exec_terminal = handler_set->in_terminal;
        ptask->task->exec_keep_terminal = false;
        // file handlers store Run As Task in keep_terminal
        ptask->task->exec_sync = handler_set->keep_terminal;
        ptask->task->exec_show_error = ptask->task->exec_sync;
        ptask->task->exec_export = true;
        ptk_file_task_run(ptask);

        if (multiple)
        {
            break;
        }
    }
}

static bool
open_files_with_app(const std::shared_ptr<ParentInfo>& parent,
                    const std::span<const std::filesystem::path> open_files,
                    const std::string_view app_desktop)
{
    if (ztd::startswith(app_desktop, "###") && !open_files.empty())
    {
        const xset_t handler_set = xset_is(ztd::removeprefix(app_desktop, "###"));
        if (handler_set == nullptr)
        {
            return false;
        }
        // is a handler
        open_files_with_handler(parent, open_files, handler_set);
        return true;
    }
    if (app_desktop.empty())
    {
        return false;
    }

    const vfs::desktop desktop = vfs_get_desktop(app_desktop);

    ztd::logger::info("EXEC({})={}", desktop->path().string(), desktop->exec());

    try
    {
        desktop->open_files(parent->cwd, open_files);
    }
    catch (const VFSAppDesktopException& e)
    {
        GtkWidget* toplevel = parent->file_browser
                                  ? gtk_widget_get_toplevel(GTK_WIDGET(parent->file_browser))
                                  : nullptr;
        ptk_show_error(GTK_WINDOW(toplevel), "Error", e.what());
    }

    return true;
}

void
ptk_open_files_with_app(const std::filesystem::path& cwd,
                        const std::span<const vfs::file_info> selected_files,
                        const std::string_view app_desktop, PtkFileBrowser* file_browser,
                        bool xforce, bool xnever)
{
    const auto parent = std::make_shared<ParentInfo>(file_browser, cwd);

    if (!app_desktop.empty())
    {
        std::vector<std::filesystem::path> files_to_open;
        files_to_open.reserve(selected_files.size());
        for (const vfs::file_info file : selected_files)
        {
            files_to_open.emplace_back(file->path());
        }

        open_files_with_app(parent, files_to_open, app_desktop);
        return;
    }

    // No app specified - Use default app for each file

    std::vector<std::filesystem::path> dirs_to_open;
    std::map<std::string, std::vector<std::filesystem::path>> files_to_open;
    for (const vfs::file_info file : selected_files)
    {
        // Is a dir?  Open in browser
        if (file->is_directory())
        {
            dirs_to_open.emplace_back(file->path());
            continue;
        }

        // If this file is an executable file, run it.
        if (!xnever && file->is_executable(file->path()) &&
            (app_settings.click_executes() || xforce))
        {
            Glib::spawn_command_line_async(file->path());
            if (file_browser)
            {
                file_browser->run_event<spacefm::signal::open_item>(file->path(),
                                                                    ptk::open_action::file);
            }
            continue;
        }

        // Find app to open this file
        std::optional<std::string> alloc_desktop = std::nullopt;

        vfs::mime_type mime_type = file->mime_type();

        // has archive handler?
        if (!selected_files.empty() &&
            open_archives_with_handler(parent, selected_files, file->path(), mime_type))
        { // all files were handled by open_archives_with_handler
            break;
        }

        // if has file handler, set alloc_desktop = ###XSETNAME
        const std::vector<xset_t> handlers =
            ptk_handler_file_has_handlers(ptk::handler::mode::file,
                                          ptk::handler::mount::mount,
                                          file->path(),
                                          mime_type,
                                          true,
                                          false,
                                          true);
        if (!handlers.empty())
        {
            const xset_t handler_set = handlers.front();
            alloc_desktop = std::format("###{}", handler_set->name);
        }

        // The file itself is a desktop entry file.
        if (!alloc_desktop)
        {
            if (file->flags() & vfs::file_info_flags::desktop_entry &&
                (app_settings.click_executes() || xforce))
            {
                alloc_desktop = file->path();
            }
            else
            {
                alloc_desktop = mime_type->default_action();
            }
        }

        if (!alloc_desktop && mime_type_is_text_file(file->path(), mime_type->type()))
        {
            // FIXME: special handling for plain text file
            mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
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
                    GtkWidget* toplevel =
                        file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser)) : nullptr;
                    ptk_show_error(GTK_WINDOW(toplevel),
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
                ztd::logger::warn("{}", e.what());
                continue;
            }
        }
        if (!alloc_desktop)
        {
            // Let the user choose an application
            GtkWidget* toplevel =
                file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser)) : nullptr;
            const auto ptk_app = ptk_choose_app_for_mime_type(GTK_WINDOW(toplevel),
                                                              mime_type,
                                                              true,
                                                              true,
                                                              true,
                                                              !file_browser);

            if (ptk_app)
            {
                alloc_desktop = ptk_app;
            }
        }
        if (!alloc_desktop)
        {
            continue;
        }

        const auto desktop = alloc_desktop.value();
        if (files_to_open.contains(desktop))
        {
            files_to_open[desktop].emplace_back(file->path());
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

    if (file_browser && dirs_to_open.size() != 0)
    {
        if (dirs_to_open.size() == 1)
        {
            file_browser->run_event<spacefm::signal::open_item>(dirs_to_open.front(),
                                                                ptk::open_action::dir);
        }
        else
        {
            for (const auto& dir : dirs_to_open)
            {
                file_browser->run_event<spacefm::signal::open_item>(dir, ptk::open_action::new_tab);
            }
        }
    }
}
