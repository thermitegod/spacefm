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

#include <print>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"
#include "keybinding.hxx"
#include "utils.hxx"

static std::uint32_t
get_keymod(Gdk::ModifierType event) noexcept
{
    return std::uint32_t(event & (Gdk::ModifierType::SHIFT_MASK | Gdk::ModifierType::CONTROL_MASK |
                                  Gdk::ModifierType::ALT_MASK | Gdk::ModifierType::SUPER_MASK |
                                  Gdk::ModifierType::HYPER_MASK | Gdk::ModifierType::META_MASK));
}

SetKeyDialog::SetKeyDialog(const std::string_view key_name, const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::keybinding::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    this->keybindings_data_ = opts.data;
    for (const auto& key_data : this->keybindings_data_)
    {
        if (key_data.name == key_name)
        {
            this->keybinding_data_ = key_data;
            break;
        }
    }

    this->set_size_request(300, -1);
    this->set_title("Set Keybindings");
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    this->title_.set_markup(std::format("<big>{}</big>", "Set Key"));
    this->box_.append(this->title_);

    this->message_.set_label(
        std::format("Press your key combination for item '{}' then click Set.\n"
                    "To remove the current key assignment, click Unset.",
                    this->keybinding_data_.label));
    this->message_.set_single_line_mode(false);
    this->box_.append(this->message_);

    this->keybinding_.set_label("");
    this->keybinding_.set_single_line_mode(false);
    this->box_.append(this->keybinding_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &SetKeyDialog::on_key_press),
                                                 false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_set_ = Gtk::Button("_Set", true);
    this->button_set_.set_sensitive(false);
    this->button_unset_ = Gtk::Button("_Unset", true);
    this->button_cancel_ = Gtk::Button("_Cancel", true);
    this->button_box_.set_halign(Gtk::Align::END);

    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_unset_);
    this->button_box_.append(this->button_set_);

    this->box_.append(this->button_box_);

    this->button_set_.signal_clicked().connect(
        sigc::mem_fun(*this, &SetKeyDialog::on_button_set_clicked));
    this->button_unset_.signal_clicked().connect(
        sigc::mem_fun(*this, &SetKeyDialog::on_button_unset_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &SetKeyDialog::on_button_cancel_clicked));

    this->set_child(this->box_);

    this->set_visible(true);
}

bool
SetKeyDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;

    if (!keyval)
    {
        this->button_set_.set_sensitive(false);
        return true;
    }

    this->button_set_.set_sensitive(true);

    const auto keymod = get_keymod(state);
    if (this->result.key != 0 && state == Gdk::ModifierType::NO_MODIFIER_MASK)
    {
        if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
        {
            // user pressed Enter after selecting a key, so click Set
            this->on_button_set_clicked();
            return true;
        }
        else if (keyval == GDK_KEY_Escape && this->result.key == GDK_KEY_Escape)
        {
            // user pressed Escape twice so click Unset
            this->on_button_unset_clicked();
            return true;
        }
    }

    this->result.name = this->keybinding_data_.name;
    this->result.key = 0;
    this->result.modifier = 0;

    if (!this->keybinding_data_.shared_key.empty())
    { // Switch to the shared_key
        for (const auto& data : this->keybindings_data_)
        {
            if (this->keybinding_data_.shared_key == data.name)
            {
                this->result.name = data.name;

                this->keybinding_data_ = data;
                break;
            }
        }
    }

    const auto keyname = ::keyname(keyval, keymod);

    for (const auto& data : this->keybindings_data_)
    {
        if (this->keybinding_data_.name == data.name)
        {
            continue;
        }

        if (data.key > 0 && data.key == keyval && data.modifier == keymod)
        {
            std::string name;
            if (!data.label.empty())
            {
                name = data.label;
            }
            else
            {
                name = "( no name )";
            }

            this->keybinding_.set_label(
                std::format("\t{}\n\tKeycode: {:#x}  Modifier: {:#x}\n\n{} is already assigned to "
                            "'{}'.\n\nPress a different key or click Set to replace the current "
                            "key assignment.",
                            keyname,
                            keyval,
                            keymod,
                            keyname,
                            name));

            this->result.key = keyval;
            this->result.modifier = keymod;

            return true;
        }
    }

    this->keybinding_.set_label(
        std::format("\t{}\n\tKeycode: {:#x}  Modifier: {:#x}", keyname, keyval, keymod));

    this->result.key = keyval;
    this->result.modifier = keymod;

    return true;
}

void
SetKeyDialog::on_button_set_clicked()
{
    const auto buffer = glz::write_json(this->result);
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
SetKeyDialog::on_button_unset_clicked()
{
    this->button_set_.set_sensitive(true);
    this->keybinding_.set_label("");

    this->result.name = this->keybinding_data_.name;
    this->result.key = 0;
    this->result.modifier = 0;

    if (!this->keybinding_data_.shared_key.empty())
    { // Switch to the shared_key
        for (const auto& data : this->keybindings_data_)
        {
            if (this->keybinding_data_.shared_key == data.name)
            {
                this->result.name = data.name;
                break;
            }
        }
    }
}

void
SetKeyDialog::on_button_cancel_clicked()
{
    this->close();
}
