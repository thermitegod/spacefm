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
#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "ptk/utils/ptk-utils.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-keyboard.hxx"
#include "xset/xset-toolbar.hxx"
#include "xset/utils/xset-utils.hxx"

#include "autosave.hxx"

#include "main-window.hxx"

static void
xset_design_job_set_key(const xset_t& set)
{
#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(set->browser)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));
#endif

    xset_set_key(parent, set);
}

static void
xset_design_job_set_add_tool(const xset_t& set, xset::tool tool_type)
{
    if (tool_type < xset::tool::devices || tool_type >= xset::tool::invalid ||
        set->tool == xset::tool::NOT)
    {
        return;
    }
    const xset_t newset = xset_new_builtin_toolitem(tool_type);
    if (newset)
    {
        xset_custom_insert_after(set, newset);
    }
}

static void
xset_design_job_set_cut(const xset_t& set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = true;
}

static void
xset_design_job_set_copy(const xset_t& set)
{
    xset_set_clipboard = set;
    xset_clipboard_is_cut = false;
}

static bool
xset_design_job_set_paste(const xset_t& set)
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
            const xset_t newset = xset_get(xset_set_clipboard->parent.value());
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
        const xset_t newset = xset_custom_copy(xset_set_clipboard, false);
        xset_custom_insert_after(set, newset);
    }

    return update_toolbars;
}

static bool
xset_design_job_set_remove(const xset_t& set)
{
    const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));

    std::string name;
    if (set->menu_label)
    {
        name = xset::utils::clean_label(set->menu_label.value(), false, false);
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
xset_design_job(GtkWidget* item, const xset_t& set)
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
    autosave::request_add();
}

static bool
xset_clipboard_in_set(const xset_t& set)
{ // look upward to see if clipboard is in set's tree
    assert(set != nullptr);

    if (!xset_set_clipboard || set->lock)
    {
        return false;
    }
    if (set == xset_set_clipboard)
    {
        return true;
    }

    if (set->parent)
    {
        const xset_t set_parent = xset_get(set->parent.value());
        if (xset_clipboard_in_set(set_parent))
        {
            return true;
        }
    }

    if (set->prev)
    {
        xset_t set_prev = xset_get(set->prev.value());
        while (set_prev)
        {
            if (set_prev->parent)
            {
                const xset_t set_prev_parent = xset_get(set_prev->parent.value());
                if (xset_clipboard_in_set(set_prev_parent))
                {
                    return true;
                }
                set_prev = nullptr;
            }
            else if (set_prev->prev)
            {
                set_prev = xset_get(set_prev->prev.value());
            }
            else
            {
                set_prev = nullptr;
            }
        }
    }
    return false;
}

bool
xset_job_is_valid(const xset_t& set, xset::job job)
{
    assert(set != nullptr);

    const bool no_remove = false;
    bool no_paste = false;

    switch (job)
    {
        case xset::job::key:
            return set->menu_style < xset::menu::submenu;
        case xset::job::cut:
            return !set->lock;
        case xset::job::copy:
            return !set->lock;
        case xset::job::paste:
            if (!xset_set_clipboard)
            {
                no_paste = true;
            }
            else if (set == xset_set_clipboard && xset_clipboard_is_cut)
            {
                // do not allow cut paste to self
                no_paste = true;
            }
            else if (xset_set_clipboard->tool > xset::tool::custom && set->tool == xset::tool::NOT)
            {
                // do not allow paste of builtin tool item to menu
                no_paste = true;
            }
            else if (xset_set_clipboard->menu_style == xset::menu::submenu)
            {
                // do not allow paste of submenu to self or below
                no_paste = xset_clipboard_in_set(set);
            }
            return !no_paste;
        case xset::job::remove:
            return (!set->lock && !no_remove);
        case xset::job::add_tool:
        case xset::job::remove_book:
        case xset::job::invalid:
            break;
    }
    return false;
}

static void
on_menu_hide(GtkWidget* widget, GtkWidget* design_menu)
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(design_menu));
}

static GtkWidget*
xset_design_additem(GtkWidget* menu, const std::string_view label, xset::job job, const xset_t& set)
{
    GtkWidget* item = nullptr;
    item = gtk_menu_item_new_with_mnemonic(label.data());

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(magic_enum::enum_integer(job)));
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xset_design_job), set.get());
    return item;
}

