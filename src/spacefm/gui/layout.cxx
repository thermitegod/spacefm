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

#include <utility>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/browser.hxx"
#include "gui/layout.hxx"

#include "vfs/task-manager.hxx"

#include "logger.hxx"
#include "reflection/enum.hxx"

gui::layout::layout(Gtk::ApplicationWindow& parent, const std::uint32_t window_id,
                    const std::shared_ptr<vfs::volume_manager>& volume_manager,
                    const std::shared_ptr<vfs::task_manager>& task_manager,
                    const std::shared_ptr<config::settings>& settings)
    : parent_(parent), window_id_(window_id), volume_manager_(volume_manager),
      task_manager_(task_manager), settings_(settings)
{
    logger::debug("gui::layout::layout({})", window_id_);

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
gui::layout::is_visible(config::panel_id panel_id) const noexcept
{
    const auto* const browser = get_browser(panel_id);

    return browser != nullptr;
}

void
gui::layout::set_pane_visible(config::panel_id panel_id, bool visible) noexcept
{
    const auto* const browser = get_browser(panel_id);

    if (visible && !browser)
    {
        create_browser(panel_id);
    }
    else if (!visible && browser)
    {
        destroy_browser(panel_id);
    }

    update_container_visibility();
}

void
gui::layout::create_browser(config::panel_id panel_id) noexcept
{
    auto* browser = Gtk::make_managed<gui::browser>(parent_,
                                                    window_id_,
                                                    panel_id,
                                                    volume_manager_,
                                                    task_manager_,
                                                    settings_);
    browsers_[panel_id] = browser;

    switch (panel_id)
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

    browser->signal_new_tab_in_panel().connect(
        [this](const config::panel_id panel, const std::filesystem::path& path)
        {
            auto* browser = get_browser(panel);
            if (!browser)
            {
                auto alert = Gtk::AlertDialog::create("New Tab in Panel Failed");
                alert->set_detail(
                    std::format("Panel '{}' is not open", reflection::enum_name(panel)));
                alert->set_modal(true);
                alert->show(parent_);
                return;
            }
            browser->new_tab(path, settings_->behavior.switch_to_new_tabs);
        });
    browser->signal_paste_in_panel().connect(
        [this](const config::panel_id panel)
        {
            auto* browser = get_browser(panel);
            if (!browser)
            {
                auto alert = Gtk::AlertDialog::create("Paste in Panel Failed");
                alert->set_detail(
                    std::format("Panel '{}' is not open", reflection::enum_name(panel)));
                alert->set_modal(true);
                alert->show(parent_);
                return;
            }
            browser->current_tab()->on_paste();
        });
}

void
gui::layout::destroy_browser(config::panel_id panel_id) noexcept
{
    switch (panel_id)
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
    browsers_[panel_id] = nullptr;
}

gui::browser*
gui::layout::get_browser(config::panel_id panel_id) const noexcept
{
    return browsers_.at(panel_id);
}

void
gui::layout::update_container_visibility() noexcept
{
    const auto top_visible =
        (browsers_[config::panel_id::panel_1] || browsers_[config::panel_id::panel_2]);
    const auto bot_visible =
        (browsers_[config::panel_id::panel_3] || browsers_[config::panel_id::panel_4]);

    logger::debug("top_visible = {} | bot_visible = {}", top_visible, bot_visible);

    top_.set_visible(top_visible);
    bottom_.set_visible(bot_visible);
}
