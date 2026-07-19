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

#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace gui::widget
{
struct ButtonSpec
{
    std::string label;
    std::function<void()> on_click_action;
    Gtk::Button** out_button = nullptr;
};

class ButtonBox : public Gtk::Box
{
  public:
    explicit ButtonBox(std::initializer_list<ButtonSpec> buttons);
    ~ButtonBox() override = default;

    [[nodiscard]] static ButtonBox* create(std::initializer_list<ButtonSpec> buttons);

    void set_button_sensitive(std::string_view label, bool sensitive) noexcept;
    void set_all_sensitive(bool sensitive) noexcept;

    [[nodiscard]] Gtk::Button* get_button(std::string_view label) noexcept;

  private:
    std::vector<Gtk::Button*> buttons_;
};
} // namespace gui::widget
