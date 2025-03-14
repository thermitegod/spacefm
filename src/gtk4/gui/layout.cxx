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

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/browser.hxx"
#include "gui/layout.hxx"

#include "logger.hxx"

gui::layout::layout(Gtk::ApplicationWindow& parent,
                    const std::shared_ptr<config::settings>& settings)
    : parent_(parent), settings_(settings)
{
    set_orientation(Gtk::Orientation::VERTICAL);
    top_.set_orientation(Gtk::Orientation::HORIZONTAL);
    bottom_.set_orientation(Gtk::Orientation::HORIZONTAL);

    set_expand(true);
    top_.set_expand(true);
    bottom_.set_expand(true);

    set_start_child(top_);
    set_end_child(bottom_);

    set_visible(true);
    top_.set_visible(false);
    bottom_.set_visible(false);
}

bool
gui::layout::is_visible(config::panel_id id) const noexcept
{
    return browsers_.at(id) != nullptr;
}

void
gui::layout::set_pane_visible(config::panel_id id, bool visible) noexcept
{
    if (visible && !browsers_.at(id))
    {
        create_browser(id);
    }
    else if (!visible && browsers_.at(id))
    {
        destroy_browser(id);
    }

    update_container_visibility();
}

void
gui::layout::create_browser(config::panel_id id) noexcept
{
    auto* browser = Gtk::make_managed<gui::browser>(parent_, id, settings_);
    browsers_.at(id) = browser;

    switch (id)
    {
        case config::panel_id::panel_1:
            top_.set_start_child(*browser);
            break;
        case config::panel_id::panel_2:
            top_.set_end_child(*browser);
            break;
        case config::panel_id::panel_3:
            bottom_.set_start_child(*browser);
            break;
        case config::panel_id::panel_4:
            bottom_.set_end_child(*browser);
            break;
    }
}

void
gui::layout::destroy_browser(config::panel_id id) noexcept
{
    switch (id)
    {
        case config::panel_id::panel_1:
            top_.unset_start_child();
            break;
        case config::panel_id::panel_2:
            top_.unset_end_child();
            break;
        case config::panel_id::panel_3:
            bottom_.unset_start_child();
            break;
        case config::panel_id::panel_4:
            bottom_.unset_end_child();
            break;
    }
    browsers_.at(id) = nullptr;
}

gui::browser*
gui::layout::get_browser(config::panel_id id) const noexcept
{
    return browsers_.at(id);
}

void
gui::layout::update_container_visibility() noexcept
{
    const auto top_visible =
        (browsers_.at(config::panel_id::panel_1) || browsers_.at(config::panel_id::panel_2));
    const auto bot_visible =
        (browsers_.at(config::panel_id::panel_3) || browsers_.at(config::panel_id::panel_4));

    logger::debug("top_visible = {} | bot_visible = {}", top_visible, bot_visible);

    top_.set_visible(top_visible);
    bottom_.set_visible(bot_visible);
}
