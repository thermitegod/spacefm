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

#include <magic_enum.hpp>
#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <utility>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "ptk/ptk-error.hxx"

#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-file-archiver.hxx"
#include "ptk/ptk-location-view.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "ptk/ptk-handler.hxx"

#include "type-conversion.hxx"

#include "settings/app.hxx"

#include "utils.hxx"

#include "ptk/ptk-file-actions-open.hxx"

using namespace std::literals::string_view_literals;

#define PARENT_INFO(obj) (static_cast<ParentInfo*>(obj))

struct ParentInfo
{
    ParentInfo(PtkFileBrowser* file_browser, const std::filesystem::path& cwd);

    PtkFileBrowser* file_browser{nullptr};
    std::filesystem::path cwd{};
};

ParentInfo::ParentInfo(PtkFileBrowser* file_browser, const std::filesystem::path& cwd)
{
    this->file_browser = file_browser;
    this->cwd = cwd;
}

static bool
open_archives_with_handler(ParentInfo* parent, const std::span<const vfs::file_info> sel_files,
                           const std::filesystem::path& full_path, vfs::mime_type mime_type)
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
                              sel_files,
                              parent->cwd,
                              dest_dir,
                              magic_enum::enum_integer(cmd),
                              true);
    return true; // all files handled
}

static void
open_files_with_handler(ParentInfo* parent, GList* files, xset_t handler_set)
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
        xset_msg_dialog(parent->file_browser ? GTK_WIDGET(parent->file_browser) : nullptr,
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
        for (GList* l = files; l; l = g_list_next(l))
        {
            // filename
            const auto name = std::filesystem::path((char*)l->data).filename();
            fm_filenames.append(std::format("{}\n", ztd::shell::quote(name.string())));
            // file path
            fm_filenames.append(std::format("{}\n", ztd::shell::quote((char*)l->data)));
        }
    }
    fm_filenames.append(")\nfm_filename=\"$fm_filenames[0]\"\n");
    fm_files.append(")\nfm_file=\"$fm_files[0]\"\n");
    // replace standard sub vars
    command = replace_line_subs(command);

    // start task(s)
    for (GList* l = files; l; l = g_list_next(l))
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

            const auto name = std::filesystem::path((char*)l->data).filename();
            quoted = ztd::shell::quote(name.string());
            str = std::format("fm_filename={}\n", quoted);
            // file path
            quoted = ztd::shell::quote((char*)l->data);
            command_final =
                std::format("{}{}{}fm_file={}\n{}", fm_filenames, fm_files, str, quoted, command);
        }

        // Run task
        PtkFileTask* ptask =
            ptk_file_exec_new(handler_set->menu_label.value(),
                              parent->cwd,
                              parent->file_browser ? GTK_WIDGET(parent->file_browser) : nullptr,
                              parent->file_browser ? parent->file_browser->task_view : nullptr);
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

static const std::string
check_desktop_name(const std::string_view app_desktop)
{
    // Check whether this is an app desktop file or just a command line
    if (ztd::endswith(app_desktop, ".desktop"))
    {
        return app_desktop.data();
    }

    // Not a desktop entry name
    // If we are lucky enough, there might be a desktop entry
    // for this program
    const std::string name = std::format("{}.desktop", app_desktop);
    if (std::filesystem::exists(name))
    {
        return name;
    }

    // fallback
    return app_desktop.data();
}

