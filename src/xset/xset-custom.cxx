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

#include "ptk/ptk-dialog.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-utils.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"

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

            // only use free xset name if no aux data dirs exist for that name too.
            if (!std::filesystem::exists(path1))
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
    if (std::filesystem::exists(path1))
    {
        std::filesystem::remove_all(path1);
        ztd::logger::info("Removed {}", path1.string());
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
                menu_label = desktop->display_name();
            }
            if (set->icon)
            {
                icon_new = vfs_load_icon(set->icon.value(), icon_size);
            }
            if (!icon_new)
            {
                icon_new = desktop->icon(icon_size);
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

char*
xset_custom_get_script(xset_t set, bool create)
{
    assert(set != nullptr);

    if ((!ztd::startswith(set->name, "cstm_") && !ztd::startswith(set->name, "cust") &&
         !ztd::startswith(set->name, "hand")))
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

    path = vfs::user_dirs->program_config_dir() / "scripts" / set->name / "exec.fish";

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

    std::filesystem::path path_dest;

    const auto path_src = vfs::user_dirs->program_config_dir() / "scripts" / src->name;
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
        ptk_show_error(
            nullptr,
            "Copy Command Error",
            std::format("An error occured copying command files\n\n{}", *standard_error));
    }
    command = std::format("chmod -R go-rwx {}", path_dest.string());
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_sync(command);
}

xset_t
xset_custom_copy(xset_t set, bool copy_next)
{
    assert(set != nullptr);

    // ztd::logger::info("xset_custom_copy({}, {})", set->name, copy_next);

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
    newset->context = set->context;
    newset->line = set->line;

    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->icon = set->icon;

    xset_custom_copy_files(set, newset);
    newset->tool = set->tool;

    if (set->menu_style == xset::menu::submenu && set->child)
    {
        xset_t set_child = xset_get(set->child.value());
        // ztd::logger::info("    copy submenu {}", set_child->name);
        xset_t newchild = xset_custom_copy(set_child, true);
        newset->child = newchild->name;
        newchild->parent = newset->name;
    }

    if (copy_next && set->next)
    {
        xset_t set_next = xset_get(set->next.value());
        // ztd::logger::info("    copy next {}", set_next->name);
        xset_t newnext = xset_custom_copy(set_next, true);
        newnext->prev = newset->name;
        newset->next = newnext->name;
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
