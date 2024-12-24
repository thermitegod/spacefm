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

#include "vfs/vfs-mime-type.hxx"

// https://github.com/GNOME/gtkmm/blob/master/demos/gtk-demo/example_listview_applauncher.cc

class AppChooserDialog : public Gtk::Window
{
  public:
    AppChooserDialog(const std::string_view json_data);

  protected:
    class AppPage : public Gtk::ScrolledWindow
    {
      public:
        void init(AppChooserDialog* parent, const std::shared_ptr<vfs::mime_type>& mime_type);
        std::uint32_t position_ = 0; // selected item

      protected:
        Glib::RefPtr<Gio::ListModel>
        create_application_list(const std::shared_ptr<vfs::mime_type>& mime_type);
        void setup_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item);
        void bind_listitem(const Glib::RefPtr<Gtk::ListItem>& list_item);
        void activate(const std::uint32_t position);

        Gtk::ListView* list_;

        AppChooserDialog* parent_;
    };

    Gtk::Box vbox_;

    Gtk::Label title_;

    Gtk::Box file_type_hbox_;
    Gtk::Label file_type_label_;
    Gtk::Label file_type_;

    Gtk::Box entry_hbox_;
    Gtk::Label entry_label_;
    Gtk::Entry entry_;

    Gtk::Notebook notebook_;

    Gtk::CheckButton btn_open_in_terminal_;
    Gtk::CheckButton btn_set_as_default_;

    Gtk::Label label_associated_ = Gtk::Label("Associated Apps");
    AppPage page_associated_;

    Gtk::Label label_all_ = Gtk::Label("All Apps");
    AppPage page_all_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_close_clicked();

  private:
    std::shared_ptr<vfs::mime_type> type_;

    // Concurrency
    // std::shared_ptr<concurrencpp::thread_executor> executor_;
    // concurrencpp::result<concurrencpp::result<void>> executor_result_;
    // concurrencpp::async_condition_variable condition_;
    // concurrencpp::async_lock lock_;
    // bool abort_{false};
};
