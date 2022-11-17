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

#include <fmt/format.h>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils.hxx"

#include "types.hxx"

#include "write.hxx"
#include "settings.hxx"

#include "settings/config-save.hxx"
#include "settings/disk-format.hxx"
#include "settings/plugins-save.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-utils.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-plugins.hxx"

const std::string
xset_custom_new_name()
{
    std::string setname;

    while (true)
    {
        setname = fmt::format("cstm_{}", randhex8());
        if (!xset_is(setname))
        {
            const std::string path1 =
                Glib::build_filename(vfs_user_get_config_dir(), "scripts", setname);
            const std::string path2 =
                Glib::build_filename(vfs_user_get_config_dir(), "plugin-data", setname);

            // only use free xset name if no aux data dirs exist for that name too.
            if (!std::filesystem::exists(path1) && !std::filesystem::exists(path2))
                break;
        }
    };

    return setname;
}

xset_t
xset_custom_new()
{
    const std::string setname = xset_custom_new_name();

    xset_t set;
    set = xset_get(setname);
    set->lock = false;
    set->keep_terminal = true;
    set->task = true;
    set->task_err = true;
    set->task_out = true;
    return set;
}

void
xset_custom_delete(xset_t set, bool delete_next)
{
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        xset_t set_child = xset_get(set->child);
        xset_custom_delete(set_child, true);
    }

    if (delete_next && set->next)
    {
        xset_t set_next = xset_get(set->next);
        xset_custom_delete(set_next, true);
    }

    if (set == xset_set_clipboard)
        xset_set_clipboard = nullptr;

    const std::string path1 = Glib::build_filename(vfs_user_get_config_dir(), "scripts", set->name);
    const std::string path2 =
        Glib::build_filename(vfs_user_get_config_dir(), "plugin-data", set->name);
    if (std::filesystem::exists(path1))
    {
        std::filesystem::remove_all(path1);
        LOG_INFO("Removed {}", path1);
    }

    if (std::filesystem::exists(path2))
    {
        std::filesystem::remove_all(path2);
        LOG_INFO("Removed {}", path2);
    }

    xset_remove(set);
}

xset_t
xset_custom_remove(xset_t set)
{
    xset_t set_prev;
    xset_t set_next;
    xset_t set_parent;
    xset_t set_child;

    /*
    LOG_INFO("xset_custom_remove {} ({})", set->name, set->menu_label );
    LOG_INFO("    set->parent = {}", set->parent );
    LOG_INFO("    set->prev   = {}", set->prev );
    LOG_INFO("    set->next   = {}", set->next );
    */
    if (set->prev)
    {
        set_prev = xset_get(set->prev);
        // LOG_INFO("        set->prev = {} ({})", set_prev->name, set_prev->menu_label);
        if (set_prev->next)
            free(set_prev->next);
        if (set->next)
            set_prev->next = ztd::strdup(set->next);
        else
            set_prev->next = nullptr;
    }
    if (set->next)
    {
        set_next = xset_get(set->next);
        if (set_next->prev)
            free(set_next->prev);
        if (set->prev)
            set_next->prev = ztd::strdup(set->prev);
        else
        {
            set_next->prev = nullptr;
            if (set->parent)
            {
                set_parent = xset_get(set->parent);
                if (set_parent->child)
                    free(set_parent->child);
                set_parent->child = ztd::strdup(set_next->name);
                set_next->parent = ztd::strdup(set->parent);
            }
        }
    }
    if (!set->prev && !set->next && set->parent)
    {
        set_parent = xset_get(set->parent);
        if (set->tool != XSetTool::NOT)
            set_child = xset_new_builtin_toolitem(XSetTool::HOME);
        else
        {
            set_child = xset_custom_new();
            set_child->menu_label = ztd::strdup("New _Command");
        }
        if (set_parent->child)
            free(set_parent->child);
        set_parent->child = ztd::strdup(set_child->name);
        set_child->parent = ztd::strdup(set->parent);
        return set_child;
    }
    return nullptr;
}