static bool
open_files_with_app(ParentInfo* parent, GList* files, const std::string_view app_desktop)
{
    xset_t handler_set;

    if (ztd::startswith(app_desktop, "###") && (handler_set = xset_is(app_desktop.substr(3))) &&
        files)
    {
        // is a handler
        open_files_with_handler(parent, files, handler_set);
        return true;
    }
    if (app_desktop.empty())
    {
        return false;
    }

    const vfs::desktop desktop = vfs_get_desktop(check_desktop_name(app_desktop));

    ztd::logger::info("EXEC({})={}", desktop->get_full_path().string(), desktop->get_exec());

    const std::vector<std::filesystem::path> open_files = glist_t_char_to_vector_t_path(files);

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

static void
open_files_with_each_app(void* key, void* value, void* user_data)
{
    char* app_desktop = (char*)key; // is const unless handler
    GList* files = (GList*)value;
    ParentInfo* parent = PARENT_INFO(user_data);
    open_files_with_app(parent, files, app_desktop);
}

static void
free_file_list_hash(void* key, void* value, void* user_data)
{
    (void)key;
    (void)user_data;

    GList* files = (GList*)value;
    g_list_foreach(files, (GFunc)std::free, nullptr);
    g_list_free(files);
}

void
ptk_open_files_with_app(const std::filesystem::path& cwd,
                        const std::span<const vfs::file_info> sel_files,
                        const std::string_view app_desktop, PtkFileBrowser* file_browser,
                        bool xforce, bool xnever)
{
    // if xnever, never execute an executable
    // if xforce, force execute of executable ignoring app_settings.click_executes

    std::filesystem::path full_path;
    GList* files_to_open = nullptr;
    GHashTable* file_list_hash = nullptr;
    char* new_dir = nullptr;
    GtkWidget* toplevel;

    const auto parent = new ParentInfo(file_browser, cwd);

    for (const vfs::file_info file : sel_files)
    {
        if (!file)
        {
            continue;
        }

        full_path = cwd / file->name();

        if (!app_desktop.empty())
        { // specified app to open all files
            files_to_open = g_list_append(files_to_open, ztd::strdup(full_path));
        }
        else
        {
            // No app specified - Use default app for each file

            // Is a dir?  Open in browser
            if (file_browser && std::filesystem::is_directory(full_path))
            {
                if (!new_dir)
                {
                    new_dir = ztd::strdup(full_path);
                }
                else
                {
                    if (file_browser)
                    {
                        file_browser->run_event<spacefm::signal::open_item>(
                            full_path,
                            ptk::open_action::new_tab);
                    }
                }
                continue;
            }

            /* If this file is an executable file, run it. */
            if (!xnever && file->is_executable(full_path) &&
                (app_settings.get_click_executes() || xforce))
            {
                Glib::spawn_command_line_async(full_path);
                if (file_browser)
                {
                    file_browser->run_event<spacefm::signal::open_item>(full_path,
                                                                        ptk::open_action::file);
                }
                continue;
            }

            /* Find app to open this file and place copy in alloc_desktop.
             * This string is freed when hash table is destroyed. */
            std::string alloc_desktop;

            vfs::mime_type mime_type = file->mime_type();

            // has archive handler?
            if (!sel_files.empty() &&
                open_archives_with_handler(parent, sel_files, full_path, mime_type))
            {
                // all files were handled by open_archives_with_handler
                break;
            }

            // if has file handler, set alloc_desktop = ###XSETNAME
            const std::vector<xset_t> handlers =
                ptk_handler_file_has_handlers(ptk::handler::mode::file,
                                              ptk::handler::mount::mount,
                                              full_path,
                                              mime_type,
                                              true,
                                              false,
                                              true);
            if (!handlers.empty())
            {
                const xset_t handler_set = handlers.front();
                alloc_desktop = std::format("###{}", handler_set->name);
            }

            /* The file itself is a desktop entry file. */
            /* was: if(ztd::endswith(file->name(), ".desktop"))
             */
            if (alloc_desktop.empty())
            {
                if (file->flags() & vfs::file_info_flags::desktop_entry &&
                    (app_settings.get_click_executes() || xforce))
                {
                    alloc_desktop = full_path;
                }
                else
                {
                    alloc_desktop = mime_type->default_action();
                }
            }

            if (alloc_desktop.empty() && mime_type_is_text_file(full_path, mime_type->type()))
            {
                /* FIXME: special handling for plain text file */
                mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_PLAIN_TEXT);
                alloc_desktop = mime_type->default_action();
            }

            if (alloc_desktop.empty() && file->is_symlink())
            {
                // broken link?
                try
                {
                    const auto target_path = std::filesystem::read_symlink(full_path);

                    if (!std::filesystem::exists(target_path))
                    {
                        const std::string msg = std::format("This symlink's target is missing or "
                                                            "you do not have permission "
                                                            "to access it:\n{}\n\nTarget: {}",
                                                            full_path.string(),
                                                            target_path.string());
                        toplevel = file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser))
                                                : nullptr;
                        ptk_show_error(GTK_WINDOW(toplevel), "Broken Link", msg.data());
                        continue;
                    }
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    ztd::logger::warn("{}", e.what());
                }
            }
            if (alloc_desktop.empty())
            {
                /* Let the user choose an application */
                toplevel =
                    file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser)) : nullptr;
                alloc_desktop = ztd::null_check(ptk_choose_app_for_mime_type(GTK_WINDOW(toplevel),
                                                                             mime_type,
                                                                             true,
                                                                             true,
                                                                             true,
                                                                             !file_browser));
            }
            if (alloc_desktop.empty())
            {
                continue;
            }

            // add full_path to list, update hash table
            files_to_open = nullptr;
            if (!file_list_hash)
            { /* this will free the keys (alloc_desktop) when hash table
               * destroyed or new key inserted/replaced */
                file_list_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, nullptr);
            }
            else
            { // get existing file list for this app
                files_to_open = (GList*)g_hash_table_lookup(file_list_hash, alloc_desktop.data());
            }

            if (!ztd::same(alloc_desktop, full_path.string()))
            { /* it is not a desktop file itself - add file to list.
               * Otherwise use full_path as hash table key, which will
               * be freed when hash table is destroyed. */
                files_to_open = g_list_append(files_to_open, ztd::strdup(full_path));
            }
            // update file list in hash table
            g_hash_table_replace(file_list_hash, ztd::strdup(alloc_desktop), files_to_open);
        }
    }

    if (!app_desktop.empty() && files_to_open)
    {
        // specified app to open all files
        open_files_with_app(parent, files_to_open, app_desktop);
        g_list_foreach(files_to_open, (GFunc)std::free, nullptr);
        g_list_free(files_to_open);
    }
    else if (file_list_hash)
    {
        // No app specified - Use default app to open each associated list of files
        // free_file_list_hash frees each file list and its strings
        g_hash_table_foreach(file_list_hash, open_files_with_each_app, parent);
        g_hash_table_foreach(file_list_hash, free_file_list_hash, nullptr);
        g_hash_table_destroy(file_list_hash);
    }

    if (new_dir)
    {
        if (file_browser)
        {
            file_browser->run_event<spacefm::signal::open_item>(full_path, ptk::open_action::dir);
        }
        std::free(new_dir);
    }

    delete parent;
}
