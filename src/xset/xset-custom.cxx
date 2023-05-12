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

#include <optional>

#include <cassert>

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
#include "vfs/vfs-user-dirs.hxx"
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
        setname = std::format("cstm_{}", ztd::randhex());
        if (!xset_is(setname))
        {
            const auto path1 = vfs::user_dirs->program_config_dir() / "scripts" / setname;
            const auto path2 = vfs::user_dirs->program_config_dir() / "plugin-data" / setname;

            // only use free xset name if no aux data dirs exist for that name too.
            if (!std::filesystem::exists(path1) && !std::filesystem::exists(path2))
            {
                break;
            }
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
    assert(set != nullptr);

    if (set->menu_style == xset::menu::submenu && set->child)
    {
        xset_t set_child = xset_get(set->child.value());
        xset_custom_delete(set_child, true);
    }

    if (delete_next && set->next)
    {
        xset_t set_next = xset_get(set->next.value());
        xset_custom_delete(set_next, true);
    }

    if (set == xset_set_clipboard)
    {
        xset_set_clipboard = nullptr;
    }

    const auto path1 = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
    const auto path2 = vfs::user_dirs->program_config_dir() / "plugin-data" / set->name;
    if (std::filesystem::exists(path1))
    {
        std::filesystem::remove_all(path1);
        ztd::logger::info("Removed {}", path1);
    }

    if (std::filesystem::exists(path2))
    {
        std::filesystem::remove_all(path2);
        ztd::logger::info("Removed {}", path2);
    }

    xset_remove(set);
}

xset_t
xset_custom_remove(xset_t set)
{
    assert(set != nullptr);

    xset_t set_prev;
    xset_t set_next;
    xset_t set_parent;
    xset_t set_child;

    /*
    ztd::logger::info("xset_custom_remove {} ({})", set->name, set->menu_label );
    ztd::logger::info("    set->parent = {}", set->parent );
    ztd::logger::info("    set->prev   = {}", set->prev );
    ztd::logger::info("    set->next   = {}", set->next );
    */
    if (set->prev)
    {
        set_prev = xset_get(set->prev.value());
        // ztd::logger::info("        set->prev = {} ({})", set_prev->name, set_prev->menu_label);
        if (set->next)
        {
            set_prev->next = set->next;
        }
        else
        {
            set_prev->next = std::nullopt;
        }
    }
    if (set->next)
    {
        set_next = xset_get(set->next.value());
        if (set->prev)
        {
            set_next->prev = set->prev;
        }
        else
        {
            set_next->prev = std::nullopt;
            if (set->parent)
            {
                set_parent = xset_get(set->parent.value());
                set_parent->child = set_next->name;
                set_next->parent = set->parent;
            }
        }
    }
    if (!set->prev && !set->next && set->parent)
    {
        set_parent = xset_get(set->parent.value());
        if (set->tool != xset::tool::NOT)
        {
            set_child = xset_new_builtin_toolitem(xset::tool::home);
        }
        else
        {
            set_child = xset_custom_new();
            set_child->menu_label = "New _Command";
        }
        set_parent->child = set_child->name;
        set_child->parent = set->parent;
        return set_child;
    }
    return nullptr;
}

const std::string
xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, i32 icon_size)
{
    assert(set != nullptr);

    std::string menu_label;
    GdkPixbuf* icon_new = nullptr;

    if (!set->lock && xset::cmd(xset_get_int(set, xset::var::x)) == xset::cmd::app)
    {
        if (set->z && ztd::endswith(set->z.value(), ".desktop"))
        {
            const vfs::desktop desktop = vfs_get_desktop(set->z.value());

            if (!set->menu_label)
            {
                menu_label = desktop->get_disp_name();
            }
            if (set->icon)
            {
                icon_new = vfs_load_icon(set->icon.value(), icon_size);
            }
            if (!icon_new)
            {
                icon_new = desktop->get_icon(icon_size);
            }
        }
        else
        {
            // not a desktop file - probably executable
            if (set->icon)
            {
                icon_new = vfs_load_icon(set->icon.value(), icon_size);
            }
            if (!icon_new && set->z)
            {
                // guess icon name from executable name
                const auto path = std::filesystem::path(set->z.value());
                const auto name = path.filename();
                icon_new = vfs_load_icon(name.string(), icon_size);
            }
        }

        if (!icon_new)
        {
            // fallback
            icon_new = vfs_load_icon("gtk-execute", icon_size);
        }
    }
    else
    {
        ztd::logger::warn("xset_custom_get_app_name_icon set is not xset::cmd::APP");
    }

    if (icon)
    {
        *icon = icon_new;
    }
    else if (icon_new)
    {
        g_object_unref(icon_new);
    }

    if (menu_label.empty())
    {
        menu_label = set->menu_label ? set->menu_label.value() : set->z.value();
        if (menu_label.empty())
        {
            menu_label = "Application";
        }
    }
    return menu_label;
}