char*
xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, i32 icon_size)
{
    char* menu_label = nullptr;
    GdkPixbuf* icon_new = nullptr;

    if (!set->lock && XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::APP)
    {
        if (set->z && ztd::endswith(set->z, ".desktop"))
        {
            vfs::desktop desktop = vfs_get_desktop(set->z);

            if (!(set->menu_label && set->menu_label[0]))
                menu_label = ztd::strdup(desktop->get_disp_name());
            if (set->icon)
                icon_new = vfs_load_icon(set->icon, icon_size);
            if (!icon_new)
                icon_new = desktop->get_icon(icon_size);
        }
        else
        {
            // not a desktop file - probably executable
            if (set->icon)
                icon_new = vfs_load_icon(set->icon, icon_size);
            if (!icon_new && set->z)
            {
                // guess icon name from executable name
                const std::string name = Glib::path_get_basename(set->z);
                icon_new = vfs_load_icon(name, icon_size);
            }
        }

        if (!icon_new)
        {
            // fallback
            icon_new = vfs_load_icon("gtk-execute", icon_size);
        }
    }
    else
        LOG_WARN("xset_custom_get_app_name_icon set is not XSetCMD::APP");

    if (icon)
        *icon = icon_new;
    else if (icon_new)
        g_object_unref(icon_new);

    if (!menu_label)
    {
        menu_label = set->menu_label && set->menu_label[0] ? ztd::strdup(set->menu_label)
                                                           : ztd::strdup(set->z);
        if (!menu_label)
            menu_label = ztd::strdup("Application");
    }
    return menu_label;
}

static bool
xset_custom_export_files(xset_t set, std::string_view plug_dir)
{
    std::string path_src;
    std::string path_dest;

    if (set->plugin)
    {
        path_src = Glib::build_filename(set->plug_dir, set->plug_name);
        path_dest = Glib::build_filename(plug_dir.data(), set->plug_name);
    }
    else
    {
        path_src = Glib::build_filename(vfs_user_get_config_dir(), "scripts", set->name);
        path_dest = Glib::build_filename(plug_dir.data(), set->name);
    }

    if (!(std::filesystem::exists(path_src) && dir_has_files(path_src)))
    {
        // skip empty or missing dirs
        return true;
    }

    i32 exit_status;
    const std::string command = fmt::format("cp -a {} {}", path_src, path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    return !!exit_status;
}

static bool
xset_custom_export_write(xsetpak_t& xsetpak, xset_t set, std::string_view plug_dir)
{ // recursively write set, submenu sets, and next sets
    xsetpak_t xsetpak_local{{fmt::format("{}", set->name), xset_pack_set(set)}};

    xsetpak.insert(xsetpak_local.begin(), xsetpak_local.end());

    if (!xset_custom_export_files(set, plug_dir))
        return false;
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->child), plug_dir))
            return false;
    }
    if (set->next)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->next), plug_dir))
            return false;
    }
    return true;
}

