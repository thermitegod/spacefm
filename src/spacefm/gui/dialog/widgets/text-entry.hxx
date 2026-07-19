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

#pragma once

#include <string>
#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::widget
{
class TextEntry : public Gtk::Box
{
  public:
    TextEntry(const std::string_view label_markup, const std::string_view initial_text = "");
    ~TextEntry() override = default;

    void set_text(const std::string_view text, bool emit_signal = true) noexcept;
    [[nodiscard]] std::string get_text() const noexcept;

    void set_status_text(const std::string_view status) noexcept;
    void clear_status_text() noexcept;

    void select_range(const std::size_t selection_length = std::string::npos) noexcept;

    [[nodiscard]] Gtk::TextView&
    get_text_view() noexcept
    {
        return input_widget_;
    }

  private:
    void on_buffer_changed() noexcept;

    Gtk::Box header_box_;
    Gtk::Label label_primary_;
    Gtk::Label label_status_;

    Gtk::ScrolledWindow scroll_container_;
    Gtk::TextView input_widget_;
    Glib::RefPtr<Gtk::TextBuffer> text_buffer_;

    sigc::connection buffer_connection_;

  public:
    [[nodiscard]] auto
    signal_changed() noexcept
    {
        return signal_changed_;
    }

  private:
    sigc::signal<void()> signal_changed_;
};
} // namespace gui::widget
