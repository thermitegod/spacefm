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
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <glycin-gtk4.h>
#include <glycin.h>

namespace Gly
{
class Frame
{
  public:
    explicit Frame(GlyFrame* castitem) : object_(castitem) {}
    ~Frame()
    {
        if (object_)
        {
            g_object_unref(object_);
        }
    }

    [[nodiscard]] Glib::RefPtr<Gdk::Texture>
    get_texture() const noexcept
    {
        GdkTexture* texture = gly_gtk_frame_get_texture(object_);

        return Glib::wrap(texture);
    }

    [[nodiscard]] std::uint32_t
    get_width() const noexcept
    {
        return gly_frame_get_width(object_);
    }

    [[nodiscard]] std::uint32_t
    get_height() const noexcept
    {
        return gly_frame_get_height(object_);
    }

    [[nodiscard]] GlyFrame*
    gobj() const noexcept
    {
        return object_;
    }

  private:
    GlyFrame* object_;
};

class Image
{
  public:
    explicit Image(GlyImage* castitem) : object_(castitem) {}
    ~Image()
    {
        if (object_)
        {
            g_object_unref(object_);
        }
    }

    [[nodiscard]] Glib::RefPtr<Frame>
    next_frame()
    {
        GError* error = nullptr;
        GlyFrame* frame = gly_image_next_frame(object_, &error);
        if (error)
        {
            throw Glib::Error(error);
        }
        return Glib::RefPtr<Frame>(new Frame(frame));
    }

    [[nodiscard]] std::string
    get_mime_type() const noexcept
    {
        return gly_image_get_mime_type(object_);
    }

    [[nodiscard]] std::uint32_t
    get_width() const noexcept
    {
        return gly_image_get_width(object_);
    }

    [[nodiscard]] std::uint32_t
    get_height() const noexcept
    {
        return gly_image_get_height(object_);
    }

    [[nodiscard]] std::vector<std::string>
    get_metadata_keys() const noexcept
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

    [[nodiscard]] std::optional<std::string>
    get_metadata_key_value(const std::string_view key) const noexcept
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

    [[nodiscard]] GlyImage*
    gobj() const noexcept
    {
        return object_;
    }

  private:
    GlyImage* object_;
};

class Loader
{
  public:
    [[nodiscard]] static Glib::RefPtr<Loader>
    create(const Glib::RefPtr<Gio::File>& file) noexcept
    {
        return Glib::RefPtr<Loader>(new Loader(gly_loader_new(file->gobj())));
    }

    ~Loader()
    {
        if (object_)
        {
            g_object_unref(object_);
        }
    }

    [[nodiscard]] Glib::RefPtr<Image>
    load()
    {
        GError* error = nullptr;
        GlyImage* image = gly_loader_load(object_, &error);
        if (error)
        {
            throw Glib::Error(error);
        }
        return Glib::RefPtr<Image>(new Image(image));
    }

    [[nodiscard]] static std::vector<std::string>
    get_mime_types() noexcept
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

  private:
    explicit Loader(GlyLoader* gobj) : object_(gobj) {}
    GlyLoader* object_;
};
} // namespace Gly
