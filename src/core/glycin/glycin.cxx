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

#include <string>
#include <vector>

#include <cstdint>

#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <glycin-gtk4.h>
#include <glycin.h>

#include "glycin.hxx"

// Frame

Gly::Frame::Frame(GlyFrame* castitem) : object_(castitem) {}

Gly::Frame::~Frame()
{
    if (object_)
    {
        g_object_unref(object_);
    }
}

Glib::RefPtr<Gdk::Texture>
Gly::Frame::get_texture() const noexcept
{
    GdkTexture* texture = gly_gtk_frame_get_texture(object_);

    return Glib::wrap(texture);
}

std::uint32_t
Gly::Frame::get_width() const noexcept
{
    return gly_frame_get_width(object_);
}

std::uint32_t
Gly::Frame::get_height() const noexcept
{
    return gly_frame_get_height(object_);
}

GlyFrame*
Gly::Frame::gobj() const noexcept
{
    return object_;
}

// Image

Gly::Image::Image(GlyImage* castitem) : object_(castitem) {}

Gly::Image::~Image()
{
    if (object_)
    {
        g_object_unref(object_);
    }
}

Glib::RefPtr<Gly::Frame>
Gly::Image::next_frame()
{
    GError* error = nullptr;
    GlyFrame* frame = gly_image_next_frame(object_, &error);
    if (error)
    {
        throw Glib::Error(error);
    }
    return Glib::RefPtr<Frame>(new Frame(frame));
}

std::string
Gly::Image::get_mime_type() const noexcept
{
    return gly_image_get_mime_type(object_);
}

std::uint32_t
Gly::Image::get_width() const noexcept
{
    return gly_image_get_width(object_);
}

std::uint32_t
Gly::Image::get_height() const noexcept
{
    return gly_image_get_height(object_);
}

std::vector<std::string>
Gly::Image::get_metadata_keys() const noexcept
{
    char** keys = gly_image_get_metadata_keys(object_);
    std::vector<std::string> result;
    for (char** it = keys; it && *it; ++it)
    {
        result.push_back(*it);
    }
    g_strfreev(keys);
    return result;
}

std::optional<std::string>
Gly::Image::get_metadata_key_value(std::string_view key) const noexcept
{
    char* val = gly_image_get_metadata_key_value(object_, key.data());
    if (!val)
    {
        return std::nullopt;
    }
    std::string result = val;
    g_free(val);
    return result;
}

GlyImage*
Gly::Image::gobj() const noexcept
{
    return object_;
}

// Loader

Gly::Loader::Loader(GlyLoader* gobj) : object_(gobj) {}

Gly::Loader::~Loader()
{
    if (object_)
    {
        g_object_unref(object_);
    }
}

Glib::RefPtr<Gly::Loader>
Gly::Loader::create(const Glib::RefPtr<Gio::File>& file) noexcept
{
    return Glib::RefPtr<Loader>(new Loader(gly_loader_new(file->gobj())));
}

Glib::RefPtr<Gly::Image>
Gly::Loader::load()
{
    GError* error = nullptr;
    GlyImage* image = gly_loader_load(object_, &error);
    if (error)
    {
        throw Glib::Error(error);
    }
    return Glib::RefPtr<Image>(new Image(image));
}

std::vector<std::string>
Gly::Loader::get_mime_types() noexcept
{
    char** types = gly_loader_get_mime_types();
    std::vector<std::string> result;
    for (char** it = types; it && *it; ++it)
    {
        result.push_back(*it);
    }
    g_strfreev(types);
    return result;
}
