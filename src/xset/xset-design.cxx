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

#include <cassert>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-keyboard.hxx"

#include "autosave.hxx"
#include "utils.hxx"

#include "settings.hxx"

#include "main-window.hxx"

static void
xset_design_job_set_key(xset_t set)
{
#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(set->browser)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));
#endif

    xset_set_key(parent, set);
}

static void
xset_design_job_set_add_tool(xset_t set, xset::tool tool_type)
{
    if (tool_type < xset::tool::devices || tool_type >= xset::tool::invalid ||
        set->tool == xset::tool::NOT)
    {
        return;
    }
    xset_t newset = xset_new_builtin_toolitem(tool_type);
    if (newset)
    {
        xset_custom_insert_after(set, newset);
    }
}

static void
xset_design_job_set_cut(xset_t set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = true;
}

static void
xset_design_job_set_copy(xset_t set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = false;
}

static bool
xset_design_job_set_paste(xset_t set)
{
    if (!xset_set_clipboard)
    {
        return false;
    }

    if (xset_set_clipboard->tool > xset::tool::custom && set->tool == xset::tool::NOT)
    {
        // failsafe - disallow pasting a builtin tool to a menu
        return false;
    }

    bool update_toolbars = false;
    if (xset_clipboard_is_cut)
    {
        update_toolbars = !(xset_set_clipboard->tool == xset::tool::NOT);
        if (!update_toolbars && xset_set_clipboard->parent)
        {
            xset_t newset = xset_get(xset_set_clipboard->parent.value());
            if (!(newset->tool == xset::tool::NOT))
            {
                // we are cutting the first item in a tool submenu
                update_toolbars = true;
            }
        }
        xset_custom_remove(xset_set_clipboard);
        xset_custom_insert_after(set, xset_set_clipboard);

        xset_set_clipboard = nullptr;
    }
    else
    {
        xset_t newset = xset_custom_copy(xset_set_clipboard, false);
        xset_custom_insert_after(set, newset);
    }

    return update_toolbars;
}

static bool
xset_design_job_set_remove(xset_t set)
{
    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));

    std::string name;
    if (set->menu_label)
    {
        name = clean_label(set->menu_label.value(), false, false);
    }
    else
    {
        if (!set->lock && set->z && set->menu_style < xset::menu::submenu &&
            cmd_type == xset::cmd::app)
        {
            name = set->z.value();
        }
        else
        {
            name = "( no name )";
        }
    }

    bool update_toolbars = false;

    // remove
    if (set->parent)
    {
        const xset_t set_next = xset_is(set->parent.value());
        if (set_next && set_next->tool == xset::tool::custom &&
            set_next->menu_style == xset::menu::submenu)
        {
            // this set is first item in custom toolbar submenu
            update_toolbars = true;
        }
    }

    xset_custom_remove(set);

    if (!(set->tool == xset::tool::NOT))
    {
        update_toolbars = true;
    }

    return update_toolbars;
}

void
xset_design_job(GtkWidget* item, xset_t set)
{
    assert(set != nullptr);

    bool update_toolbars = false;

    const auto job = xset::job(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job")));

    // ztd::logger::info("activate job {} {}", (i32)job, set->name);
    switch (job)
    {
        case xset::job::key:
        {
            xset_design_job_set_key(set);
            break;
        }
        case xset::job::add_tool:
        {
            const auto tool_type =
                xset::tool(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tool_type")));
            xset_design_job_set_add_tool(set, tool_type);
            break;
        }
        case xset::job::cut:
        {
            xset_design_job_set_cut(set);
            break;
        }
        case xset::job::copy:
        {
            xset_design_job_set_copy(set);
            break;
        }
        case xset::job::paste:
        {
            update_toolbars = xset_design_job_set_paste(set);
            break;
        }
        case xset::job::remove:
        case xset::job::remove_book:
        {
            update_toolbars = xset_design_job_set_remove(set);
            break;
        }
        case xset::job::invalid:
            break;
    }

    if ((set && !set->lock && !(set->tool == xset::tool::NOT)) || update_toolbars)
    {
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);
    }

    // autosave
    autosave_request_add();
}
