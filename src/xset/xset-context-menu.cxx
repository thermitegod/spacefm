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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "utils/strdup.hxx"

#include "xset/utils/xset-utils.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset.hxx"

#include "gui/file-browser.hxx"

#include "gui/dialog/text.hxx"

#include "vfs/user-dirs.hxx"

#include "autosave.hxx"

void
xset_add_menu(gui::browser* browser, GtkWidget* menu, GtkAccelGroup* accel_group,
              const std::vector<xset::name>& submenu_entries) noexcept
{
    if (submenu_entries.empty())
    {
        return;
    }

    for (const auto submenu_entry : submenu_entries)
    {
        const auto set = xset::set::get(submenu_entry);
        xset_add_menuitem(browser, menu, accel_group, set);
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
        const std::string str = xset::utils::clean_label(label, false);
        item = gtk_menu_item_new_with_label(str.data());
    }
    else
    {
        item = gtk_menu_item_new_with_mnemonic(label.data());
    }
    return item;
}

GtkWidget*
xset_add_menuitem(gui::browser* browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                  const xset_t& set) noexcept
{
    GtkWidget* item = nullptr;
    GtkWidget* submenu = nullptr;
    xset_t keyset;
    xset_t set_next;
    std::string icon_name;

    // logger::info("xset_add_menuitem {}", set->name());

    if (icon_name.empty() && set->icon)
    {
        icon_name = set->icon.value();
    }

    if (icon_name.empty())
    {
        const auto icon_file = vfs::program::config() / "scripts" / set->name() / "icon";
        if (std::filesystem::exists(icon_file))
        {
            icon_name = icon_file;
        }
    }

    if (set->menu.type != xset::set::menu_type::normal)
    {
        xset_t set_radio;
        switch (set->menu.type)
        {
            case xset::set::menu_type::check:
                item = gtk_check_menu_item_new_with_mnemonic(set->menu.label->data());
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                               set->b == xset::set::enabled::yes);
                break;
            case xset::set::menu_type::radio:
                if (set->menu.radio_set)
                {
                    set_radio = set->menu.radio_set;
                }
                else
                {
                    set_radio = set;
                    set_radio->menu.radio_group = nullptr;
                }
                item = gtk_radio_menu_item_new_with_mnemonic((GSList*)set_radio->menu.radio_group,
                                                             set->menu.label->data());
                set_radio->menu.radio_group =
                    gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                               set->b == xset::set::enabled::yes);
                break;
            case xset::set::menu_type::submenu:
                submenu = gtk_menu_new();
                item = xset_new_menuitem(set->menu.label.value_or(""), icon_name);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
                xset_add_menu(browser, submenu, accel_group, set->context_menu_entries);
                break;
            case xset::set::menu_type::sep:
                item = gtk_separator_menu_item_new();
                break;
            case xset::set::menu_type::normal:
            case xset::set::menu_type::string:
            case xset::set::menu_type::reserved_00:
            case xset::set::menu_type::reserved_01:
            case xset::set::menu_type::reserved_02:
            case xset::set::menu_type::reserved_03:
            case xset::set::menu_type::reserved_04:
            case xset::set::menu_type::reserved_05:
            case xset::set::menu_type::reserved_06:
            case xset::set::menu_type::reserved_07:
            case xset::set::menu_type::reserved_08:
            case xset::set::menu_type::reserved_09:
            case xset::set::menu_type::reserved_10:
            case xset::set::menu_type::reserved_11:
            case xset::set::menu_type::reserved_12:
                break;
        }
    }
    if (!item)
    {
        // get menu icon size
        i32 icon_w = 0;
        i32 icon_h = 0;
        gtk_icon_size_lookup(GtkIconSize::GTK_ICON_SIZE_MENU, icon_w.unwrap(), icon_h.unwrap());
        item = xset_new_menuitem(set->menu.label.value_or(""), icon_name);
    }

    set->browser = browser;
    g_object_set_data(G_OBJECT(item), "menu", menu);
    g_object_set_data_full(G_OBJECT(item), "set", ::utils::strdup(set->name()), std::free);

    if (set->menu.obj.key)
    {
        g_object_set_data(G_OBJECT(item), set->menu.obj.key, set->menu.obj.data);
    }

    if (set->menu.type < xset::set::menu_type::submenu)
    {
        // activate callback
        if (!set->callback.func || set->menu.type != xset::set::menu_type::normal)
        {
            // use xset menu callback
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(xset_menu_cb), set.get());
        }
        else if (set->callback.func)
        {
            // use custom callback directly
            g_signal_connect(G_OBJECT(item),
                             "activate",
                             G_CALLBACK(set->callback.func),
                             set->callback.data);
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
            gtk_widget_add_accelerator(item,
                                       "activate",
                                       accel_group,
                                       keyset->keybinding.key.data(),
                                       (GdkModifierType)keyset->keybinding.modifier.data(),
                                       GTK_ACCEL_VISIBLE);
        }
    }

    gtk_widget_set_sensitive(item, !set->disable);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    return item;
}

void
xset_menu_cb(GtkWidget* item, const xset_t& set) noexcept
{
    GFunc cb_func = nullptr;
    void* cb_data = nullptr;

    if (item)
    {
        if (set->menu.type == xset::set::menu_type::radio && GTK_IS_CHECK_MENU_ITEM(item) &&
            !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
        {
            return;
        }

        cb_func = set->callback.func;
        cb_data = set->callback.data;
    }

    switch (set->menu.type)
    {
        case xset::set::menu_type::normal:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::set::menu_type::sep:
            break;
        case xset::set::menu_type::check:
            if (set->b == xset::set::enabled::yes)
            {
                set->b = xset::set::enabled::no;
            }
            else
            {
                set->b = xset::set::enabled::yes;
            }

            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::set::menu_type::string:
        {
            std::string title;
            if (set->title)
            {
                title = set->title.value();
            }
            else
            {
                title = xset::utils::clean_label(set->menu.label.value(), false);
            }

            const auto msg = set->desc.value();
            const auto default_str = set->z.value();

            const auto [response, answer] =
                gui::dialog::text(title, msg, set->s.value(), default_str);
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
        case xset::set::menu_type::radio:
            if (set->b != xset::set::enabled::yes)
            {
                set->b = xset::set::enabled::yes;
            }
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::set::menu_type::reserved_00:
        case xset::set::menu_type::reserved_01:
        case xset::set::menu_type::reserved_02:
        case xset::set::menu_type::reserved_03:
        case xset::set::menu_type::reserved_04:
        case xset::set::menu_type::reserved_05:
        case xset::set::menu_type::reserved_06:
        case xset::set::menu_type::reserved_07:
        case xset::set::menu_type::reserved_08:
        case xset::set::menu_type::reserved_09:
        case xset::set::menu_type::reserved_10:
        case xset::set::menu_type::reserved_11:
        case xset::set::menu_type::reserved_12:
        case xset::set::menu_type::submenu:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
    }

    if (set->menu.type != xset::set::menu_type::normal)
    {
        autosave::request_add();
    }
}
