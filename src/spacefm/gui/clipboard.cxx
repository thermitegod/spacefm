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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "gui/clipboard.hxx"

#include "glycin/glycin.hxx"
#include "logger.hxx"

gui::clipboard::clipboard_content
gui::clipboard::get_content_type() noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();
    auto formats = clipboard->get_formats();
    if (!formats)
    {
        return clipboard_content::invalid;
    }

    if (formats->contain_mime_type("x-special/gnome-copied-files"))
    {
        return clipboard_content::files;
    }

    if (formats->contain_gtype(Gdk::Texture::get_type()) ||
        formats->contain_mime_type("image/png") || formats->contain_mime_type("image/jpeg") ||
        formats->contain_mime_type("image/tiff"))
    {
        return clipboard_content::image;
    }

    if (formats->contain_mime_type("text/plain;charset=utf-8") ||
        formats->contain_mime_type("text/plain"))
    {
        return clipboard_content::text;
    }

    return clipboard_content::invalid;
}

bool
gui::clipboard::is_valid() noexcept
{
    return get_content_type() != clipboard_content::invalid;
}

static void
set_clipboard(std::span<const std::shared_ptr<vfs::file>> files, bool is_cut) noexcept
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
gui::clipboard::copy_files(std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    set_clipboard(files, false);
}

void
gui::clipboard::cut_files(std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    set_clipboard(files, true);
}

void
gui::clipboard::paste_files(
    std::copyable_function<void(const std::vector<std::string>&, bool) const> callback) noexcept
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
gui::clipboard::set_text(std::string_view text) noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    clipboard->set_text(text.data());
}

std::optional<std::string>
gui::clipboard::get_text() noexcept
{
    // TODO should copy paste_files() api and use a callback

    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    std::optional<std::string> text = std::nullopt;

    auto loop = Glib::MainLoop::create();

    clipboard->read_text_async(
        [clipboard, loop, &text](const Glib::RefPtr<Gio::AsyncResult>& result)
        {
            try
            {
                text = clipboard->read_text_finish(result);
            }
            catch (const Glib::Error& ex)
            {
                text = std::nullopt;
            }

            loop->quit();
        });

    loop->run();

    return text;
}

void
gui::clipboard::set_image(const std::shared_ptr<vfs::file>& file) noexcept
{
    auto get_texture = [](const std::shared_ptr<vfs::file>& f) -> Glib::RefPtr<Gdk::Texture>
    {
        auto file = Gio::File::create_for_path(f->path());

        try
        {
            auto loader = Gly::Loader::create(file);
            auto image = loader->load();
            auto frame = image->next_frame();
            auto texture = frame->get_texture();

            return texture;
        }
        catch (const Glib::Error& e)
        {
            logger::error<logger::vfs>("Loading '{}' failed with: {}", file->get_path(), e.what());
            return nullptr;
        }
    };

    auto texture = get_texture(file);
    if (texture)
    {
        set_image(texture);
    }
}

void
gui::clipboard::set_image(const Glib::RefPtr<Gdk::Texture>& texture) noexcept
{
    if (!texture)
    {
        return;
    }

    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    clipboard->set_texture(texture);
}

void
gui::clipboard::get_image(
    std::copyable_function<void(const Glib::RefPtr<Gdk::Texture>&) const> callback) noexcept
{
    auto display = Gdk::Display::get_default();
    auto clipboard = display->get_clipboard();

    clipboard->read_texture_async(
        [clipboard, callback](const Glib::RefPtr<Gio::AsyncResult>& result)
        {
            try
            {
                auto texture = clipboard->read_texture_finish(result);
                callback(texture);
            }
            catch (const Glib::Error& ex)
            {
                logger::warn<logger::gui>("clipboard get image: {}", ex.what());
                callback(nullptr);
            }
        });
}
