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

#include <format>
#include <string>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/widgets/text-entry.hxx"

gui::widget::TextEntry::TextEntry(const std::string_view label_markup,
                                  const std::string_view initial_text)
{
    set_orientation(Gtk::Orientation::VERTICAL);
    // set_margin(5);

    header_box_.set_orientation(Gtk::Orientation::HORIZONTAL);
    header_box_.set_margin(5);

    label_primary_.set_markup(std::format("{}", label_markup));
    label_primary_.set_halign(Gtk::Align::START);
    label_primary_.set_valign(Gtk::Align::START);
    label_primary_.set_margin(4);
    label_primary_.set_selectable(false);
    label_primary_.set_mnemonic_widget(input_widget_);

    label_status_.set_margin_start(10);
    label_status_.set_selectable(false);
    label_status_.set_halign(Gtk::Align::START);

    header_box_.append(label_primary_);
    header_box_.append(label_status_);

    text_buffer_ = Gtk::TextBuffer::create();
    text_buffer_->set_text(std::format("{}", initial_text));
    input_widget_.set_buffer(text_buffer_);
    input_widget_.set_wrap_mode(Gtk::WrapMode::CHAR);
    input_widget_.set_monospace(true);

    scroll_container_.set_child(input_widget_);
    scroll_container_.set_expand(true);

    append(header_box_);
    append(scroll_container_);

    buffer_connection_ =
        text_buffer_->signal_changed().connect(sigc::mem_fun(*this, &TextEntry::on_buffer_changed));
}

void
gui::widget::TextEntry::select_range(const std::size_t selection_length) noexcept
{
    auto start_iter = text_buffer_->begin();
    auto end_iter = text_buffer_->end();

    if (selection_length != std::string::npos)
    {
        const std::string full_text = text_buffer_->get_text(false);
        if (selection_length < full_text.size())
        {
            end_iter =
                text_buffer_->get_iter_at_offset(static_cast<std::int32_t>(selection_length));
        }
    }

    text_buffer_->select_range(start_iter, end_iter);
    input_widget_.grab_focus();
}

void
gui::widget::TextEntry::set_text(const std::string_view text, bool emit_signal) noexcept
{
    if (!emit_signal)
    {
        buffer_connection_.block();
        text_buffer_->set_text(std::format("{}", text));
        buffer_connection_.unblock();
    }
    else
    {
        text_buffer_->set_text(std::format("{}", text));
    }
}

std::string
gui::widget::TextEntry::get_text() const noexcept
{
    return text_buffer_->get_text();
}

void
gui::widget::TextEntry::set_status_text(const std::string_view status) noexcept
{
    label_status_.set_markup(std::format("{}", status));
}

void
gui::widget::TextEntry::clear_status_text() noexcept
{
    label_status_.set_text("");
}

void
gui::widget::TextEntry::on_buffer_changed() noexcept
{
    signal_changed_.emit();
}
