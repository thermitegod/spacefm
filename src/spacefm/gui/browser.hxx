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

#include "vfs/task-manager.hxx"
#include "vfs/volume-manager.hxx"

namespace gui
{
class browser final : public Gtk::Notebook
{
  public:
    browser(Gtk::ApplicationWindow& parent, const std::uint32_t window_id,
            const config::panel_id panel,
            const std::shared_ptr<vfs::volume_manager>& volume_manager,
            const std::shared_ptr<vfs::task_manager>& task_manager,
            const std::shared_ptr<config::settings>& settings);
    ~browser();

    void new_tab(const std::filesystem::path& path, const bool set_active = false) noexcept;
    void new_tab(const config::tab_state& state, const bool set_active = false) noexcept;
    void new_tab_here() noexcept;
    void close_tab() noexcept;
    void restore_tab() noexcept;
    void open_in_tab(const std::filesystem::path& path, std::int32_t tab) noexcept;

    [[nodiscard]] bool set_active_tab(std::int32_t tab) noexcept;

  private:
    void add_shortcuts() noexcept;
    gui::tab* get_tab(const std::int32_t page) noexcept;
    gui::tab* current_tab() noexcept;
    [[nodiscard]] std::string display_filename(const std::filesystem::path& path) noexcept;

    void save_tab_state() noexcept;

    Gtk::ApplicationWindow& parent_;
    std::uint32_t window_id_;
    config::panel_id panel_id_;
    std::shared_ptr<vfs::volume_manager> volume_manager_;
    std::shared_ptr<vfs::task_manager> task_manager_;
    std::shared_ptr<config::settings> settings_;

    Glib::RefPtr<Gio::SimpleActionGroup> action_group_;

    Glib::RefPtr<Gio::SimpleAction> action_close_;
    Glib::RefPtr<Gio::SimpleAction> action_restore_;
    Glib::RefPtr<Gio::SimpleAction> action_tab_;
    Glib::RefPtr<Gio::SimpleAction> action_tab_here_;

    std::queue<config::tab_state> restore_tabs_;

    bool enable_state_ = false;
    gui::tab* context_menu_tab_ = nullptr;

  public:
    [[nodiscard]] auto
    signal_new_tab_in_panel() noexcept
    {
        return signal_new_tab_in_panel_;
    }

  private:
    sigc::signal<void(config::panel_id, const std::filesystem::path&)> signal_new_tab_in_panel_;

    // Signals we connect to
    sigc::scoped_connection signal_page_added_;
    sigc::scoped_connection signal_page_removed_;
    sigc::scoped_connection signal_page_reordered_;
    sigc::scoped_connection signal_switch_page_;
};
} // namespace gui
