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

#include <cstdint>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/text.hxx"

gui::dialog::text::text(Gtk::ApplicationWindow& parent, const std::string_view title,
                        const std::string_view message, const std::string_view text,
                        const std::string_view default_text)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(600, 400);
    set_resizable(false);

    set_title(title.data());

    default_text_ = default_text;

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    message_label_.set_label(message.data());
    box_.append(message_label_);

    buf_ = Gtk::TextBuffer::create();
    buf_->set_text(text.data());
    input_.set_buffer(buf_);
    input_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    input_.set_monospace(true);
    input_.set_size_request(-1, 300);
    scroll_.set_child(input_);
    scroll_.set_size_request(-1, 300);
    box_.append(scroll_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(*this, &gui::dialog::text::on_key_press),
        false);
    input_.add_controller(key_controller);

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_ok_ = Gtk::Button("_Ok", true);
    button_cancel_ = Gtk::Button("_Close", true);
    button_reset_ = Gtk::Button("_Default", true);
    button_reset_.set_visible(!default_text_.empty());

    button_ok_.signal_clicked().connect([this]() { on_button_ok_clicked(); });
    button_cancel_.signal_clicked().connect([this]() { on_button_cancel_clicked(); });
    button_reset_.signal_clicked().connect([this]() { on_button_reset_clicked(); });

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_reset_);
    button_box_.append(button_cancel_);
    button_box_.append(button_ok_);

    set_child(box_);

    set_visible(true);

    input_.grab_focus();
}

bool
gui::dialog::text::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        on_button_ok_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_cancel_clicked();
    }
    return false;
}

void
gui::dialog::text::on_button_ok_clicked()
{
    const std::string text = buf_->get_text(false);
    if (text.contains('\n') || text.contains("\\n"))
    {
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail("Your input is invalid because it contains linefeeds");
        alert->set_modal(true);
        alert->show();

        return;
    }

    signal_confirm().emit(text_response{.text = buf_->get_text()});

    close();
}

void
gui::dialog::text::on_button_cancel_clicked()
{
    close();
}

void
gui::dialog::text::on_button_reset_clicked()
{
    buf_->set_text(default_text_);
}
