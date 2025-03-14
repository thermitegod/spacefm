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
#include <string_view>

#include <cstdint>

#include <gdkmm.h>
#include <gtkmm.h>

namespace gui::dialog
{
class donate final : public Gtk::Window
{
  public:
    explicit donate(Gtk::ApplicationWindow& parent) noexcept;

  protected:
    Gtk::Box box_;
    Gtk::Notebook notebook_;

    class page : public Gtk::Box
    {
      public:
        page() noexcept;

        void set_image(const std::filesystem::path& path) noexcept;
        void set_label(const std::string_view text) noexcept;

      private:
        Gtk::Picture img_;
        Gtk::Label label_;
    };

    Gtk::Box button_box_;
    Gtk::Button button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;
    void on_button_close_clicked() noexcept;
};
} // namespace gui::dialog
