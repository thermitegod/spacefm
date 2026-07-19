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

#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::widget
{
class ValidatedEntry : public Gtk::Entry
{
  public:
    ValidatedEntry();

    void set_success() noexcept;
    void set_error() noexcept;
    void clear_validation() noexcept;

  private:
    void init_style() noexcept;
};
} // namespace gui::widget
