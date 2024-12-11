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

#include <vector>

#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"

// https://github.com/GNOME/gtkmm/blob/master/demos/gtk-demo/example_listview_liststore.cc

class ActionDialog : public Gtk::Window
{
  public:
    ActionDialog(const std::string_view header, const std::string_view json_data);

  protected:
    class ModelColumns : public Glib::Object
    {
      public:
        std::string name_;
        std::string size_;
        bool is_dir_;

        static Glib::RefPtr<ModelColumns>
        create(const std::string_view name, const std::string_view size, const bool is_dir)
        {
            return Glib::make_refptr_for_instance<ModelColumns>(
                new ModelColumns(name, size, is_dir));
        }

      protected:
        ModelColumns(const std::string_view name, const std::string_view size, const bool is_dir)
            : Glib::ObjectBase(typeid(ModelColumns)), name_(name), size_(size), is_dir_(is_dir)
        {
        }
    };

    Gtk::Box vbox_;

    Gtk::ScrolledWindow scrolled_window_;
    Gtk::Label label_;
    Gtk::ColumnView columnview_;
    Glib::RefPtr<Gio::ListStore<ModelColumns>> liststore_;
    Glib::RefPtr<Gtk::SingleSelection> selection_model_;

    Gtk::Label total_size_label_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_cancel_;

    void create_model();
    void add_columns();
    void liststore_add_item(const std::string_view name, const std::uint64_t size,
                            const bool is_dir);

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();

    void on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Align halign);
    void on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void on_bind_size(const Glib::RefPtr<Gtk::ListItem>& list_item);

  private:
    std::vector<datatype::file_action_dialog::request> file_data_;
    std::uint64_t total_size_ = 0;
};
