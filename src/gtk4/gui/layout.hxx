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

#include <flat_map>

#include <gtkmm.h>

#include "settings/settings.hxx"

#include "gui/browser.hxx"

#include "vfs/task-manager.hxx"

namespace gui
{
class layout : public Gtk::Paned
{
  public:
    layout(Gtk::ApplicationWindow& parent, const std::shared_ptr<vfs::task_manager>& task_manager,
           const std::shared_ptr<config::settings>& settings);

    [[nodiscard]] bool is_visible(config::panel_id id) const noexcept;

    void set_pane_visible(config::panel_id id, bool visible) noexcept;
    gui::browser* get_browser(config::panel_id id) const noexcept;

  private:
    void create_browser(config::panel_id id) noexcept;
    void destroy_browser(config::panel_id id) noexcept;
    void update_container_visibility() noexcept;

    // Disable updating browsers saved tabs
    void freeze_browsers() noexcept;
    // Enable updating browsers saved tabs
    void unfreeze_browsers() noexcept;

    Gtk::ApplicationWindow& parent_;
    std::shared_ptr<vfs::task_manager> task_manager_;
    std::shared_ptr<config::settings> settings_;

    Gtk::Paned top_;
    Gtk::Paned bottom_;

    std::flat_map<config::panel_id, gui::browser*> browsers_{
        {config::panel_id::panel_1, nullptr},
        {config::panel_id::panel_2, nullptr},
        {config::panel_id::panel_3, nullptr},
        {config::panel_id::panel_4, nullptr},
    };
};
} // namespace gui
