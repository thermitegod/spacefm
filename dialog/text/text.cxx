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

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"
#include "text.hxx"

TextDialog::TextDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::text::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    this->default_text_ = opts.text_default;

    this->set_size_request(600, 400);
    this->set_title(opts.title);
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    this->message_label_.set_label(opts.message);
    this->box_.append(this->message_label_);

    this->buf_ = Gtk::TextBuffer::create();
    this->buf_->set_text(opts.text);
    this->input_.set_buffer(this->buf_);
    this->input_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    this->input_.set_monospace(true);
    this->input_.set_size_request(-1, 300);
    this->scroll_.set_child(this->input_);
    this->scroll_.set_size_request(-1, 300);
    this->box_.append(this->scroll_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &TextDialog::on_key_press),
                                                 false);
    this->input_.add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Ok", true);
    this->button_cancel_ = Gtk::Button("_Close", true);
    this->button_reset_ = Gtk::Button("_Default", true);
    this->button_reset_.set_visible(!this->default_text_.empty());

    this->box_.append(this->button_box_);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_reset_);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_ok_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &TextDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &TextDialog::on_button_cancel_clicked));
    this->button_reset_.signal_clicked().connect(
        sigc::mem_fun(*this, &TextDialog::on_button_reset_clicked));

    this->set_child(this->box_);

    this->set_visible(true);

    // Set default focus to the text input
    this->input_.grab_focus();
}

bool
TextDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        this->on_button_ok_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_cancel_clicked();
    }
    return false;
}

void
TextDialog::on_button_ok_clicked()
{
    const std::string text = this->buf_->get_text(false);
    if (text.contains('\n') || text.contains("\\n"))
    {
        auto dialog = Gtk::AlertDialog::create("Error");
        dialog->set_detail("Your input is invalid because it contains linefeeds");
        dialog->set_modal(true);
        dialog->show();

        return;
    }

    const auto buffer =
        glz::write_json(datatype::text::response{.text = this->buf_->get_text().data()});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
TextDialog::on_button_cancel_clicked()
{
    this->close();
}

void
TextDialog::on_button_reset_clicked()
{
    this->buf_->set_text(this->default_text_);
}