static bool
xset_custom_export_files(xset_t set, const std::filesystem::path& plug_dir)
{
    assert(set != nullptr);

    std::filesystem::path path_src;
    std::filesystem::path path_dest;

    if (set->plugin)
    {
        path_src = set->plugin->path / set->plugin->name;
        path_dest = plug_dir / set->plugin->name;
    }
    else
    {
        path_src = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
        path_dest = plug_dir / set->name;
    }

    if (!(std::filesystem::exists(path_src) && dir_has_files(path_src)))
    {
        // skip empty or missing dirs
        return true;
    }

    i32 exit_status;
    const std::string command = std::format("cp -a {} {}", path_src.string(), path_dest.string());
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    return !!exit_status;
}

static bool
xset_custom_export_write(xsetpak_t& xsetpak, xset_t set, const std::filesystem::path& plug_dir)
{ // recursively write set, submenu sets, and next sets
    xsetpak_t xsetpak_local{{std::format("{}", set->name), xset_pack_set(set)}};

    xsetpak.insert(xsetpak_local.begin(), xsetpak_local.end());

    if (!xset_custom_export_files(set, plug_dir))
    {
        return false;
    }
    if (set->menu_style == xset::menu::submenu && set->child)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->child.value()), plug_dir))
        {
            return false;
        }
    }
    if (set->next)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->next.value()), plug_dir))
        {
            return false;
        }
    }
    return true;
}

