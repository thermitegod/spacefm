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

#include <optional>
#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <vector>

#include <algorithm>
#include <ranges>

#include <memory>

#include <cassert>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "utils.hxx"

#include "settings/disk-format.hxx"
#include "settings/plugins-load.hxx"

#include "settings.hxx"
#include "main-window.hxx"

#include "ptk/ptk-error.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-handler.hxx"

#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-plugins.hxx"

void
clean_plugin_mirrors()
{ // remove plugin mirrors for non-existent plugins
    bool redo = true;

    while (redo)
    {
        redo = false;
        for (xset_t set : xsets)
        {
            assert(set != nullptr);

            if (set->desc && ztd::same(set->desc.value(), "@plugin@mirror@"))
            {
                if (!set->shared_key || !xset_is(set->shared_key.value()))
                {
                    xset_remove(set);
                    redo = true;
                    break;
                }
            }
        }
    }

    // remove plugin-data for non-existent xsets
    const auto path = vfs::user_dirs->program_config_dir() / "plugin-data";
    if (std::filesystem::is_directory(path))
    {
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const auto file_name = file.path().filename();
            if (ztd::startswith(file_name.string(), "cstm_") && !xset_is(file_name.string()))
            {
                const auto plugin_path = path / file_name;
                std::filesystem::remove_all(plugin_path);
                ztd::logger::info("Removed {}/{}", path, file_name);
            }
        }
    }
}

static void
xset_set_plugin_mirror(xset_t pset)
{
    assert(pset != nullptr);

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->desc && ztd::same(set->desc.value(), "@plugin@mirror@"))
        {
            if (set->parent && set->child)
            {
                if (ztd::same(set->child.value(), pset->plugin->name) &&
                    ztd::same(set->parent.value(), pset->plugin->path.string()))
                {
                    set->shared_key = pset->name;
                    pset->shared_key = set->name;
                    return;
                }
            }
        }
    }
}

xset_t
xset_get_plugin_mirror(xset_t set)
{
    assert(set != nullptr);

    // plugin mirrors are custom xsets that save the user's key, icon
    // and run prefs for the plugin, if any
    if (!set->plugin)
    {
        return set;
    }
    if (set->shared_key)
    {
        return xset_get(set->shared_key.value());
    }

    xset_t newset = xset_custom_new();
    newset->desc = "@plugin@mirror@";
    newset->parent = set->plugin->path;
    newset->child = set->plugin->name;
    newset->shared_key = set->name; // this will not be saved
    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->context = set->context;
    newset->opener = set->opener;
    newset->b = set->b;
    newset->s = set->s;
    set->shared_key = newset->name;
    return newset;
}

static i32
compare_plugin_sets(xset_t a, xset_t b)
{
    assert(a != nullptr);
    assert(b != nullptr);

    return g_utf8_collate(a->menu_label.value().data(), b->menu_label.value().data());
}

const std::vector<xset_t>
xset_get_plugins()
{ // return list of plugin sets sorted by menu_label
    std::vector<xset_t> plugins;

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->plugin && set->plugin->is_top && !set->plugin->path.empty())
        {
            plugins.emplace_back(set);
        }
    }
    std::ranges::sort(plugins, compare_plugin_sets);
    return plugins;
}

void
xset_clear_plugins(const std::span<const xset_t> plugins)
{
    std::ranges::for_each(plugins, xset_remove);
}

static xset_t
xset_get_by_plug_name(const std::filesystem::path& plug_dir, const std::string_view plug_name)
{
    if (plug_name.empty())
    {
        return nullptr;
    }

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->plugin && ztd::same(plug_name, set->plugin->name) &&
            std::filesystem::equivalent(plug_dir, set->plugin->path))
        {
            return set;
        }
    }

    // add new
    const std::string setname = xset_custom_new_name();

    xset_t set = xset_new(setname, xset::name::custom);
    set->plugin = std::make_unique<xset::XSetPlugin>(plug_name, plug_dir);
    set->lock = false;
    xsets.emplace_back(set);

    return set;
}

