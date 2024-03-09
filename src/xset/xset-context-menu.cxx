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

#include <filesystem>

#include <vector>

#include <optional>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/utils/xset-utils.hxx"

#include "autosave.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "ptk/ptk-file-browser.hxx"

#if (GTK_MAJOR_VERSION == 4)
void
xset_add_menu(ptk::browser* file_browser, GtkWidget* menu, GtkEventController* accel_group,
              const std::vector<xset::name>& submenu_entries) noexcept
#elif (GTK_MAJOR_VERSION == 3)
void
xset_add_menu(ptk::browser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
              const std::vector<xset::name>& submenu_entries) noexcept
#endif
{
    if (submenu_entries.empty())
    {
        return;
    }

    for (const auto submenu_entry : submenu_entries)
    {
        const xset_t set = xset_get(submenu_entry);
        xset_add_menuitem(file_browser, menu, accel_group, set);
    }
}

static GtkWidget*
xset_new_menuitem(const std::string_view label, const std::string_view icon) noexcept
{
    (void)icon;

    GtkWidget* item = nullptr;

    if (label.contains("\\_"))
    {
        // allow escape of underscore
        const std::string str = xset::utils::clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str.data());
    }
    else
    {
        item = gtk_menu_item_new_with_mnemonic(label.data());
    }
    return item;
}

#if (GTK_MAJOR_VERSION == 4)
GtkWidget*
xset_add_menuitem(ptk::browser* file_browser, GtkWidget* menu, GtkEventController* accel_group,
                  const xset_t& set) noexcept
#elif (GTK_MAJOR_VERSION == 3)
GtkWidget*
xset_add_menuitem(ptk::browser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                  const xset_t& set) noexcept
#endif
{
    GtkWidget* item = nullptr;
    GtkWidget* submenu = nullptr;
    xset_t keyset;
    xset_t set_next;
    std::string icon_name;

    // ztd::logger::info("xset_add_menuitem {}", set->name);

    if (icon_name.empty() && set->icon)
    {
        icon_name = set->icon.value();
    }

    if (icon_name.empty())
    {
        const auto icon_file = vfs::program::config() / "scripts" / set->name / "icon";
        if (std::filesystem::exists(icon_file))
        {
            icon_name = icon_file;
        }
    }

    if (set->tool != xset::tool::NOT && set->menu_style != xset::menu::submenu)
    {
        // item = xset_new_menuitem(set->menu_label, icon_name);
    }
    else if (set->menu_style != xset::menu::normal)
    {
        xset_t set_radio;
        switch (set->menu_style)
        {
            case xset::menu::check:
                if (set->lock || (xset::cmd(xset_get_int(set, xset::var::x)) <= xset::cmd::script))
                {
                    item = gtk_check_menu_item_new_with_mnemonic(set->menu_label.value().data());
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                   set->b == xset::b::xtrue);
                }
                break;
            case xset::menu::radio:
                if (set->ob2_data)
                {
                    set_radio = xset_get(static_cast<const char*>(set->ob2_data));
                }
                else
                {
                    set_radio = set;
                }
                item = gtk_radio_menu_item_new_with_mnemonic((GSList*)set_radio->ob2_data,
                                                             set->menu_label.value().data());
                set_radio->ob2_data =
                    (void*)gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), set->b == xset::b::xtrue);
                break;
            case xset::menu::submenu:
                submenu = gtk_menu_new();
                item = xset_new_menuitem(set->menu_label.value_or(""), icon_name);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
                if (set->lock)
                {
                    if (!set->context_menu_entries.empty())
                    {
                        xset_add_menu(file_browser,
                                      submenu,
                                      accel_group,
                                      set->context_menu_entries);
                    }
                }
                else if (set->child)
                {
                    set_next = xset_get(set->child.value());
                    xset_add_menuitem(file_browser, submenu, accel_group, set_next);
                    GList* l = gtk_container_get_children(GTK_CONTAINER(submenu));
                    if (l)
                    {
                        g_list_free(l);
                    }
                    else
                    {
                        // Nothing was added to the menu (all items likely have
                        // invisible context) so destroy (hide) - issue #215
                        gtk_widget_destroy(item);

                        // next item
                        if (set->next)
                        {
                            set_next = xset_get(set->next.value());
                            xset_add_menuitem(file_browser, menu, accel_group, set_next);
                        }
                        return item;
                    }
                }
                break;
            case xset::menu::sep:
                item = gtk_separator_menu_item_new();
                break;
            case xset::menu::normal:
            case xset::menu::string:
            case xset::menu::reserved_00:
            case xset::menu::reserved_01:
            case xset::menu::reserved_02:
            case xset::menu::reserved_03:
            case xset::menu::reserved_04:
            case xset::menu::reserved_05:
            case xset::menu::reserved_06:
            case xset::menu::reserved_07:
            case xset::menu::reserved_08:
            case xset::menu::reserved_09:
            case xset::menu::reserved_10:
            case xset::menu::reserved_11:
            case xset::menu::reserved_12:
                break;
        }
    }
    if (!item)
    {
        // get menu icon size
        i32 icon_w = 0;
        i32 icon_h = 0;
        gtk_icon_size_lookup(GtkIconSize::GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
        item = xset_new_menuitem(set->menu_label.value_or(""), icon_name);
    }

    set->browser = file_browser;
    g_object_set_data(G_OBJECT(item), "menu", menu);
    g_object_set_data(G_OBJECT(item), "set", set->name.data());

    if (set->ob1)
    {
        g_object_set_data(G_OBJECT(item), set->ob1, set->ob1_data);
    }
    if (set->menu_style != xset::menu::radio && set->ob2)
    {
        g_object_set_data(G_OBJECT(item), set->ob2, set->ob2_data);
    }

    if (set->menu_style < xset::menu::submenu)
    {
        // activate callback
        if (!set->cb_func || set->menu_style != xset::menu::normal)
        {
            // use xset menu callback
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xset_menu_cb), set.get());
        }
        else if (set->cb_func)
        {
            // use custom callback directly
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(set->cb_func), set->cb_data);
        }

        // key accel
        if (set->shared_key)
        {
            keyset = set->shared_key;
        }
        else
        {
            keyset = set;
        }
        if (keyset->keybinding.key > 0 && accel_group)
        {
#if (GTK_MAJOR_VERSION == 4)
            ztd::logger::debug("TODO - PORT - accel_group");
            // gtk_widget_add_controller(item, accel_group);
            // gtk_shortcut_controller_add_shortcut(
            //     GTK_SHORTCUT_CONTROLLER(accel_group),
            //     gtk_shortcut_new(gtk_keyval_trigger_new(keyset->keybinding.key, (GdkModifierType)keyset->keybinding.modifier),
            //                     gtk_callback_action_new(callback_func /* TODO */, nullptr, nullptr)))
#elif (GTK_MAJOR_VERSION == 3)
            gtk_widget_add_accelerator(item,
                                       "activate",
                                       accel_group,
                                       keyset->keybinding.key,
                                       (GdkModifierType)keyset->keybinding.modifier,
                                       GTK_ACCEL_VISIBLE);
#endif
        }
    }

    gtk_widget_set_sensitive(item, !set->disable);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    // next item
    if (set->next)
    {
        set_next = xset_get(set->next.value());
        xset_add_menuitem(file_browser, menu, accel_group, set_next);
    }
    return item;
}

