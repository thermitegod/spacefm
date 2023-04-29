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

#include <span>

#include <vector>

#include <algorithm>
#include <ranges>

#include <cassert>

#include <fmt/format.h>

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

PluginData::PluginData()
{
    this->main_window = nullptr;
    this->handler_dlg = nullptr;
    this->plug_dir = nullptr;
    this->set = nullptr;
}

PluginData::~PluginData()
{
    if (this->plug_dir)
    {
        std::free(this->plug_dir);
    }
}

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

            if (set->desc && ztd::same(set->desc, "@plugin@mirror@"))
            {
                if (!set->shared_key || !xset_is(set->shared_key))
                {
                    xset_remove(set);
                    redo = true;
                    break;
                }
            }
        }
    }

    // remove plugin-data for non-existent xsets
    const std::string path =
        Glib::build_filename(vfs::user_dirs->program_config_dir(), "plugin-data");
    if (std::filesystem::is_directory(path))
    {
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const std::string file_name = std::filesystem::path(file).filename();
            if (ztd::startswith(file_name, "cstm_") && !xset_is(file_name))
            {
                const std::string plugin_path = fmt::format("{}/{}", path, file_name);
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

        if (set->desc && ztd::same(set->desc, "@plugin@mirror@"))
        {
            if (set->parent && set->child)
            {
                if (ztd::same(set->child, pset->plug_name) &&
                    ztd::same(set->parent, pset->plug_dir))
                {
                    if (set->shared_key)
                    {
                        std::free(set->shared_key);
                    }
                    set->shared_key = ztd::strdup(pset->name);
                    if (pset->shared_key)
                    {
                        std::free(pset->shared_key);
                    }
                    pset->shared_key = ztd::strdup(set->name);
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
        return xset_get(set->shared_key);
    }

    xset_t newset = xset_custom_new();
    newset->desc = ztd::strdup("@plugin@mirror@");
    newset->parent = ztd::strdup(set->plug_dir);
    newset->child = ztd::strdup(set->plug_name);
    newset->shared_key = ztd::strdup(set->name); // this will not be saved
    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->context = ztd::strdup(set->context);
    newset->opener = set->opener;
    newset->b = set->b;
    newset->s = ztd::strdup(set->s);
    set->shared_key = ztd::strdup(newset->name);
    return newset;
}

static i32
compare_plugin_sets(xset_t a, xset_t b)
{
    assert(a != nullptr);
    assert(b != nullptr);

    return g_utf8_collate(a->menu_label, b->menu_label);
}

const std::vector<xset_t>
xset_get_plugins()
{ // return list of plugin sets sorted by menu_label
    std::vector<xset_t> plugins;

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->plugin && set->plugin_top && set->plug_dir)
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
xset_get_by_plug_name(std::string_view plug_dir, std::string_view plug_name)
{
    if (plug_name.empty())
    {
        return nullptr;
    }

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->plugin && ztd::same(plug_name, set->plug_name) &&
            ztd::same(plug_dir, set->plug_dir))
        {
            return set;
        }
    }

    // add new
    const std::string setname = xset_custom_new_name();

    xset_t set = xset_new(setname, XSetName::CUSTOM);
    set->plug_dir = ztd::strdup(plug_dir.data());
    set->plug_name = ztd::strdup(plug_name.data());
    set->plugin = true;
    set->lock = false;
    xsets.emplace_back(set);

    return set;
}

static void
xset_parse_plugin(std::string_view plug_dir, std::string_view name, std::string_view setvar,
                  std::string_view value, PluginUse use)
{
    if (value.empty())
    {
        return;
    }

    // handler
    std::string prefix;
    switch (use)
    {
        case PluginUse::HAND_ARC:
            prefix = "hand_arc_";
            break;
        case PluginUse::HAND_FS:
            prefix = "hand_fs_";
            break;
        case PluginUse::HAND_NET:
            prefix = "hand_net_";
            break;
        case PluginUse::HAND_FILE:
            prefix = "hand_f_";
            break;
        case PluginUse::BOOKMARKS:
        case PluginUse::NORMAL:
        default:
            prefix = "cstm_";
            break;
    }

    if (!ztd::startswith(name, prefix))
    {
        return;
    }

    XSetVar var;
    try
    {
        var = xset_get_xsetvar_from_name(setvar);
    }
    catch (const std::logic_error& e)
    {
        const std::string msg = fmt::format("Plugin load error:\n\"{}\"\n{}", plug_dir, e.what());
        ztd::logger::error("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return;
    }

    xset_t set;
    xset_t set2;

    set = xset_get_by_plug_name(plug_dir, name);
    xset_set_var(set, var, value.data());

    if (use >= PluginUse::BOOKMARKS)
    {
        // map plug names to new set names (does not apply to handlers)
        if (set->prev && var == XSetVar::PREV)
        {
            if (ztd::startswith(set->prev, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->prev);
                std::free(set->prev);
                set->prev = ztd::strdup(set2->name);
            }
            else
            {
                std::free(set->prev);
                set->prev = nullptr;
            }
        }
        else if (set->next && var == XSetVar::NEXT)
        {
            if (ztd::startswith(set->next, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->next);
                std::free(set->next);
                set->next = ztd::strdup(set2->name);
            }
            else
            {
                std::free(set->next);
                set->next = nullptr;
            }
        }
        else if (set->parent && var == XSetVar::PARENT)
        {
            if (ztd::startswith(set->parent, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->parent);
                std::free(set->parent);
                set->parent = ztd::strdup(set2->name);
            }
            else
            {
                std::free(set->parent);
                set->parent = nullptr;
            }
        }
        else if (set->child && var == XSetVar::CHILD)
        {
            if (ztd::startswith(set->child, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->child);
                std::free(set->child);
                set->child = ztd::strdup(set2->name);
            }
            else
            {
                std::free(set->child);
                set->child = nullptr;
            }
        }
    }
}

static void
xset_import_plugin_parse(std::string_view plug_dir, PluginUse* use, std::string_view name,
                         std::string_view var, std::string_view value)
{
    // ztd::logger::info("name: {} | var: {} | value: {}", name, var, value);

    if (use && *use == PluginUse::NORMAL)
    {
        if (ztd::startswith(name, "hand_"))
        {
            if (ztd::startswith(name, "hand_fs_"))
            {
                *use = PluginUse::HAND_FS;
            }
            else if (ztd::startswith(name, "hand_arc_"))
            {
                *use = PluginUse::HAND_ARC;
            }
            else if (ztd::startswith(name, "hand_net_"))
            {
                *use = PluginUse::HAND_NET;
            }
            else if (ztd::startswith(name, "hand_f_"))
            {
                *use = PluginUse::HAND_FILE;
            }
        }
    }
    xset_parse_plugin(plug_dir, name, var, value, use ? *use : PluginUse::NORMAL);
}

xset_t
xset_import_plugin(const char* plug_dir, PluginUse* use)
{
    if (use)
    {
        *use = PluginUse::NORMAL;
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

            if (set->plugin && ztd::same(plug_dir, set->plug_dir))
            {
                xset_remove(set);
                redo = true; // search list from start again due to changed list
                break;
            }
        }
    }

    // read plugin file into xsets
    const std::string plugin = Glib::build_filename(plug_dir, PLUGIN_FILE_FILENAME);

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

        if (set->plugin && ztd::same(plug_dir, set->plug_dir))
        {
            set->key = 0;
            set->keymod = 0;
            set->tool = XSetTool::NOT;
            set->opener = 0;
            xset_set_plugin_mirror(set);
            if ((set->plugin_top = top))
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
    if (plugin_data->job == PluginJob::REMOVE) // uninstall
    {
        if (!std::filesystem::exists(plugin_data->plug_dir))
        {
            xset_custom_delete(plugin_data->set, false);
            clean_plugin_mirrors();
        }
    }
    else
    {
        const std::string plugin =
            Glib::build_filename(plugin_data->plug_dir, PLUGIN_FILE_FILENAME);
        if (std::filesystem::exists(plugin))
        {
            PluginUse use = PluginUse::NORMAL;
            set = xset_import_plugin(plugin_data->plug_dir, &use);
            if (!set)
            {
                const std::string msg = fmt::format(
                    "The imported plugin directory does not contain a valid plugin.\n\n({}/)",
                    plugin_data->plug_dir);
                xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Invalid Plugin",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                msg);
            }
            else if (use != PluginUse::BOOKMARKS)
            {
                // handler
                set->plugin_top = false; // prevent being added to Plugins menu
                if (plugin_data->job == PluginJob::INSTALL)
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
                    ptk_handler_import(INT(use), plugin_data->handler_dlg, set);
                }
            }
            else if (plugin_data->job == PluginJob::COPY)
            {
                // copy
                set->plugin_top = false; // do not show tmp plugin in Plugins menu
                if (plugin_data->set)
                {
                    // paste after insert_set (plugin_data->set)
                    xset_t newset = xset_custom_copy(set, false, true);
                    newset->prev = ztd::strdup(plugin_data->set->name);
                    newset->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        xset_t set_next = xset_get(plugin_data->set->next);
                        std::free(set_next->prev);
                        set_next->prev = ztd::strdup(newset->name);
                    }
                    plugin_data->set->next = ztd::strdup(newset->name);
                    if (plugin_data->set->tool != XSetTool::NOT)
                    {
                        newset->tool = XSetTool::CUSTOM;
                    }
                    else
                    {
                        newset->tool = XSetTool::NOT;
                    }
                }
                else
                {
                    // place on design clipboard
                    xset_set_clipboard = set;
                    xset_clipboard_is_cut = false;
                    if (xset_get_b(XSetName::PLUG_CVERB) || plugin_data->handler_dlg)
                    {
                        std::string msg;
                        const std::string label = clean_label(set->menu_label, false, false);
                        if (geteuid() == 0)
                        {
                            msg = fmt::format(
                                "The '{}' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu.",
                                label);
                        }
                        else
                        {
                            msg = fmt::format(
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
install_plugin_file(void* main_win, GtkWidget* handler_dlg, std::string_view path,
                    std::string_view plug_dir, PluginJob job, xset_t insert_set)
{
    std::string own;
    const std::string plug_dir_q = ztd::shell::quote(plug_dir);
    const std::string file_path_q = ztd::shell::quote(path);

    MainWindow* main_window = MAIN_WINDOW(main_win);
    // task
    PtkFileTask* ptask = ptk_file_exec_new("Install Plugin",
                                           main_win ? GTK_WIDGET(main_window) : nullptr,
                                           main_win ? main_window->task_view : nullptr);

    switch (job)
    {
        case PluginJob::INSTALL:
            // install
            own =
                fmt::format("chown -R root:root {} && chmod -R go+rX-w {}", plug_dir_q, plug_dir_q);
            ptask->task->exec_as_user = "root";
            break;
        case PluginJob::COPY:
            // copy to clipboard or import to menu
            own = fmt::format("chmod -R go+rX-w {}", plug_dir_q);
            break;
        case PluginJob::REMOVE:
        default:
            break;
    }

    std::string book;
    if (job == PluginJob::INSTALL || !insert_set)
    {
        // prevent install of exported bookmarks or handler as plugin or design clipboard
        if (job == PluginJob::INSTALL)
        {
            book = " || [ -e main_book ] || [ -d hand_* ]";
        }
        else
        {
            book = " || [ -e main_book ]";
        }
    }

    ptask->task->exec_command = fmt::format(
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
    plugin_data->plug_dir = ztd::strdup(plug_dir.data());
    plugin_data->job = job;
    plugin_data->set = insert_set;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}
