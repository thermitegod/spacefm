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

#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::widget
{
class PasteButton : public Gtk::Button
{
  public:
    PasteButton();

  private:
    Gtk::Box box_;
    Gtk::Image icon_;
    Gtk::Label label_;

    void on_button_clicked() noexcept;

  public:
    [[nodiscard]] auto
    signal_paste_text() noexcept
    {
        return signal_paste_text_;
    }

  private:
    sigc::signal<void(std::string)> signal_paste_text_;
};
} // namespace gui::widget
