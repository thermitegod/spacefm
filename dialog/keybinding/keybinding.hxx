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

#include <print>
#include <span>
#include <string_view>
#include <vector>

#include <gtkmm.h>

#include "datatypes.hxx"

class KeybindingDialog : public Gtk::ApplicationWindow
{
  public:
    KeybindingDialog(const std::string_view json_data);

  protected:
    class KeybindingPage : public Gtk::Box
    {
      public:
        class ModelColumns : public Glib::Object
        {
          public:
            Glib::PropertyProxy<std::string>
            property_name()
            {
                return this->property_name_.get_proxy();
            }

            Glib::PropertyProxy<std::uint32_t>
            property_key()
            {
                return this->property_key_.get_proxy();
            }

            Glib::PropertyProxy<std::uint32_t>
            property_modifier()
            {
                return this->property_modifier_.get_proxy();
            }

            std::string name_;
            std::string keybinding_;

            static Glib::RefPtr<ModelColumns>
            create(const std::string_view name, const std::string_view keybinding,
                   const std::uint32_t key, const std::uint32_t modifier)
            {
                return Glib::make_refptr_for_instance<ModelColumns>(
                    new ModelColumns(name, keybinding, key, modifier));
            }

          protected:
            ModelColumns(const std::string_view name, const std::string_view keybinding,
                         const std::uint32_t key, const std::uint32_t modifier)
                : Glib::ObjectBase(typeid(ModelColumns)), name_(name), keybinding_(keybinding),
                  property_name_(*this, "name", name.data()), property_key_(*this, "key", key),
                  property_modifier_(*this, "modifier", modifier)
            {
                this->property_key().signal_changed().connect(
                    sigc::mem_fun(*this, &ModelColumns::on_key_changed));

                this->property_modifier().signal_changed().connect(
                    sigc::mem_fun(*this, &ModelColumns::on_modifier_changed));
            }

            Glib::Property<std::string> property_name_;
            Glib::Property<std::uint32_t> property_key_;
            Glib::Property<std::uint32_t> property_modifier_;

            void
            on_key_changed()
            {
                // std::println("on_key_changed()");
            }

            void
            on_modifier_changed()
            {
                // std::println("on_modifier_changed()");
            }
        };

        Gtk::ScrolledWindow scrolled_window_;

        Gtk::ColumnView columnview_;
        Glib::RefPtr<Gio::ListStore<ModelColumns>> liststore_;
        Glib::RefPtr<Gtk::SingleSelection> selection_model_;

        void init(KeybindingDialog* parent,
                  const std::span<const datatype::keybinding::request> keybindings,
                  const std::string_view category);

        void create_model(const std::span<const datatype::keybinding::request> keybindings,
                          const std::string_view category);
        void add_columns();
        void liststore_add_item(const std::string_view name, const std::uint32_t key,
                                const std::uint32_t modifier);

        void on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Align halign);
        void on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
        void on_bind_keybinding(const Glib::RefPtr<Gtk::ListItem>& list_item);

        void on_columnview_row_activate(std::uint32_t pos);
    };

    Gtk::Box box_;

    Gtk::Notebook notebook_;

    Gtk::Label label_navigation_ = Gtk::Label("Navigation");
    KeybindingPage page_navigation_;

    Gtk::Label label_editing_ = Gtk::Label("Editing");
    KeybindingPage page_editing_;

    Gtk::Label label_view_ = Gtk::Label("View");
    KeybindingPage page_view_;

    Gtk::Label label_tabs_ = Gtk::Label("Tabs");
    KeybindingPage page_tabs_;

    Gtk::Label label_general_ = Gtk::Label("General");
    KeybindingPage page_general_;

    Gtk::Label label_opening_ = Gtk::Label("Opening");
    KeybindingPage page_opening_;

    Gtk::Label label_invalid_ = Gtk::Label("Invalid");
    KeybindingPage page_invalid_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_cancel_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();

  private:
    std::vector<datatype::keybinding::request> keybindings_data_;
    std::vector<datatype::keybinding::response> changed_keybindings_data_;
};
