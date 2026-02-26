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

#include <memory>

#include <gtkmm.h>

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "gui/layout.hxx"
#include "gui/menubar.hxx"
#include "gui/task.hxx"

#include "vfs/bookmarks.hxx"
#include "vfs/task-manager.hxx"

namespace gui
{
class main_window : public Gtk::ApplicationWindow
{
  public:
    main_window(const Glib::RefPtr<Gtk::Application>& app);
    ~main_window();

  private:
    std::shared_ptr<vfs::task_manager> task_manager_ = vfs::task_manager::create();
    std::shared_ptr<config::settings> settings_ = std::make_shared<config::settings>();
    std::shared_ptr<config::manager> config_manager_ = std::make_shared<config::manager>(settings_);
    std::shared_ptr<vfs::bookmarks> bookmark_manager_ = std::make_shared<vfs::bookmarks>();

    Gtk::Box box_;
    gui::menubar menubar_;

    gui::layout layout_ = gui::layout(*this, task_manager_, settings_);

    Gtk::ScrolledWindow task_scroll_;
    gui::task tasks_ = gui::task(*this, task_manager_);

    void add_shortcuts() noexcept;

    void on_open() noexcept;
    void on_open_search() noexcept;
    void on_open_terminal() noexcept;
    void on_open_new_window() noexcept;
    void on_close() noexcept;
    void on_quit() noexcept;
    void on_set_title() noexcept;
    void on_fullscreen() noexcept;
    void on_open_keybindings() noexcept;
    void on_open_preferences() noexcept;
    void on_open_bookmark_manager() noexcept;
    void on_open_about() noexcept;
    void on_open_donate() noexcept;

    void on_new_tab() noexcept;
    void on_new_tab_here() noexcept;
    void on_close_tab() noexcept;

    void on_update_window_title() noexcept;
};
} // namespace gui
