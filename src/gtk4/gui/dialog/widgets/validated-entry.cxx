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

#include "gui/dialog/widgets/validated-entry.hxx"

gui::widget::ValidatedEntry::ValidatedEntry()
{
    set_hexpand(true);

    init_style();
}

void
gui::widget::ValidatedEntry::init_style() noexcept
{
    auto css = Gtk::CssProvider::create();
    css->load_from_data(".success-entry text { color: #2ec27e; }"
                        ".success-entry { border-color: #2ec27e; }"
                        ".error-entry text { color: #e01b24; }"
                        ".error-entry { border-color: #e01b24; }");

    get_style_context()->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void
gui::widget::ValidatedEntry::set_success() noexcept
{
    auto style = get_style_context();
    style->remove_class("error-entry");
    style->add_class("success-entry");
}

void
gui::widget::ValidatedEntry::set_error() noexcept
{
    auto style = get_style_context();
    style->remove_class("success-entry");
    style->add_class("error-entry");
}

void
gui::widget::ValidatedEntry::clear_validation() noexcept
{
    auto style = get_style_context();
    style->remove_class("success-entry");
    style->remove_class("error-entry");
}