void
xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set)
{
    std::optional<std::string> default_path = std::nullopt;
    std::string deffile;

    // get new plugin filename
    xset_t save = xset_get(xset::name::plug_cfile);
    if (save->s) //&& std::filesystem::is_directory(save->s)
    {
        default_path = save->s;
    }
    else
    {
        default_path = xset_get_s(xset::name::go_set_default);
        if (!default_path)
        {
            default_path = "/";
        }
    }

    if (!set->plugin)
    {
        const std::string s1 = clean_label(set->menu_label.value(), true, false);
        std::string type;
        if (ztd::startswith(set->name, "hand_arc_"))
        {
            type = "archive-handler";
        }
        else if (ztd::startswith(set->name, "hand_fs_"))
        {
            type = "device-handler";
        }
        else if (ztd::startswith(set->name, "hand_net_"))
        {
            type = "protocol-handler";
        }
        else if (ztd::startswith(set->name, "hand_f_"))
        {
            type = "file-handler";
        }
        else
        {
            type = "plugin";
        }

        deffile = std::format("{}-{}-{}.tar.xz", s1, PACKAGE_NAME, type);
    }
    else
    {
        const auto s1 = set->plugin->path.filename();
        // Need to use .string() to avoid fmt adding double quotes when formating
        deffile = std::format("{}-{}-plugin.tar.xz", s1.string(), PACKAGE_NAME);
    }

    char* path = xset_file_dialog(parent,
                                  GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
                                  "Save As Plugin File",
                                  default_path.value().c_str(),
                                  deffile.data());
    if (!path)
    {
        return;
    }
    save->s = std::filesystem::path(path).parent_path();

    // get or create tmp plugin dir
    std::filesystem::path plug_dir;
    if (!set->plugin)
    {
        const auto user_tmp = vfs::user_dirs->program_tmp_dir();
        if (!std::filesystem::is_directory(user_tmp))
        {
            std::free(path);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Export Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        while (true)
        {
            plug_dir = user_tmp / ztd::randhex();
            if (!std::filesystem::exists(plug_dir))
            {
                break;
            }
        }
        std::filesystem::create_directories(plug_dir);
        std::filesystem::permissions(plug_dir, std::filesystem::perms::owner_all);

        // Create plugin file
        // const auto plugin_path = plug_dir / PLUGIN_FILE_FILENAME;

        // Do not want to store plugins prev/next/parent
        // TODO - Better way
        const auto s_prev = set->prev;
        const auto s_next = set->next;
        const auto s_parent = set->parent;
        set->prev = std::nullopt;
        set->next = std::nullopt;
        set->parent = std::nullopt;
        xsetpak_t xsetpak{{std::format("{}", set->name), xset_pack_set(set)}};
        set->prev = s_prev;
        set->next = s_next;
        set->parent = s_parent;
        //

        if (!xset_custom_export_files(set, plug_dir))
        {
            if (!set->plugin)
            {
                std::filesystem::remove_all(plug_dir);
                ztd::logger::info("Removed {}", plug_dir);
            }
            std::free(path);
            xset_msg_dialog(parent,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Export Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        if (set->menu_style == xset::menu::submenu && set->child)
        {
            if (!xset_custom_export_write(xsetpak, xset_get(set->child.value()), plug_dir))
            {
                if (!set->plugin)
                {
                    std::filesystem::remove_all(plug_dir);
                    ztd::logger::info("Removed {}", plug_dir);
                }
                std::free(path);
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
        plug_dir = set->plugin->path;
    }

    // tar and delete tmp files
    // task
    PtkFileTask* ptask;
    ptask = ptk_file_exec_new("Export Plugin",
                              plug_dir,
                              parent,
                              file_browser ? file_browser->task_view : nullptr);

    const std::string plug_dir_q = ztd::shell::quote(plug_dir.string());
    const std::string path_q = ztd::shell::quote(path);
    if (!set->plugin)
    {
        ptask->task->exec_command =
            std::format("tar --numeric-owner -cJf {} * ; err=$status ; rm -rf {} ; "
                        "if [ $err -ne 0 ];then rm -f {} ; fi ; exit $err",
                        path_q,
                        plug_dir_q,
                        path_q);
    }
    else
    {
        ptask->task->exec_command =
            std::format("tar --numeric-owner -cJf {} * ; err=$status ; "
                        "if [ $err -ne 0 ] ; then rm -f {} ; fi ; exit $err",
                        path_q,
                        path_q);
    }
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_browser = file_browser;
    ptk_file_task_run(ptask);

    std::free(path);
    return;
}

char*
xset_custom_get_script(xset_t set, bool create)
{
    assert(set != nullptr);

    if ((!ztd::startswith(set->name, "cstm_") && !ztd::startswith(set->name, "cust") &&
         !ztd::startswith(set->name, "hand")) ||
        (create && set->plugin))
    {
        return nullptr;
    }

    std::filesystem::path path;

    if (create)
    {
        path = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    }

    if (set->plugin)
    {
        path = set->plugin->path / set->plugin->name / "exec.fish";
    }
    else
    {
        path = vfs::user_dirs->program_config_dir() / "scripts" / set->name / "exec.fish";
    }

    if (create && !std::filesystem::exists(path))
    {
        std::string data;
        data.append(std::format("#!{}\n", FISH_PATH));
        data.append(std::format("source {}\n\n", FISH_FMLIB));
        data.append("#import file manager variables\n");
        data.append("$fm_import\n\n");
        data.append("#For all spacefm variables see man page: spacefm-scripts\n\n");
        data.append("#Start script\n");
        data.append("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        data.append("#End script\n");
        data.append("exit $status\n");

        write_file(path, data);

        if (std::filesystem::exists(path))
        {
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    }
    return ztd::strdup(path);
}

static void
xset_custom_copy_files(xset_t src, xset_t dest)
{
    assert(src != nullptr);
    assert(dest != nullptr);

    std::string command;

    std::string* standard_output = nullptr;
    std::string* standard_error = nullptr;
    i32 exit_status;

    // ztd::logger::info("xset_custom_copy_files( {}, {} )", src->name, dest->name);

    // copy command dir

    std::filesystem::path path_src;
    std::filesystem::path path_dest;

    if (src->plugin)
    {
        path_src = src->plugin->path / src->plugin->name;
    }
    else
    {
        path_src = vfs::user_dirs->program_config_dir() / "scripts" / src->name;
    }
    // ztd::logger::info("    path_src={}", path_src);

    // ztd::logger::info("    path_src EXISTS");
    path_dest = vfs::user_dirs->program_config_dir() / "scripts";
    std::filesystem::create_directories(path_dest);
    std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
    path_dest = vfs::user_dirs->program_config_dir() / "scripts" / dest->name;
    command = std::format("cp -a {} {}", path_src.string(), path_dest.string());
    // ztd::logger::info("    path_dest={}", path_dest);
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
    std::string out;
    if (standard_output)
    {
        out.append(*standard_output);
    }
    if (standard_error)
    {
        out.append(*standard_error);
    }
    ztd::logger::info("{}", out);
    if (exit_status && WIFEXITED(exit_status))
    {
        const std::string msg =
            std::format("An error occured copying command files\n\n{}", *standard_error);
        xset_msg_dialog(nullptr,
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Copy Command Error",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        msg);
    }
    command = std::format("chmod -R go-rwx {}", path_dest.string());
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_sync(command);

    // copy data dir
    xset_t mset = xset_get_plugin_mirror(src);
    path_src = vfs::user_dirs->program_config_dir() / "plugin-data" / mset->name;
    if (std::filesystem::is_directory(path_src))
    {
        path_dest = vfs::user_dirs->program_config_dir() / "plugin-data" / dest->name;
        command = std::format("cp -a {} {}", path_src.string(), path_dest.string());
        ztd::logger::info("COMMAND={}", command);
        Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
        std::string copy_out;
        if (standard_output)
        {
            copy_out.append(*standard_output);
        }
        if (standard_error)
        {
            copy_out.append(*standard_error);
        }
        ztd::logger::info("{}", copy_out);
        if (exit_status && WIFEXITED(exit_status))
        {
            const std::string msg =
                std::format("An error occured copying command data files\n\n{}", *standard_error);
            xset_msg_dialog(nullptr,
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Copy Command Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            msg);
        }
        command = std::format("chmod -R go-rwx {}", path_dest.string());
        ztd::logger::info("COMMAND={}", command);
        Glib::spawn_command_line_sync(command);
    }
}

xset_t
xset_custom_copy(xset_t set, bool copy_next, bool delete_set)
{
    assert(set != nullptr);

    // ztd::logger::info("xset_custom_copy( {}, {}, {})", set->name, copy_next ? "true" : "false",
    // delete_set ? "true" : "false");
    xset_t mset = set;
    // if a plugin with a mirror, get the mirror
    if (set->plugin && set->shared_key)
    {
        mset = xset_get_plugin_mirror(set);
    }

    xset_t newset = xset_custom_new();
    newset->menu_label = set->menu_label;
    newset->s = set->s;
    newset->x = set->x;
    newset->y = set->y;
    newset->z = set->z;
    newset->desc = set->desc;
    newset->title = set->title;
    newset->b = set->b;
    newset->menu_style = set->menu_style;
    newset->context = mset->context;
    newset->line = set->line;

    newset->task = mset->task;
    newset->task_pop = mset->task_pop;
    newset->task_err = mset->task_err;
    newset->task_out = mset->task_out;
    newset->in_terminal = mset->in_terminal;
    newset->keep_terminal = mset->keep_terminal;
    newset->scroll_lock = mset->scroll_lock;

    if (!mset->icon && set->plugin)
    {
        newset->icon = set->icon;
    }
    else
    {
        newset->icon = mset->icon;
    }

    xset_custom_copy_files(set, newset);
    newset->tool = set->tool;

    if (set->menu_style == xset::menu::submenu && set->child)
    {
        xset_t set_child = xset_get(set->child.value());
        // ztd::logger::info("    copy submenu {}", set_child->name);
        xset_t newchild = xset_custom_copy(set_child, true, delete_set);
        newset->child = newchild->name;
        newchild->parent = newset->name;
    }

    if (copy_next && set->next)
    {
        xset_t set_next = xset_get(set->next.value());
        // ztd::logger::info("    copy next {}", set_next->name);
        xset_t newnext = xset_custom_copy(set_next, true, delete_set);
        newnext->prev = newset->name;
        newset->next = newnext->name;
    }

    // when copying imported plugin file, discard mirror xset
    if (delete_set)
    {
        xset_custom_delete(set, false);
    }

    return newset;
}

xset_t
xset_find_custom(const std::string_view search)
{
    // find a custom command or submenu by label or xset name
    const std::string label = clean_label(search, true, false);

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (!set->lock && ((set->menu_style == xset::menu::submenu && set->child) ||
                           (set->menu_style < xset::menu::submenu &&
                            xset::cmd(xset_get_int(set, xset::var::x)) <= xset::cmd::bookmark)))
        {
            // custom submenu or custom command - label or name matches?
            const std::string str = clean_label(set->menu_label.value(), true, false);
            if (ztd::same(set->name, search) || ztd::same(str, label))
            {
                // match
                return set;
            }
        }
    }
    return nullptr;
}
