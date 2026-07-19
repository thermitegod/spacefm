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

#include <gtkmm.h>

#include "gui/lib/clipboard.hxx"

#include "gui/dialog/widgets/copy-button.hxx"

gui::widget::CopyButton::CopyButton()
{
    box_.set_orientation(Gtk::Orientation::HORIZONTAL);
    box_.set_margin(5);

    label_.set_text("Copy");
    icon_.set_from_icon_name("edit-copy-symbolic");

    box_.append(icon_);
    box_.append(label_);

    set_child(box_);

    signal_clicked().connect(sigc::mem_fun(*this, &CopyButton::on_button_clicked));
}

void
gui::widget::CopyButton::set_copy_text(const std::string_view text) noexcept
{
    text_ = text;
}

void
gui::widget::CopyButton::on_button_clicked() noexcept
{
    if (text_.empty())
    {
        return;
    }

    gui::clipboard::set_text(text_);
}
