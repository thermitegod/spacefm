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

#include "datatypes.hxx"

class PreferenceDialog : public Gtk::ApplicationWindow
{
  public:
    PreferenceDialog(const std::string_view json_data);

  protected:
    class ListColumns : public Glib::Object
    {
      public:
        std::string entry_;
        std::uint32_t value_;

        static Glib::RefPtr<ListColumns>
        create(const std::string_view entry, const std::uint32_t value)
        {
            return Glib::make_refptr_for_instance<ListColumns>(new ListColumns(entry, value));
        }

      protected:
        ListColumns(const std::string_view entry, const std::uint32_t value)
            : entry_(entry), value_(value)
        {
        }
    };

    Gtk::Box vbox_;
    Gtk::Notebook notebook_;

    Gtk::Label total_size_label_;
    Gtk::Label size_on_disk_label_;
    Gtk::Label count_label_;

    Gtk::Box button_box_;
    Gtk::Button button_apply_;
    Gtk::Button button_reset_;
    Gtk::Button button_cancel_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_apply_clicked();
    void on_button_reset_clicked();
    void on_button_cancel_clicked();

    // Settings
    Gtk::CheckButton btn_show_thumbnails_;
    Gtk::CheckButton btn_thumbnail_size_limit_;
    Gtk::SpinButton btn_thumbnail_max_size_;

    Gtk::DropDown icon_size_big_;
    Gtk::DropDown icon_size_small_;
    Gtk::DropDown icon_size_tool_;

    Gtk::CheckButton btn_click_executes_;

    Gtk::CheckButton btn_confirm_;
    Gtk::CheckButton btn_confirm_delete_;
    Gtk::CheckButton btn_confirm_trash_;

    Gtk::CheckButton btn_load_saved_tabs_;

    Gtk::CheckButton btn_always_show_tabs_;
    Gtk::CheckButton btn_show_close_tab_buttons_;
    Gtk::CheckButton btn_new_tab_here_;

    Gtk::CheckButton btn_show_toolbar_home_;
    Gtk::CheckButton btn_show_toolbar_refresh_;
    Gtk::CheckButton btn_show_toolbar_search_;

    Gtk::CheckButton btn_use_si_prefix_;

    // xset settings
    Gtk::DropDown drag_action_;
    Gtk::Entry editor_;
    Gtk::DropDown terminal_;

  private:
    void setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item);

    void init_general_tab() noexcept;
    void init_interface_tab() noexcept;
    void init_advanced_tab() noexcept;

    datatype::settings_extended settings_; // this does not get updated
};
