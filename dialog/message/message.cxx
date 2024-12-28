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

#include <string_view>

#include <print>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"

#include "message.hxx"

MessageDialog::MessageDialog(const std::string_view title, const std::string_view message,
                             const std::string_view secondary_message, const bool button_ok,
                             const bool button_cancel, const bool button_close,
                             const bool button_yes_no, const bool button_ok_cancel)
{
    this->set_size_request(200, -1);
    this->set_title("Message Dialog");
    this->set_resizable(false);

    // Content //

    this->vbox_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->vbox_.set_margin_start(5);
    this->vbox_.set_margin_end(5);
    this->vbox_.set_margin_top(5);
    this->vbox_.set_margin_bottom(5);

    this->title_.set_markup(std::format("<big>{}</big>", title));
    this->vbox_.append(this->title_);

    this->message_.set_label(message.data());
    this->message_.set_single_line_mode(false);
    this->vbox_.append(this->message_);

    if (!secondary_message.empty())
    {
        this->secondary_message_.set_label(secondary_message.data());
        this->secondary_message_.set_single_line_mode(false);
        this->vbox_.append(this->secondary_message_);
    }

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &MessageDialog::on_key_press),
                                                 false);
    this->add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Ok", true);
    this->button_cancel_ = Gtk::Button("_Cancel", true);
    this->button_yes_ = Gtk::Button("_Yes", true);
    this->button_no_ = Gtk::Button("_No", true);
    this->button_close_ = Gtk::Button("_Close", true);
    this->button_box_.set_halign(Gtk::Align::END);
    if (button_ok)
    {
        this->button_box_.append(this->button_ok_);
    }
    else if (button_cancel)
    {
        this->button_box_.append(this->button_cancel_);
    }
    else if (button_close)
    {
        this->button_box_.append(this->button_close_);
    }
    else if (button_yes_no)
    {
        this->button_box_.append(this->button_no_);
        this->button_box_.append(this->button_yes_);
    }
    else if (button_ok_cancel)
    {
        this->button_box_.append(this->button_cancel_);
        this->button_box_.append(this->button_ok_);
    }

    this->vbox_.append(this->button_box_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &MessageDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &MessageDialog::on_button_cancel_clicked));
    this->button_yes_.signal_clicked().connect(
        sigc::mem_fun(*this, &MessageDialog::on_button_yes_clicked));
    this->button_no_.signal_clicked().connect(
        sigc::mem_fun(*this, &MessageDialog::on_button_no_clicked));
    this->button_close_.signal_clicked().connect(
        sigc::mem_fun(*this, &MessageDialog::on_button_close_clicked));

    this->set_child(this->vbox_);

    this->set_visible(true);
}

bool
MessageDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_close_clicked();
    }
    return false;
}

void
MessageDialog::on_button_ok_clicked()
{
    const auto buffer = glz::write_json(datatype::message_dialog::response{.result = "Ok"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
MessageDialog::on_button_cancel_clicked()
{
    const auto buffer = glz::write_json(datatype::message_dialog::response{.result = "Cancel"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
MessageDialog::on_button_yes_clicked()
{
    const auto buffer = glz::write_json(datatype::message_dialog::response{.result = "Yes"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}
void
MessageDialog::on_button_no_clicked()
{
    const auto buffer = glz::write_json(datatype::message_dialog::response{.result = "No"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
MessageDialog::on_button_close_clicked()
{
    const auto buffer = glz::write_json(datatype::message_dialog::response{.result = "Close"});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}