void
xset_menu_cb(GtkWidget* item, const xset_t& set) noexcept
{
    GFunc cb_func = nullptr;
    void* cb_data = nullptr;

    if (item)
    {
        if (set->lock && set->menu_style == xset::menu::radio && GTK_IS_CHECK_MENU_ITEM(item) &&
            !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
        {
            return;
        }

        cb_func = set->cb_func;
        cb_data = set->cb_data;
    }

    GtkWidget* parent = GTK_WIDGET(set->browser);

    switch (set->menu_style)
    {
        case xset::menu::normal:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::menu::sep:
            break;
        case xset::menu::check:
            if (set->b == xset::b::xtrue)
            {
                set->b = xset::b::xfalse;
            }
            else
            {
                set->b = xset::b::xtrue;
            }

            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::menu::string:
        {
            std::string title;
            std::string msg = set->desc.value();
            std::string default_str;
            if (set->title && set->lock)
            {
                title = set->title.value();
            }
            else
            {
                title = xset::utils::clean_label(set->menu_label.value(), false, false);
            }
            if (set->lock)
            {
                default_str = set->z.value();
            }
            else
            {
                msg = ztd::replace(msg, "\\n", "\n");
                msg = ztd::replace(msg, "\\t", "\t");
            }
            const auto [response, answer] =
                xset_text_dialog(parent, title, msg, "", set->s.value(), default_str, false);
            set->s = answer;
            if (response)
            {
                if (cb_func)
                {
                    cb_func(item, cb_data);
                }
            }
        }
        break;
        case xset::menu::radio:
            if (set->b != xset::b::xtrue)
            {
                set->b = xset::b::xtrue;
            }
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::menu::reserved_00:
        case xset::menu::reserved_01:
        case xset::menu::reserved_02:
        case xset::menu::reserved_03:
        case xset::menu::reserved_04:
        case xset::menu::reserved_05:
        case xset::menu::reserved_06:
        case xset::menu::reserved_07:
        case xset::menu::reserved_08:
        case xset::menu::reserved_09:
        case xset::menu::reserved_10:
        case xset::menu::reserved_11:
        case xset::menu::reserved_12:
        case xset::menu::submenu:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
    }

    if (set->menu_style != xset::menu::normal)
    {
        autosave::request_add();
    }
}
