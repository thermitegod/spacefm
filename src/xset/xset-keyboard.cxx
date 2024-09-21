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

#include <gtkmm.h>
#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "ptk/utils/ptk-utils.hxx"

#include "xset/xset.hxx"
#include "xset/xset-keyboard.hxx"
#include "xset/utils/xset-utils.hxx"

const std::string
xset_get_keyname(const xset_t& set, u32 key_val, u32 key_mod) noexcept
{
    u32 keyval = 0;
    u32 keymod = 0;
    if (set)
    {
        keyval = set->keybinding.key;
        keymod = set->keybinding.modifier;
    }
    else
    {
        keyval = key_val;
        keymod = key_mod;
    }
    if (keyval <= 0)
    {
        return "( none )";
    }

    std::string mod = gdk_keyval_name(keyval);
    if (keymod)
    {
        if (keymod & GdkModifierType::GDK_SUPER_MASK)
        {
            mod = std::format("Super+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_HYPER_MASK)
        {
            mod = std::format("Hyper+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_META_MASK)
        {
            mod = std::format("Meta+{}", mod);
        }
#if (GTK_MAJOR_VERSION == 4)
        if (keymod & GdkModifierType::GDK_ALT_MASK)
#elif (GTK_MAJOR_VERSION == 3)
        if (keymod & GdkModifierType::GDK_MOD1_MASK)
#endif
        {
            mod = std::format("Alt+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_CONTROL_MASK)
        {
            mod = std::format("Ctrl+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_SHIFT_MASK)
        {
            mod = std::format("Shift+{}", mod);
        }
    }
    return mod;
}

static bool
on_set_key_keypress(GtkWidget* widget, GdkEvent* event, void* user_data) noexcept
{
    (void)user_data;

    u32* const newkey = (u32*)g_object_get_data(G_OBJECT(widget), "newkey");
    u32* const newkeymod = (u32*)g_object_get_data(G_OBJECT(widget), "newkeymod");
    GtkButton* btn_set = GTK_BUTTON(g_object_get_data(G_OBJECT(widget), "btn_set"));
    GtkButton* btn_unset = GTK_BUTTON(g_object_get_data(G_OBJECT(widget), "btn_unset"));

    const auto set =
        static_cast<xset::set*>(g_object_get_data(G_OBJECT(widget), "set"))->shared_from_this();
    assert(set != nullptr);

    const auto keyval = gdk_key_event_get_keyval(event);

    if (!keyval)
    {
        *newkey = 0;
        *newkeymod = 0;
        gtk_widget_set_sensitive(GTK_WIDGET(btn_set), false);
        return true;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(btn_set), true);

    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));
    if (*newkey != 0 && keymod == 0)
    {
        if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
        {
#if (GTK_MAJOR_VERSION == 4)
            // TODO
            (void)btn_unset;
#elif (GTK_MAJOR_VERSION == 3)
            // user pressed Enter after selecting a key, so click Set
            gtk_button_clicked(btn_set);
#endif
            return true;
        }
        else if (keyval == GDK_KEY_Escape && *newkey == GDK_KEY_Escape)
        {
#if (GTK_MAJOR_VERSION == 4)
            // TODO
            (void)btn_unset;
#elif (GTK_MAJOR_VERSION == 3)
            // user pressed Escape twice so click Unset
            gtk_button_clicked(btn_unset);
#endif
            return true;
        }
    }

    *newkey = 0;
    *newkeymod = 0;

    xset_t keyset = nullptr;
    if (set->shared_key)
    {
        keyset = set->shared_key;
    }

    const std::string keyname = xset_get_keyname(nullptr, keyval, keymod);

    for (const xset_t& set2 : xset::sets())
    {
        assert(set2 != nullptr);

        if (set != set2 && set2->keybinding.key > 0 && set2->keybinding.key == keyval &&
            set2->keybinding.modifier == keymod && set2 != keyset)
        {
            std::string name;
            if (set2->menu.label)
            {
                name = xset::utils::clean_label(set2->menu.label.value(), false, false);
            }
            else
            {
                name = "( no name )";
            }

            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(widget),
                std::format("\t{}\n\tKeycode: {:#x}  Modifier: {:#x}\n\n{} is already assigned to "
                            "'{}'.\n\nPress a different key or click Set to replace the current "
                            "key assignment.",
                            keyname,
                            keyval,
                            keymod,
                            keyname,
                            name)
                    .data(),
                nullptr);

            *newkey = keyval;
            *newkeymod = keymod;

            return true;
        }
    }

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(widget),
        std::format("\t{}\n\tKeycode: {:#x}  Modifier: {:#x}", keyname, keyval, keymod).data(),
        nullptr);
    *newkey = keyval;
    *newkeymod = keymod;

    return true;
}

void
xset_set_key(GtkWidget* parent, const xset_t& set) noexcept
{
    xset_t keyset;

    std::string name;
    if (set->menu.label)
    {
        name = xset::utils::clean_label(set->menu.label.value(), false, true);
    }
    else
    {
        name = "( no name )";
    }

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(parent),
        GtkDialogFlags::GTK_DIALOG_MODAL,
        GtkMessageType::GTK_MESSAGE_QUESTION,
        GtkButtonsType::GTK_BUTTONS_NONE,
        std::format("Press your key combination for item '{}' then click Set. "
                    "To remove the current key assignment, click Unset.",
                    name)
            .data(),
        nullptr);

    GtkButton* btn_cancel = GTK_BUTTON(gtk_button_new_with_label("Cancel"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 GTK_WIDGET(btn_cancel),
                                 GtkResponseType::GTK_RESPONSE_CANCEL);

    GtkButton* btn_unset = GTK_BUTTON(gtk_button_new_with_label("Unset"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 GTK_WIDGET(btn_unset),
                                 GtkResponseType::GTK_RESPONSE_NO);

    GtkButton* btn_set = GTK_BUTTON(gtk_button_new_with_label("Set"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 GTK_WIDGET(btn_set),
                                 GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(GTK_WIDGET(btn_set), false);

    if (set->shared_key)
    {
        keyset = set->shared_key;
    }
    else
    {
        keyset = set;
    }
    if (keyset->keybinding.key <= 0)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(btn_unset), false);
    }

    u32 newkey = 0;
    u32 newkeymod = 0;

    g_object_set_data(G_OBJECT(dialog), "set", set.get());
    g_object_set_data(G_OBJECT(dialog), "newkey", &newkey);
    g_object_set_data(G_OBJECT(dialog), "newkeymod", &newkeymod);
    g_object_set_data(G_OBJECT(dialog), "btn_set", btn_set);
    g_object_set_data(G_OBJECT(dialog), "btn_unset", btn_unset);
    g_signal_connect(G_OBJECT(dialog), "key_press_event", G_CALLBACK(on_set_key_keypress), nullptr);
    gtk_widget_show_all(GTK_WIDGET(dialog));
    gtk_window_set_title(GTK_WINDOW(dialog), "Set Key");

    const auto response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GtkResponseType::GTK_RESPONSE_OK ||
        response == GtkResponseType::GTK_RESPONSE_NO)
    {
        if (response == GtkResponseType::GTK_RESPONSE_OK && (newkey || newkeymod))
        {
            // clear duplicate key assignments
            for (const xset_t& set2 : xset::sets())
            {
                assert(set2 != nullptr);

                if (set2 && set2->keybinding.key > 0 && set2->keybinding.key == newkey &&
                    set2->keybinding.modifier == newkeymod)
                {
                    set2->keybinding.key = 0;
                    set2->keybinding.modifier = 0;
                }
            }
        }
        else if (response == GtkResponseType::GTK_RESPONSE_NO)
        {
            newkey = 0; // unset
            newkeymod = 0;
        }

        // set new key
        if (set->shared_key)
        {
            keyset = set->shared_key;
        }
        else
        {
            keyset = set;
        }
        keyset->keybinding.key = newkey;
        keyset->keybinding.modifier = newkeymod;
    }
}