static void
xset_parse_plugin(const std::filesystem::path& plug_dir, const std::string_view name,
                  const std::string_view setvar, const std::string_view value, plugin::use use)
{
    if (value.empty())
    {
        return;
    }

    // handler
    std::string prefix;
    switch (use)
    {
        case plugin::use::handler_archive:
            prefix = "handler_archive_";
            break;
        case plugin::use::handler_filesystem:
            prefix = "handler_filesystem_";
            break;
        case plugin::use::handler_network:
            prefix = "handler_network_";
            break;
        case plugin::use::handler_file:
            prefix = "handler_file_";
            break;
        case plugin::use::bookmarks:
        case plugin::use::normal:
            prefix = "cstm_";
            break;
    }

    if (!ztd::startswith(name, prefix))
    {
        return;
    }

    xset::var var;
    try
    {
        var = xset::get_xsetvar_from_name(setvar);
    }
    catch (const std::logic_error& e)
    {
        const std::string msg =
            std::format("Plugin load error:\n\"{}\"\n{}", plug_dir.string(), e.what());
        ztd::logger::error("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return;
    }

    xset_t set;
    xset_t set2;

    set = xset_get_by_plug_name(plug_dir, name);
    xset_set_var(set, var, value.data());

    if (use >= plugin::use::bookmarks)
    {
        // map plug names to new set names (does not apply to handlers)
        if (set->prev && var == xset::var::prev)
        {
            if (ztd::startswith(set->prev.value(), "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->prev.value());
                set->prev = set2->name;
            }
            else
            {
                set->prev = std::nullopt;
            }
        }
        else if (set->next && var == xset::var::next)
        {
            if (ztd::startswith(set->next.value(), "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->next.value());
                set->next = set2->name;
            }
            else
            {
                set->next = std::nullopt;
            }
        }
        else if (set->parent && var == xset::var::parent)
        {
            if (ztd::startswith(set->parent.value(), "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->parent.value());
                set->parent = set2->name;
            }
            else
            {
                set->parent = std::nullopt;
            }
        }
        else if (set->child && var == xset::var::child)
        {
            if (ztd::startswith(set->child.value(), "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->child.value());
                set->child = set2->name;
            }
            else
            {
                set->child = std::nullopt;
            }
        }
    }
}

static void
xset_import_plugin_parse(const std::filesystem::path& plug_dir, plugin::use* use,
                         const std::string_view name, const std::string_view var,
                         const std::string_view value)
{
    // ztd::logger::info("name: {} | var: {} | value: {}", name, var, value);

    if (use && *use == plugin::use::normal)
    {
        if (ztd::startswith(name, "handler_"))
        {
            if (ztd::startswith(name, "handler_filesystem_"))
            {
                *use = plugin::use::handler_filesystem;
            }
            else if (ztd::startswith(name, "handler_archive_"))
            {
                *use = plugin::use::handler_archive;
            }
            else if (ztd::startswith(name, "handler_network_"))
            {
                *use = plugin::use::handler_network;
            }
            else if (ztd::startswith(name, "handler_file_"))
            {
                *use = plugin::use::handler_file;
            }
        }
    }
    xset_parse_plugin(plug_dir, name, var, value, use ? *use : plugin::use::normal);
}

xset_t
xset_import_plugin(const std::filesystem::path& plug_dir, plugin::use* use)
{
    if (use)
    {
        *use = plugin::use::normal;
    }

    // clear all existing plugin sets with this plug_dir
    // ( keep the mirrors to retain user prefs )
    bool redo = true;
    while (redo)
    {
        redo = false;
        for (xset_t set : xsets)
        {
            assert(set != nullptr);

            if (set->plugin && std::filesystem::equivalent(plug_dir, set->plugin->path))
            {
                xset_remove(set);
                redo = true; // search list from start again due to changed list
                break;
            }
        }
    }

    // read plugin file into xsets
    const std::filesystem::path plugin = plug_dir / PLUGIN_FILE_FILENAME;

    if (!std::filesystem::exists(plugin))
    {
        return nullptr;
    }

    const bool plugin_good = load_user_plugin(plug_dir, use, plugin, &xset_import_plugin_parse);

    // clean plugin sets, set type
    bool top = true;
    xset_t rset = nullptr;
    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->plugin && std::filesystem::equivalent(plug_dir, set->plugin->path))
        {
            set->key = 0;
            set->keymod = 0;
            set->tool = xset::tool::NOT;
            set->opener = 0;
            xset_set_plugin_mirror(set);
            set->plugin->is_top = top;
            if (set->plugin->is_top)
            {
                top = false;
                rset = set;
            }
        }
    }
    return plugin_good ? rset : nullptr;
}

void
on_install_plugin_cb(vfs::file_task task, PluginData* plugin_data)
{
    (void)task;
    xset_t set;
    // ztd::logger::info("on_install_plugin_cb");
    if (plugin_data->job == plugin::job::remove) // uninstall
    {
        if (!std::filesystem::exists(plugin_data->plug_dir))
        {
            xset_custom_delete(plugin_data->set, false);
            clean_plugin_mirrors();
        }
    }
    else
    {
        const auto plugin = plugin_data->plug_dir / PLUGIN_FILE_FILENAME;
        if (std::filesystem::exists(plugin))
        {
            plugin::use use = plugin::use::normal;
            set = xset_import_plugin(plugin_data->plug_dir, &use);
            if (!set)
            {
                const std::string msg = std::format(
                    "The imported plugin directory does not contain a valid plugin.\n\n({}/)",
                    plugin_data->plug_dir.string());
                xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Invalid Plugin",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                msg);
            }
            else if (use != plugin::use::bookmarks)
            {
                // handler
                set->plugin->is_top = false; // prevent being added to Plugins menu
                if (plugin_data->job == plugin::job::install)
                {
                    // This dialog should never be seen - failsafe
                    xset_msg_dialog(plugin_data->main_window ? GTK_WIDGET(plugin_data->main_window)
                                                             : nullptr,
                                    GtkMessageType::GTK_MESSAGE_ERROR,
                                    "Handler Plugin",
                                    GtkButtonsType::GTK_BUTTONS_OK,
                                    "This file contains a handler plugin which cannot be installed "
                                    "as a plugin.\n\nYou can import handlers from a handler "
                                    "configuration window, or use Plugins|Import.");
                }
                else
                {
                    // TODO
                    ptk_handler_import(magic_enum::enum_integer(use),
                                       plugin_data->handler_dlg,
                                       set);
                }
            }
            else if (plugin_data->job == plugin::job::copy)
            {
                // copy
                set->plugin->is_top = false; // do not show tmp plugin in Plugins menu
                if (plugin_data->set)
                {
                    // paste after insert_set (plugin_data->set)
                    xset_t newset = xset_custom_copy(set, false, true);
                    newset->prev = plugin_data->set->name;
                    newset->next = plugin_data->set->next;
                    if (plugin_data->set->next)
                    {
                        xset_t set_next = xset_get(plugin_data->set->next.value());
                        set_next->prev = newset->name;
                    }
                    plugin_data->set->next = newset->name;
                    if (plugin_data->set->tool != xset::tool::NOT)
                    {
                        newset->tool = xset::tool::custom;
                    }
                    else
                    {
                        newset->tool = xset::tool::NOT;
                    }
                }
                else
                {
                    // place on design clipboard
                    xset_set_clipboard = set;
                    xset_clipboard_is_cut = false;
                    if (xset_get_b(xset::name::plug_cverb) || plugin_data->handler_dlg)
                    {
                        std::string msg;
                        const std::string label =
                            clean_label(set->menu_label.value(), false, false);
                        if (geteuid() == 0)
                        {
                            msg = std::format(
                                "The '{}' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu.",
                                label);
                        }
                        else
                        {
                            msg = std::format(
                                "The '{}' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu, and its contents are not protected by root (once pasted "
                                "it will be saved with normal ownership).\n\nIf this plugin "
                                "contains su commands or will be run as root, installing it to "
                                "and running it only from the Plugins menu is recommended to "
                                "improve your system security.",
                                label);
                        }
                        xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                        GtkMessageType::GTK_MESSAGE_INFO,
                                        "Copy Plugin",
                                        GtkButtonsType::GTK_BUTTONS_OK,
                                        msg);
                    }
                }
            }
            clean_plugin_mirrors();
        }
    }

    delete plugin_data;
}

void
install_plugin_file(void* main_win, GtkWidget* handler_dlg, const std::filesystem::path& path,
                    const std::filesystem::path& plug_dir, plugin::job job, xset_t insert_set)
{
    std::string own;
    const std::string plug_dir_q = ztd::shell::quote(plug_dir.string());
    const std::string file_path_q = ztd::shell::quote(path.string());

    MainWindow* main_window = MAIN_WINDOW(main_win);
    // task
    PtkFileTask* ptask = ptk_file_exec_new("Install Plugin",
                                           main_win ? GTK_WIDGET(main_window) : nullptr,
                                           main_win ? main_window->task_view : nullptr);

    switch (job)
    {
        case plugin::job::install:
            // install
            own =
                std::format("chown -R root:root {} && chmod -R go+rX-w {}", plug_dir_q, plug_dir_q);
            ptask->task->exec_as_user = "root";
            break;
        case plugin::job::copy:
            // copy to clipboard or import to menu
            own = std::format("chmod -R go+rX-w {}", plug_dir_q);
            break;
        case plugin::job::remove:
            break;
    }

    std::string book;
    if (job == plugin::job::install || !insert_set)
    {
        // prevent install of exported bookmarks or handler as plugin or design clipboard
        if (job == plugin::job::install)
        {
            book = " || [ -e main_book ] || [ -d hand_* ]";
        }
        else
        {
            book = " || [ -e main_book ]";
        }
    }

    ptask->task->exec_command = std::format(
        "rm -rf {} ; mkdir -p {} && cd {} && tar --exclude='/*' --keep-old-files -xf {} ; "
        "err=$status ; if [ $err -ne 0 ] || [ ! -e plugin ] {} ; then rm -rf {} ; echo 'Error "
        "installing "
        "plugin (invalid plugin file?)'; exit 1 ; fi ; {}",
        plug_dir_q,
        plug_dir_q,
        plug_dir_q,
        file_path_q,
        book,
        plug_dir_q,
        own);

    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;

    const auto plugin_data = new PluginData;
    plugin_data->main_window = main_window;
    plugin_data->handler_dlg = handler_dlg;
    plugin_data->plug_dir = plug_dir;
    plugin_data->job = job;
    plugin_data->set = insert_set;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}
