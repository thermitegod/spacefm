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

#include <cstdint>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::dialog
{
struct text_response final
{
    std::string text;
};

class text final : public Gtk::Window
{
  public:
    text(Gtk::ApplicationWindow& parent, const std::string_view title,
         const std::string_view message, const std::string_view text,
         const std::string_view default_text);

  protected:
    std::string default_text_;

    Gtk::Box box_;

    Gtk::Label message_label_;

    Gtk::TextView input_;
    Glib::RefPtr<Gtk::TextBuffer> buf_;
    Gtk::ScrolledWindow scroll_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_cancel_;
    Gtk::Button button_reset_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();
    void on_button_reset_clicked();

  public:
    [[nodiscard]] auto
    signal_confirm() noexcept
    {
        return signal_confirm_;
    }

  private:
    sigc::signal<void(text_response)> signal_confirm_;
};
} // namespace gui::dialog
