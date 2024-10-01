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

#include <print>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include "error.hxx"

ErrorDialog::ErrorDialog(const std::string_view title, const std::string_view message)
{
    this->set_size_request(200, -1);
    this->set_title("Message Dialog");
    this->set_resizable(false);

    // Error Icon //

    const auto theme = Gtk::IconTheme::create();
    if (theme)
    {
        const std::string icon_name = "dialog-error";

        const auto theme_icon = theme->lookup_icon(icon_name, 64);

        if (theme_icon)
        {
            this->icon_ = Gtk::Image(theme_icon);
            this->icon_.set_margin_end(15);
        }
        else
        {
            std::print("Failed to load the '{}' icon from theme '{}'",
                       icon_name,
                       theme->property_theme_name().get_name());
        }
    }
    else
    {
        std::print("Failed to load a theme");
    }

    // Content //

    this->vbox_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->vbox_.set_margin_start(5);
    this->vbox_.set_margin_end(5);
    this->vbox_.set_margin_top(5);
    this->vbox_.set_margin_bottom(5);

    this->hbox_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 0);
    this->title_.set_markup(std::format("<big>{}</big>", title));
    this->hbox_.append(this->icon_);
    this->hbox_.append(this->title_);
    this->vbox_.append(this->hbox_);

    this->message_.set_label(message.data());
    this->message_.set_single_line_mode(false);
    this->vbox_.append(this->message_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &ErrorDialog::on_key_press),
                                                 false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Ok", true);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_ok_);
    this->vbox_.append(this->button_box_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &ErrorDialog::on_button_ok_clicked));

    this->set_child(this->vbox_);

    this->set_visible(true);
}

bool
ErrorDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        this->close();
    }
    return false;
}

void
ErrorDialog::on_button_ok_clicked()
{
    this->close();
}
