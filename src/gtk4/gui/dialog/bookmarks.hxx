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

#pragma once

#include <chrono>

#include <gtkmm.h>

#include "settings/settings.hxx"

#include "vfs/bookmarks.hxx"

namespace gui::dialog
{
class bookmarks : public Gtk::ApplicationWindow
{
  public:
    explicit bookmarks(Gtk::ApplicationWindow& parent,
                       const std::shared_ptr<vfs::bookmarks>& bookmarks,
                       const std::shared_ptr<config::settings>& settings) noexcept;

  private:
    class ModelColumns : public Glib::Object
    {
      public:
        std::string name_;
        std::filesystem::path path_;
        std::chrono::system_clock::time_point created_;

        static Glib::RefPtr<ModelColumns>
        create(const std::string_view name, const std::filesystem::path& path,
               const std::chrono::system_clock::time_point created) noexcept
        {
            return Glib::make_refptr_for_instance<ModelColumns>(
                new ModelColumns(name, path, created));
        }

      protected:
        explicit ModelColumns(const std::string_view name, const std::filesystem::path& path,
                              const std::chrono::system_clock::time_point created) noexcept
            : Glib::ObjectBase(typeid(ModelColumns)), name_(name), path_(path), created_(created)
        {
        }
    };

    Gtk::Box box_;

    Gtk::ScrolledWindow scrolled_window_;
    Gtk::ColumnView columnview_;
    Glib::RefPtr<Gio::ListStore<ModelColumns>> liststore_;
    Glib::RefPtr<Gtk::SingleSelection> selection_model_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_close_;
    Gtk::Button button_remove_;
    Gtk::Button button_remove_all_;

    std::shared_ptr<vfs::bookmarks> bookmarks_;
    std::shared_ptr<config::settings> settings_;

    void create_model() noexcept;
    void add_columns() noexcept;
    void liststore_add_item(const std::string_view name, const std::filesystem::path& path,
                            const std::chrono::system_clock::time_point created) noexcept;

    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;
    void on_button_ok_clicked() noexcept;
    void on_button_close_clicked() noexcept;
    void on_button_remove_clicked() noexcept;
    void on_button_remove_all_clicked() noexcept;

    void on_setup_label(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Align halign) noexcept;

    void on_bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept;
    void on_bind_path(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept;
    void on_bind_created(const Glib::RefPtr<Gtk::ListItem>& list_item) noexcept;

  public:
    [[nodiscard]] auto
    signal_confirm() noexcept
    {
        return signal_confirm_;
    }

  private:
    sigc::signal<void(std::filesystem::path)> signal_confirm_;
};
} // namespace gui::dialog
