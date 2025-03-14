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

#include <print>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "chooser.hxx"
#include "datatypes.hxx"

FileChooserDialog::FileChooserDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::file_chooser::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    // cursed invisible window
    this->set_size_request(0, 0);
    this->set_resizable(false);
    this->set_visible(false);

    /////////////////////////

    auto dialog = Gtk::FileDialog::create();

    dialog->set_title(opts.title);
    dialog->set_modal(true);

    if (!opts.default_path.empty())
    {
        dialog->set_initial_folder(Gio::File::create_for_path(opts.default_path));
    }
    else
    {
        dialog->set_initial_folder(Gio::File::create_for_path(Glib::get_home_dir()));
    }

    if (opts.mode == datatype::file_chooser::mode::file && !opts.default_file.empty())
    {
        dialog->set_initial_file(Gio::File::create_for_path(opts.default_file));
    }

#if 0
    auto filter_all = Gtk::FileFilter::create();
    filter_all->set_name("All Files");
    filter_all->add_pattern("*");

    auto filter_dir = Gtk::FileFilter::create();
    filter_dir->set_name("Directories");
    filter_dir->add_mime_type("inode/directory");

    auto filters = Gio::ListStore<Gtk::FileFilter>::create();

    filters->append(filter_all);
    filters->append(filter_dir);

    dialog->set_default_filter((opts.mode == datatype::file_chooser::mode::file) ? filter_all
                                                                                 : filter_dir);
    dialog->set_filters(filters);
#endif

    if (opts.mode == datatype::file_chooser::mode::file)
    {
        dialog->open(
            *this,
            sigc::bind(sigc::mem_fun(*this, &FileChooserDialog::on_file_dialog_open_file), dialog));
    }
    else
    {
        dialog->select_folder(
            *this,
            sigc::bind(sigc::mem_fun(*this, &FileChooserDialog::on_file_dialog_open_folder),
                       dialog));
    }
}

void
FileChooserDialog::on_file_dialog_open_file(const Glib::RefPtr<Gio::AsyncResult>& result,
                                            const Glib::RefPtr<Gtk::FileDialog>& dialog)
{
    try
    {
        auto file = dialog->open_finish(result);
        if (!file)
        {
            // return;
            Gtk::Application::get_default()->quit();
        }

        const auto buffer =
            glz::write_json(datatype::file_chooser::response{.path = file->get_path()});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }
    catch (const Glib::Error&)
    {
        Gtk::Application::get_default()->quit();
    }

    // this->close();
    Gtk::Application::get_default()->quit();
}

void
FileChooserDialog::on_file_dialog_open_folder(const Glib::RefPtr<Gio::AsyncResult>& result,
                                              const Glib::RefPtr<Gtk::FileDialog>& dialog)
{
    try
    {
        auto file = dialog->select_folder_finish(result);
        if (!file)
        {
            // return;
            Gtk::Application::get_default()->quit();
        }

        const auto buffer =
            glz::write_json(datatype::file_chooser::response{.path = file->get_path()});
        if (buffer)
        {
            std::println("{}", buffer.value());
        }
    }
    catch (const Glib::Error&)
    {
        Gtk::Application::get_default()->quit();
    }

    // this->close();
    Gtk::Application::get_default()->quit();
}
