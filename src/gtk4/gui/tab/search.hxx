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

namespace gui
{
class search final : public Gtk::Entry
{
  public:
    search();

  private:
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);

  public:
    [[nodiscard]] auto
    signal_confirm() noexcept
    {
        return signal_confirm_;
    }

  private:
    sigc::signal<void(std::string)> signal_confirm_;
};
} // namespace gui
