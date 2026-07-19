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

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/widgets/radio-button-box.hxx"

gui::widget::RadioButtonBox::RadioButtonBox(std::initializer_list<RadioButtonSpec> buttons)
{
    set_orientation(Gtk::Orientation::HORIZONTAL);
    set_spacing(5);
    set_halign(Gtk::Align::START);

    Gtk::CheckButton* first_radio = nullptr;

    for (const auto& [label, action, out_button] : buttons)
    {
        auto* button = Gtk::make_managed<Gtk::CheckButton>(std::string(label), true);
        button->set_focus_on_click(false);

        if (!first_radio)
        {
            first_radio = button;
            button->set_active(true);
        }
        else
        {
            button->set_group(*first_radio);
        }

        if (action)
        {
            button->signal_toggled().connect(action);
        }

        if (out_button != nullptr)
        {
            *out_button = button;
        }

        append(*button);
        buttons_.push_back(button);
    }
}

gui::widget::RadioButtonBox*
gui::widget::RadioButtonBox::create(std::initializer_list<RadioButtonSpec> buttons)
{
    return Gtk::make_managed<RadioButtonBox>(buttons);
}

void
gui::widget::RadioButtonBox::set_button_sensitive(std::string_view label, bool sensitive) noexcept
{
    if (auto* button = get_button(label))
    {
        button->set_sensitive(sensitive);
    }
}

void
gui::widget::RadioButtonBox::set_all_sensitive(bool sensitive) noexcept
{
    for (auto* button : buttons_)
    {
        button->set_sensitive(sensitive);
    }
}

Gtk::CheckButton*
gui::widget::RadioButtonBox::get_button(std::string_view label) noexcept
{
    for (auto* button : buttons_)
    {
        if (button->get_label().data() == label)
        {
            return button;
        }
    }
    return nullptr;
}