GtkWidget*
xset_design_show_menu(GtkWidget* menu, const xset_t& set, const xset_t& book_insert, u32 button,
                      const std::chrono::system_clock::time_point time_point)
{
    (void)button;
    (void)time_point;

    GtkWidget* newitem = nullptr;
    GtkWidget* submenu = nullptr;
    const bool no_remove = false;
    bool no_paste = false;

    // xset_t mset;
    xset_t insert_set;

    // book_insert is a bookmark set to be used for Paste, etc
    insert_set = book_insert ? book_insert : set;
    // to signal this is a bookmark, pass book_insert = set
    const bool show_keys = set->tool == xset::tool::NOT;

    if (!xset_set_clipboard)
    {
        no_paste = true;
    }
    else if (insert_set == xset_set_clipboard && xset_clipboard_is_cut)
    {
        // do not allow cut paste to self
        no_paste = true;
    }
    else if (xset_set_clipboard->tool > xset::tool::custom && insert_set->tool == xset::tool::NOT)
    {
        // do not allow paste of builtin tool item to menu
        no_paste = true;
    }
    else if (xset_set_clipboard->menu_style == xset::menu::submenu)
    {
        // do not allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set(insert_set);
    }

    GtkWidget* design_menu = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    // Cut
    newitem = xset_design_additem(design_menu, "Cu_t", xset::job::cut, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_x,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Copy
    newitem = xset_design_additem(design_menu, "_Copy", xset::job::copy, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_c,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Paste
    newitem = xset_design_additem(design_menu, "_Paste", xset::job::paste, insert_set);
    gtk_widget_set_sensitive(newitem, !no_paste);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_v,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Remove
    newitem = xset_design_additem(design_menu, "_Remove", xset::job::remove, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !no_remove);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Delete,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Add >
    if (insert_set->tool != xset::tool::NOT)
    {
        // "Add" submenu for builtin tool items
        newitem = gtk_menu_item_new_with_mnemonic("_Add");
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
        gtk_container_add(GTK_CONTAINER(design_menu), newitem);

        for (const auto& it : xset_toolbar_builtin_tools())
        {
            const auto name = it.second.name;
            if (name)
            {
                newitem =
                    xset_design_additem(submenu, name.value(), xset::job::add_tool, insert_set);
                g_object_set_data(G_OBJECT(newitem),
                                  "tool_type",
                                  GINT_TO_POINTER(magic_enum::enum_integer(it.first)));
            }
        }
    }

    // Separator
    gtk_container_add(GTK_CONTAINER(design_menu), gtk_separator_menu_item_new());

    // Key Shorcut
    newitem = xset_design_additem(design_menu, "_Key Shortcut", xset::job::key, set);
    gtk_widget_set_sensitive(newitem, (set->menu_style < xset::menu::submenu));
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_k,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // show menu
    gtk_widget_show_all(GTK_WIDGET(design_menu));
    /* sfm 1.0.6 passing button (3) here when menu == nullptr causes items in New
     * submenu to not activate with some trackpads (eg two-finger right-click)
     * to open original design menu.  Affected only bookmarks pane and toolbar
     * where menu == nullptr.  So pass 0 for button if !menu. */

    // Get the pointer location
    i32 x = 0;
    i32 y = 0;
    GdkModifierType mods;
    gdk_window_get_device_position(gtk_widget_get_window(menu), nullptr, &x, &y, &mods);

    // Popup the menu at the pointer location
    gtk_menu_popup_at_pointer(GTK_MENU(design_menu), nullptr);

    if (menu)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(menu), false);
        g_signal_connect(G_OBJECT(menu), "hide", G_CALLBACK(on_menu_hide), design_menu);
    }
    // clang-format off
    g_signal_connect(G_OBJECT(design_menu), "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    // clang-format on

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(design_menu), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(design_menu), true);

    return design_menu;
}

bool
xset_design_cb(GtkWidget* item, GdkEvent* event, const xset_t& set)
{
    xset::job job = xset::job::invalid;

    GtkWidget* menu = item ? GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu")) : nullptr;
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    const auto button = gdk_button_event_get_button(event);
    const auto type = gdk_event_get_event_type(event);
    const auto time_point = std::chrono::system_clock::from_time_t(gdk_event_get_time(event));

    if (type == GdkEventType::GDK_BUTTON_RELEASE)
    {
        if (button == 1 && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // does not always fire on this event so handle it ourselves
            // see also ptk-file-menu.c on_app_button_press()
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if (menu)
            {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            }
            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    switch (button)
    {
        case 1:
        case 3:
            switch (keymod)
            {
                // left or right click
                case 0:
                    // no modifier
                    if (button == 3)
                    {
                        // right
                        xset_design_show_menu(menu, set, nullptr, button, time_point);
                        return true;
                    }
                    else if (button == 1 && set->tool != xset::tool::NOT && !set->lock)
                    {
                        // activate
                        if (set->tool == xset::tool::custom)
                        {
                            xset_menu_cb(nullptr, set);
                        }
                        else
                        {
                            xset_builtin_tool_activate(set->tool, set, event);
                        }
                        return true;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::copy;
                    break;
#if (GTK_MAJOR_VERSION == 4)
                case GdkModifierType::GDK_ALT_MASK:
#elif (GTK_MAJOR_VERSION == 3)
                case GdkModifierType::GDK_MOD1_MASK:
#endif
                    // alt
                    job = xset::job::cut;
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = xset::job::paste;
                    break;
                default:
                    break;
            }
            break;
        case 2:
            switch (keymod)
            {
                // middle click
                case 0:
                    // no modifier
                    if (set->lock)
                    {
                        xset_design_show_menu(menu, set, nullptr, button, time_point);
                        return true;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::key;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = xset::job::remove;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (job != xset::job::invalid)
    {
        if (xset_job_is_valid(set, job))
        {
            if (menu)
            {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            }
            g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
            xset_design_job(item, set);
        }
        else
        {
            xset_design_show_menu(menu, set, nullptr, button, time_point);
        }
        return true;
    }
    return false; // true will not stop activate on button-press (will on release)
}
