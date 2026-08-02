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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <cstring>

#include <gexiv2/gexiv2.h>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "gui/dialog/media/metadata.hxx"

#include "glycin/glycin.hxx"
#include "logger.hxx"

std::vector<metadata_data>
image_metadata(const std::filesystem::path& path) noexcept
{
    struct image_exif_data
    {
        std::string_view description;
        std::span<const std::string_view> tags;
    };

    using namespace std::string_view_literals;
    // clang-format off
    static constexpr std::array make_tags{"Exif.Image.Make"sv};
    static constexpr std::array model_tags{"Exif.Image.Model"sv, "Exif.Image.UniqueCameraModel"sv};
    static constexpr std::array exp_time_tags{"Exif.Photo.ExposureTime"sv};
    static constexpr std::array exp_prog_tags{"Exif.Photo.ExposureMode"sv};
    static constexpr std::array aperture_tags{"Exif.Photo.ApertureValue"sv};
    static constexpr std::array iso_tags{"Exif.Photo.ISOSpeedRatings"sv, "Xmp.exifEX.ISOSpeed"sv};
    static constexpr std::array flash_tags{"Exif.Photo.Flash"sv};
    static constexpr std::array metering_tags{"Exif.Photo.MeteringMode"sv};
    static constexpr std::array focal_tags{"Exif.Photo.FocalLength"sv};
    static constexpr std::array software_tags{"Exif.Image.Software"sv};
    static constexpr std::array title_tags{"Xmp.dc.title"sv};
    static constexpr std::array desc_tags{"Xmp.dc.description"sv, "Exif.Photo.UserComment"sv};
    static constexpr std::array keywords_tags{"Xmp.dc.subject"sv};
    static constexpr std::array creator_tags{"Xmp.dc.creator"sv, "Exif.Image.Artist"sv};
    static constexpr std::array created_tags{"Exif.Photo.DateTimeOriginal"sv, "Xmp.xmp.CreateDate"sv, "Exif.Image.DateTime"sv};
    static constexpr std::array copyright_tags{"Xmp.dc.rights"sv};
    static constexpr std::array rating_tags{"Xmp.xmp.Rating"sv};
    // clang-format on

    static constexpr std::array<image_exif_data, 17> image_exif_tags{{
        {"Camera Brand", make_tags},
        {"Camera Model", model_tags},
        {"Exposure Time", exp_time_tags},
        {"Exposure Program", exp_prog_tags},
        {"Aperture Value", aperture_tags},
        {"ISO Speed Rating", iso_tags},
        {"Flash Fired", flash_tags},
        {"Metering Mode", metering_tags},
        {"Focal Length", focal_tags},
        {"Software", software_tags},
        {"Title", title_tags},
        {"Description", desc_tags},
        {"Keywords", keywords_tags},
        {"Creator", creator_tags},
        {"Created On", created_tags},
        {"Copyright", copyright_tags},
        {"Rating", rating_tags},
    }};

    std::vector<metadata_data> data;

    Glib::RefPtr<Gly::Image> image;
    try
    {
        auto loader = Gly::Loader::create(Gio::File::create_for_path(path));
        image = loader->load();
    }
    catch (const Glib::Error& ex)
    {
        logger::error<logger::gui>("failed to load image metadata {}", ex.what());
        return data;
    }

    const auto mime_type = image->get_mime_type();
    if (!mime_type.empty())
    {
        data.push_back({"Image Type", mime_type});
    }

    data.push_back({"Width", std::format("{} pixels", image->get_width())});
    data.push_back({"Height", std::format("{} pixels", image->get_height())});

    // Load EXIF/XMP image metadata
    GError* error = nullptr;
    GExiv2Metadata* metadata = gexiv2_metadata_new();
    if (!gexiv2_metadata_open_path(metadata, path.c_str(), &error))
    {
        logger::error<logger::gui>("Error opening metadata: {}", error->message);
        g_error_free(error);
        return data;
    }

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
