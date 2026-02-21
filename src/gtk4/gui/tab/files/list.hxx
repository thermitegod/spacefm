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
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/tab/files/base.hxx"

namespace gui
{
class list final : public Gtk::ColumnView, public files_base
{
  public:
    list(const config::columns columns, const std::shared_ptr<config::settings>& settings);
    ~list() = default;

    list(const list&) = delete;
    list& operator=(const list&) = delete;
    list(list&&) = delete;
    list& operator=(list&&) = delete;

  private:
    void add_columns() noexcept;

    void on_setup_name(const Glib::RefPtr<Gtk::ListItem>& item, Gtk::Align halign) noexcept;
    void on_setup_label(const Glib::RefPtr<Gtk::ListItem>& item, Gtk::Align halign) noexcept;

    void on_bind_name(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_size(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_bytes(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_type(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_mime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_perm(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_owner(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_group(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_atime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_btime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_ctime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_mtime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;

    void on_unbind_name(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_unbind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;

    void on_background_click(std::int32_t n_press, double x, double y) noexcept;

    Glib::RefPtr<Gdk::ContentProvider> on_drag_prepare(double x, double y) const noexcept;
    bool on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept;
    Gdk::DragAction on_drag_motion(double, double) noexcept;

    void update_column_visibility() noexcept;

    Glib::RefPtr<Gtk::ColumnViewColumn> column_name_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_size_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_bytes_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_type_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_mime_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_perm_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_owner_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_group_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_atime_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_btime_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_ctime_;
    Glib::RefPtr<Gtk::ColumnViewColumn> column_mtime_;
};
} // namespace gui
