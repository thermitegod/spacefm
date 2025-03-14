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

#include <string_view>

#include <gtkmm.h>

#include "settings/settings.hxx"

namespace gui::dialog
{
class preferences : public Gtk::ApplicationWindow
{
  public:
    explicit preferences(Gtk::ApplicationWindow& parent,
                         const std::shared_ptr<config::settings>& settings) noexcept;

  private:
    class ListColumns : public Glib::Object
    {
      public:
        std::string entry_;
        std::uint32_t value_;

        static Glib::RefPtr<ListColumns>
        create(const std::string_view entry, const std::uint32_t value) noexcept
        {
            return Glib::make_refptr_for_instance<ListColumns>(new ListColumns(entry, value));
        }

      protected:
        explicit ListColumns(const std::string_view entry, const std::uint32_t value) noexcept
            : entry_(entry), value_(value)
        {
        }
    };

    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;
    void on_button_close_clicked() noexcept;

    void init_general_tab() noexcept;
    void init_display_tab() noexcept;
    void init_advanced_tab() noexcept;

    void setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept;
    void bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept;

    Gtk::Box box_;
    Gtk::Notebook notebook_;

    Gtk::Box button_box_;
    Gtk::Button button_close_;

    std::shared_ptr<config::settings> settings_;
};
} // namespace gui::dialog
