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

#include <filesystem>
#include <queue>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include "settings/settings.hxx"

#include "gui/tab/tab.hxx"

namespace gui
{
class browser final : public Gtk::Notebook
{
  public:
    browser(Gtk::ApplicationWindow& parent, config::panel_id panel,
            const std::shared_ptr<config::settings>& settings);
    ~browser();

    void new_tab(const std::filesystem::path& path) noexcept;
    void new_tab(const config::tab_state& state) noexcept;
    void new_tab_here() noexcept;
    void close_tab() noexcept;
    void restore_tab() noexcept;
    void open_in_tab(const std::filesystem::path& path, std::int32_t tab) noexcept;

    [[nodiscard]] bool set_active_tab(std::int32_t tab) noexcept;

    void freeze_state() noexcept;
    void unfreeze_state() noexcept;

  private:
    void add_shortcuts() noexcept;
    gui::tab* current_tab() noexcept;
    [[nodiscard]] std::string display_filename(const std::filesystem::path& path) noexcept;

    void save_tab_state() noexcept;

    Gtk::ApplicationWindow& parent_;
    config::panel_id panel_; // which panel the browser is in
    std::shared_ptr<config::settings> settings_;

    Glib::RefPtr<Gio::SimpleActionGroup> action_group_;

    Glib::RefPtr<Gio::SimpleAction> action_close_;
    Glib::RefPtr<Gio::SimpleAction> action_restore_;
    Glib::RefPtr<Gio::SimpleAction> action_tab_;
    Glib::RefPtr<Gio::SimpleAction> action_tab_here_;

    std::queue<config::tab_state> restore_tabs_;

    bool state_frozen_ = false;

    // Signals we connect to
    sigc::connection signal_page_added_;
    sigc::connection signal_page_removed_;
    sigc::connection signal_page_reordered_;
    sigc::connection signal_switch_page_;
};
} // namespace gui
