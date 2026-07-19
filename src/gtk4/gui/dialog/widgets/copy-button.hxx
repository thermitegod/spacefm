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

#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::widget
{
class CopyButton : public Gtk::Button
{
  public:
    CopyButton();

    void set_copy_text(const std::string_view text) noexcept;

  private:
    Gtk::Box box_;
    Gtk::Image icon_;
    Gtk::Label label_;

    std::string text_;

    void on_button_clicked() noexcept;
};
} // namespace gui::widget
