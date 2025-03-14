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

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "gui/lib/clipboard.hxx"

#include "logger.hxx"

[[nodiscard]] bool
gui::clipboard::is_valid() noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    const auto formats = clipboard->get_formats();

    return formats->contain_mime_type("x-special/gnome-copied-files");
}

static void
set_clipboard(const std::span<const std::shared_ptr<vfs::file>>& files, bool is_cut) noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    std::string data_gnome = is_cut ? "cut" : "copy";
    std::string data_uri;
    std::string data_text;

    for (const auto& file : files)
    {
        std::string uri = file->uri().data();

        data_gnome += "\n" + uri;
        data_uri += uri + "\n";
        data_text += file->path().string() + "\n";
    }

    // logger::info("gnome \n{}", data_gnome);
    // logger::info("uri   \n{}", data_uri);
    // logger::info("text  \n{}", data_text);

    std::vector<Glib::RefPtr<Gdk::ContentProvider>> providers;

    providers.push_back(
        Gdk::ContentProvider::create("x-special/gnome-copied-files",
                                     Glib::Bytes::create(data_gnome.c_str(), data_gnome.length())));

    providers.push_back(
        Gdk::ContentProvider::create("text/uri-list",
                                     Glib::Bytes::create(data_uri.c_str(), data_uri.length())));

    providers.push_back(
        Gdk::ContentProvider::create("text/plain",
                                     Glib::Bytes::create(data_text.c_str(), data_text.length())));

    clipboard->set_content(Gdk::ContentProvider::create(providers));
}

void
gui::clipboard::copy_files(const std::span<const std::shared_ptr<vfs::file>>& files) noexcept
{
    set_clipboard(files, false);
}

void
gui::clipboard::cut_files(const std::span<const std::shared_ptr<vfs::file>>& files) noexcept
{
    set_clipboard(files, true);
}

void
gui::clipboard::paste_files(
    const std::function<void(const std::vector<std::string>&, bool)>& callback) noexcept
{
    if (!is_valid())
    {
        return;
    }

    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    auto slot = [clipboard, callback](Glib::RefPtr<Gio::AsyncResult>& result)
    {
        try
        {
            Glib::ustring mime_type;
            auto input_stream = clipboard->read_finish(result, mime_type);
            if (!input_stream)
            {
                return;
            }

            auto output_stream = Gio::MemoryOutputStream::create();

            auto slot =
                [output_stream, clipboard, callback](Glib::RefPtr<Gio::AsyncResult>& splice_res)
            {
                (void)clipboard;

                try
                {
                    output_stream->splice_finish(splice_res);

                    std::string content(static_cast<const char*>(output_stream->get_data()),
                                        output_stream->get_data_size());
                    std::stringstream ss(content);

                    std::vector<std::string> paths;
                    bool is_cut = false;
                    bool first_line = true;

                    std::string line;
                    while (std::getline(ss, line))
                    {
                        if (line.empty())
                        {
                            continue;
                        }

                        if (first_line)
                        {
                            is_cut = (line == "cut");
                            first_line = false;
                        }
                        else
                        {
                            paths.push_back(line);
                        }
                    }

                    callback(paths, is_cut);

                    clipboard->unset_content();
                }
                catch (const Glib::Error& ex)
                {
                    logger::error<logger::gui>("clipboard internal error: {}", ex.what());
                }
            };

            output_stream->splice_async(input_stream, slot);
        }
        catch (const Glib::Error& ex)
        {
            logger::warn<logger::gui>("clipboard: {}", ex.what());
        }
    };

    clipboard->read_async({"x-special/gnome-copied-files"}, Glib::PRIORITY_DEFAULT, slot);
}

void
gui::clipboard::set_text(const std::string_view text) noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    clipboard->set_text(text.data());
}
