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
#include <string>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/tab/path.hxx"
#include "gui/tab/search.hxx"

namespace gui
{
class toolbar final : public Gtk::Box
{
  public:
    toolbar(const std::shared_ptr<config::settings>& settings);

    void update(const std::filesystem::path& path, bool has_back, bool has_forward,
                bool has_up) noexcept;

    void focus_path() noexcept;
    void focus_search() noexcept;

  private:
    std::shared_ptr<config::settings> settings_;

    Gtk::Button button_back_;
    Gtk::Button button_forward_;
    Gtk::Button button_up_;
    Gtk::Button button_home_;
    Gtk::Button button_refresh_;

    gui::path path_;
    gui::search search_;

  public:
    [[nodiscard]] auto
    signal_navigate_back() noexcept
    {
        return button_back_.signal_clicked();
    }

    [[nodiscard]] auto
    signal_navigate_forward() noexcept
    {
        return button_forward_.signal_clicked();
    }

    [[nodiscard]] auto
    signal_navigate_up() noexcept
    {
        return button_up_.signal_clicked();
    }

    [[nodiscard]] auto
    signal_refresh() noexcept
    {
        return button_refresh_.signal_clicked();
    }

    [[nodiscard]] auto
    signal_chdir() noexcept
    {
        return signal_chdir_;
    }

    [[nodiscard]] auto
    signal_filter() noexcept
    {
        return signal_filter_;
    }

  private:
    sigc::signal<void()> signal_navigate_back_;
    sigc::signal<void()> signal_navigate_forward_;
    sigc::signal<void()> signal_navigate_up_;
    sigc::signal<void()> signal_refresh_;
    sigc::signal<void(std::filesystem::path)> signal_chdir_;
    sigc::signal<void(std::string)> signal_filter_;
};
} // namespace gui
