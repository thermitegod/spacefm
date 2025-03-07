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
#include "pattern.hxx"

// stolen from the fnmatch man page
inline constexpr auto FNMATCH_HELP =
    "'?(pattern-list)'\n"
    "The pattern matches if zero or one occurrences of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'*(pattern-list)'\n"
    "The pattern matches if zero or more occurrences of any of the patterns in the "
    "pattern-list "
    "match the input string.\n\n"
    "'+(pattern-list)'\n"
    "The pattern matches if one or more occurrences of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'@(pattern-list)'\n"
    "The pattern matches if exactly one occurrence of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'!(pattern-list)'\n"
    "The pattern matches if the input string cannot be matched with any of the patterns in the "
    "pattern-list.\n";

PatternDialog::PatternDialog()
{
    this->set_size_request(600, 300);
    this->set_title("Select By Pattern");
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    this->expand_.set_label("Show Pattern Matching Help");
    this->expand_.set_expanded(false);
    this->expand_.set_resize_toplevel(false);
    this->expand_data_.set_label(FNMATCH_HELP);
    this->expand_data_.set_single_line_mode(false);
    this->expand_.set_child(this->expand_data_);
    this->box_.append(this->expand_);

    this->buf_ = Gtk::TextBuffer::create();
    this->input_.set_buffer(this->buf_);
    this->input_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    this->input_.set_monospace(true);
    this->input_.set_size_request(-1, 300);
    this->scroll_.set_child(this->input_);
    this->scroll_.set_size_request(-1, 300);
    this->box_.append(this->scroll_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &PatternDialog::on_key_press),
                                                 false);
    this->input_.add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_ok_ = Gtk::Button("_Ok", true);
    this->button_cancel_ = Gtk::Button("_Close", true);

    this->box_.append(this->button_box_);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_ok_);

    this->button_ok_.signal_clicked().connect(
        sigc::mem_fun(*this, &PatternDialog::on_button_ok_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &PatternDialog::on_button_cancel_clicked));

    this->set_child(this->box_);

    this->set_visible(true);

    // Set default focus to the text input
    this->input_.grab_focus();
}

bool
PatternDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
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
PatternDialog::on_button_ok_clicked()
{
    const auto buffer =
        glz::write_json(datatype::pattern::response{.result = this->buf_->get_text().data()});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
PatternDialog::on_button_cancel_clicked()
{
    this->close();
}
