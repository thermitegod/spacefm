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

#include <gtkmm.h>

class FileChooserDialog : public Gtk::ApplicationWindow
{
  public:
    FileChooserDialog(const std::string_view json_data);

  protected:
    void on_file_dialog_open_file(const Glib::RefPtr<Gio::AsyncResult>& result,
                                  const Glib::RefPtr<Gtk::FileDialog>& dialog);
    void on_file_dialog_open_folder(const Glib::RefPtr<Gio::AsyncResult>& result,
                                    const Glib::RefPtr<Gtk::FileDialog>& dialog);
};