void
xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set)
{
    const char* deffolder;
    std::string deffile;

    // get new plugin filename
    xset_t save = xset_get(XSetName::PLUG_CFILE);
    if (save->s) //&& std::filesystem::is_directory(save->s)
    {
        deffolder = save->s;
    }
    else
    {
        if (!(deffolder = xset_get_s(XSetName::GO_SET_DEFAULT)))
            deffolder = ztd::strdup("/");
    }

    if (!set->plugin)
    {
        const std::string s1 = clean_label(set->menu_label, true, false);
        std::string type;
        if (ztd::startswith(set->name, "hand_arc_"))
            type = "archive-handler";
        else if (ztd::startswith(set->name, "hand_fs_"))
            type = "device-handler";
        else if (ztd::startswith(set->name, "hand_net_"))
            type = "protocol-handler";
        else if (ztd::startswith(set->name, "hand_f_"))
            type = "file-handler";
        else
            type = "plugin";

        deffile = fmt::format("{}-{}-{}.tar.xz", s1, PACKAGE_NAME, type);
    }
    else
    {
        const std::string s1 = Glib::path_get_basename(set->plug_dir);
        deffile = fmt::format("{}-{}-plugin.tar.xz", s1, PACKAGE_NAME);
    }

    char* path = xset_file_dialog(parent,
                                  GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
                                  "Save As Plugin File",
                                  deffolder,
                                  deffile.data());
    if (!path)
        return;
    if (save->s)
        free(save->s);
    save->s = ztd::strdup(Glib::path_get_dirname(path));

    // get or create tmp plugin dir
    std::string plug_dir;
    if (!set->plugin)
    {
        const std::string user_tmp = vfs_user_get_tmp_dir();
        if (!std::filesystem::is_directory(user_tmp))
        {
            free(path);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Export Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        while (true)
        {
            plug_dir = Glib::build_filename(user_tmp, randhex8());
            if (!std::filesystem::exists(plug_dir))
                break;
        }
        std::filesystem::create_directories(plug_dir);
        std::filesystem::permissions(plug_dir, std::filesystem::perms::owner_all);

        // Create plugin file
        // const std::string plugin_path = Glib::build_filename(plug_dir, PLUGIN_FILE_FILENAME);

        // Do not want to store plugins prev/next/parent
        // TODO - Better way
        char* s_prev = set->prev;
        char* s_next = set->next;
        char* s_parent = set->parent;
        set->prev = nullptr;
        set->next = nullptr;
        set->parent = nullptr;
        xsetpak_t xsetpak{{fmt::format("{}", set->name), xset_pack_set(set)}};
        set->prev = s_prev;
        set->next = s_next;
        set->parent = s_parent;
        //

        if (!xset_custom_export_files(set, plug_dir))
        {
            if (!set->plugin)
            {
                std::filesystem::remove_all(plug_dir);
                LOG_INFO("Removed {}", plug_dir);
            }
            free(path);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Export Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        if (set->menu_style == XSetMenu::SUBMENU && set->child)
        {
            if (!xset_custom_export_write(xsetpak, xset_get(set->child), plug_dir))
            {
                if (!set->plugin)
                {
                    std::filesystem::remove_all(plug_dir);
                    LOG_INFO("Removed {}", plug_dir);
                }
                free(path);
                xset_msg_dialog(parent,
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Export Error",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                "Unable to create temporary files");
                return;
            }
        }

        save_user_plugin(path, xsetpak);
    }
    else
    {
        plug_dir = ztd::strdup(set->plug_dir);
    }

    // tar and delete tmp files
    // task
    PtkFileTask* ptask;
    ptask = ptk_file_exec_new("Export Plugin",
                              plug_dir,
                              parent,
                              file_browser ? file_browser->task_view : nullptr);

    const std::string plug_dir_q = bash_quote(plug_dir);
    const std::string path_q = bash_quote(path);
    if (!set->plugin)
        ptask->task->exec_command =
            fmt::format("tar --numeric-owner -cJf {} * ; err=$? ; rm -rf {} ; "
                        "if [ $err -ne 0 ];then rm -f {} ; fi ; exit $err",
                        path_q,
                        plug_dir_q,
                        path_q);
    else
        ptask->task->exec_command =
            fmt::format("tar --numeric-owner -cJf {} * ; err=$? ; "
                        "if [ $err -ne 0 ] ; then rm -f {} ; fi ; exit $err",
                        path_q,
                        path_q);
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_browser = file_browser;
    ptk_file_task_run(ptask);

    free(path);
    return;
}

char*
xset_custom_get_script(xset_t set, bool create)
{
    if ((!ztd::startswith(set->name, "cstm_") && !ztd::startswith(set->name, "cust") &&
         !ztd::startswith(set->name, "hand")) ||
        (create && set->plugin))
        return nullptr;

    std::string path;

    if (create)
    {
        path = Glib::build_filename(vfs_user_get_config_dir(), "scripts", set->name);
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    }

    if (set->plugin)
    {
        path = Glib::build_filename(set->plug_dir, set->plug_name, "exec.sh");
    }
    else
    {
        path = Glib::build_filename(vfs_user_get_config_dir(), "scripts", set->name, "exec.sh");
    }

    if (create && !std::filesystem::exists(path))
    {
        std::string data;
        data.append(fmt::format("#!{}\n", BASH_PATH));
        data.append(fmt::format("{}\n\n", SHELL_SETTINGS));
        data.append("#import file manager variables\n");
        data.append("$fm_import\n\n");
        data.append("#For all spacefm variables see man page: spacefm-scripts\n\n");
        data.append("#Start script\n");
        data.append("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        data.append("#End script\n");
        data.append("exit $?\n");

        write_file(path, data);

        if (std::filesystem::exists(path))
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    }
    return ztd::strdup(path);
}

static void
xset_custom_copy_files(xset_t src, xset_t dest)
{
    std::string path_src;
    std::string path_dest;
    std::string command;

    std::string* standard_output = nullptr;
    std::string* standard_error = nullptr;
    i32 exit_status;

    // LOG_INFO("xset_custom_copy_files( {}, {} )", src->name, dest->name);

    // copy command dir

    if (src->plugin)
        path_src = Glib::build_filename(src->plug_dir, src->plug_name);
    else
        path_src = Glib::build_filename(vfs_user_get_config_dir(), "scripts", src->name);
    // LOG_INFO("    path_src={}", path_src);

    // LOG_INFO("    path_src EXISTS");
    path_dest = Glib::build_filename(vfs_user_get_config_dir(), "scripts");
    std::filesystem::create_directories(path_dest);
    std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
    path_dest = Glib::build_filename(vfs_user_get_config_dir(), "scripts", dest->name);
    command = fmt::format("cp -a {} {}", path_src, path_dest);

    // LOG_INFO("    path_dest={}", path_dest );
    print_command(command);
    Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
    LOG_INFO("{}{}", *standard_output, *standard_error);
    if (exit_status && WIFEXITED(exit_status))
    {
        const std::string msg =
            fmt::format("An error occured copying command files\n\n{}", *standard_error);
        xset_msg_dialog(nullptr,
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Copy Command Error",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        msg);
    }
    command = fmt::format("chmod -R go-rwx {}", path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command);

    // copy data dir
    xset_t mset = xset_get_plugin_mirror(src);
    path_src = Glib::build_filename(vfs_user_get_config_dir(), "plugin-data", mset->name);
    if (std::filesystem::is_directory(path_src))
    {
        path_dest = Glib::build_filename(vfs_user_get_config_dir(), "plugin-data", dest->name);
        command = fmt::format("cp -a {} {}", path_src, path_dest);
        print_command(command);
        Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
        LOG_INFO("{}{}", *standard_output, *standard_error);
        if (exit_status && WIFEXITED(exit_status))
        {
            const std::string msg =
                fmt::format("An error occured copying command data files\n\n{}", *standard_error);
            xset_msg_dialog(nullptr,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Copy Command Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            msg);
        }
        command = fmt::format("chmod -R go-rwx {}", path_dest);
        print_command(command);
        Glib::spawn_command_line_sync(command);
    }
}

xset_t
xset_custom_copy(xset_t set, bool copy_next, bool delete_set)
{
    // LOG_INFO("xset_custom_copy( {}, {}, {})", set->name, copy_next ? "true" : "false",
    // delete_set ? "true" : "false");
    xset_t mset = set;
    // if a plugin with a mirror, get the mirror
    if (set->plugin && set->shared_key)
        mset = xset_get_plugin_mirror(set);

    xset_t newset = xset_custom_new();
    newset->menu_label = ztd::strdup(set->menu_label);
    newset->s = ztd::strdup(set->s);
    newset->x = ztd::strdup(set->x);
    newset->y = ztd::strdup(set->y);
    newset->z = ztd::strdup(set->z);
    newset->desc = ztd::strdup(set->desc);
    newset->title = ztd::strdup(set->title);
    newset->b = set->b;
    newset->menu_style = set->menu_style;
    newset->context = ztd::strdup(mset->context);
    newset->line = ztd::strdup(set->line);

    newset->task = mset->task;
    newset->task_pop = mset->task_pop;
    newset->task_err = mset->task_err;
    newset->task_out = mset->task_out;
    newset->in_terminal = mset->in_terminal;
    newset->keep_terminal = mset->keep_terminal;
    newset->scroll_lock = mset->scroll_lock;

    if (!mset->icon && set->plugin)
        newset->icon = ztd::strdup(set->icon);
    else
        newset->icon = ztd::strdup(mset->icon);

    xset_custom_copy_files(set, newset);
    newset->tool = set->tool;

    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        xset_t set_child = xset_get(set->child);
        // LOG_INFO("    copy submenu {}", set_child->name);
        xset_t newchild = xset_custom_copy(set_child, true, delete_set);
        newset->child = ztd::strdup(newchild->name);
        newchild->parent = ztd::strdup(newset->name);
    }

    if (copy_next && set->next)
    {
        xset_t set_next = xset_get(set->next);
        // LOG_INFO("    copy next {}", set_next->name);
        xset_t newnext = xset_custom_copy(set_next, true, delete_set);
        newnext->prev = ztd::strdup(newset->name);
        newset->next = ztd::strdup(newnext->name);
    }

    // when copying imported plugin file, discard mirror xset
    if (delete_set)
        xset_custom_delete(set, false);

    return newset;
}

xset_t
xset_find_custom(std::string_view search)
{
    // find a custom command or submenu by label or xset name
    const std::string label = clean_label(search, true, false);

    for (xset_t set : xsets)
    {
        if (!set->lock && ((set->menu_style == XSetMenu::SUBMENU && set->child) ||
                           (set->menu_style < XSetMenu::SUBMENU &&
                            XSetCMD(xset_get_int(set, XSetVar::X)) <= XSetCMD::BOOKMARK)))
        {
            // custom submenu or custom command - label or name matches?
            const std::string str = clean_label(set->menu_label, true, false);
            if (ztd::same(set->name, search) || ztd::same(str, label))
            {
                // match
                return set;
            }
        }
    }
    return nullptr;
}
