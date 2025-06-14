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

#if defined(HAVE_MEDIA)

#include <array>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <cstring>

#include <gexiv2/gexiv2.h>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"
#include "media/metadata.hxx"

std::vector<metadata_data>
image_metadata(const std::filesystem::path& path) noexcept
{
    struct image_exif_data
    {
        std::string_view description;
        std::array<std::string_view, 5> tags;
    };
    static constexpr std::array<image_exif_data, 17> image_exif_tags{{
        {
            "Camera Brand",
            {"Exif.Image.Make"},
        },
        {
            "Camera Model",
            {"Exif.Image.Model", "Exif.Image.UniqueCameraModel"},
        },
        {
            "Exposure Time",
            {"Exif.Photo.ExposureTime"},
        },
        {
            "Exposure Program",
            {"Exif.Photo.ExposureMode"},
        },
        {
            "Aperture Value",
            {"Exif.Photo.ApertureValue"},
        },
        {
            "ISO Speed Rating",
            {"Exif.Photo.ISOSpeedRatings", "Xmp.exifEX.ISOSpeed"},
        },
        {
            "Flash Fired",
            {"Exif.Photo.Flash"},
        },
        {
            "Metering Mode",
            {"Exif.Photo.MeteringMode"},
        },
        {
            "Focal Length",
            {"Exif.Photo.FocalLength"},
        },
        {
            "Software",
            {"Exif.Image.Software"},
        },
        {
            "Title",
            {"Xmp.dc.title"},
        },
        {
            "Description",
            {"Xmp.dc.description", "Exif.Photo.UserComment"},
        },
        {
            "Keywords",
            {"Xmp.dc.subject"},
        },
        {
            "Creator",
            {"Xmp.dc.creator", "Exif.Image.Artist"},
        },
        {
            "Created On",
            {"Exif.Photo.DateTimeOriginal", "Xmp.xmp.CreateDate", "Exif.Image.DateTime"},
        },
        {
            "Copyright",
            {"Xmp.dc.rights"},
        },
        {
            "Rating",
            {"Xmp.xmp.Rating"},
        },
    }};

    std::vector<metadata_data> data;

    GdkPixbufFormat* format = gdk_pixbuf_get_file_info(path.c_str(), nullptr, nullptr);
    g_autofree char* name = gdk_pixbuf_format_get_name(format);
    g_autofree char* desc = gdk_pixbuf_format_get_description(format);

    data.push_back({"Image Type", std::format("{} ({})", name, desc)});

    // Load EXIF/XMP image metadata
    GError* error = nullptr;
    GExiv2Metadata* metadata = gexiv2_metadata_new();
    if (!gexiv2_metadata_open_path(metadata, path.c_str(), &error))
    {
        logger::error<logger::domain::vfs>("Error opening metadata: {}", error->message);
        g_error_free(error);
        return data;
    }

    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);
    if (pixbuf == nullptr)
    {
        logger::error<logger::domain::vfs>("Failed to load image: {}", error->message);
        g_error_free(error);
        return data;
    }
    auto width = gdk_pixbuf_get_width(pixbuf);
    auto height = gdk_pixbuf_get_height(pixbuf);

    GExiv2Orientation orientation = gexiv2_metadata_try_get_orientation(metadata, nullptr);
    if (orientation == GEXIV2_ORIENTATION_ROT_90 || orientation == GEXIV2_ORIENTATION_ROT_270 ||
        orientation == GEXIV2_ORIENTATION_ROT_90_HFLIP ||
        orientation == GEXIV2_ORIENTATION_ROT_90_VFLIP)
    {
        auto temp = width;
        width = height;
        height = temp;
    }

    data.push_back({"Width", std::format("{} pixels", width)});
    data.push_back({"Height", std::format("{} pixels", height)});

    for (const auto& tag_data : image_exif_tags)
    {
        for (const auto tag_name : tag_data.tags)
        {
            if (gexiv2_metadata_try_has_tag(metadata, tag_name.data(), nullptr))
            {
                g_autofree char* tag_value =
                    gexiv2_metadata_try_get_tag_interpreted_string(metadata,
                                                                   tag_name.data(),
                                                                   nullptr);

                /* don't add empty tags - try next one */
                if (tag_value != nullptr && std::strlen(tag_value) > 0)
                {
                    data.push_back({tag_data.description.data(), tag_value});
                    break;
                }
            }
        }
    }

    double longitude = NAN;
    double latitude = NAN;
    double altitude = NAN;
    if (gexiv2_metadata_try_get_gps_info(metadata, &longitude, &latitude, &altitude, nullptr))
    {
        const std::string gps_coords = std::format("{}° {} {}° {} ({:.0f} m)",
                                                   std::fabs(latitude),
                                                   latitude >= 0 ? "N" : "S",
                                                   std::fabs(longitude),
                                                   longitude >= 0 ? "E" : "W",
                                                   altitude);

        data.push_back({"Coordinates", gps_coords});
    }

    g_object_unref(metadata);

    return data;
}

#endif
