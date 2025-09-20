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

#include <array>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <vector>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "vfs/user-dirs.hxx"

#include "logger.hxx"

int
main(int argc, char** argv)
{
    // required to get Gdk::Pixbuf working
    // Failed to wrap object of type 'GdkPixbuf'. Hint: this error is commonly caused by failing to call a library init() function.
    auto app = Gtk::Application::create();

    logger::initialize();

    CLI::App cli{"Validate thumbnails in the Thumbnmail Cache"};

    std::array<std::string, 5> valid_sizes = {
        "normal",
        "large",
        "xlarge",
        "xxlarge",
        "all",
    };

    bool dryrun = false;
    cli.add_option("--dryrun", dryrun, "Do not delete invalid thumbnails");

    std::string size;
    cli.add_option("--size", size, "Validate for thumbnail size")
        ->default_val("all")
        ->check(CLI::IsMember(valid_sizes));

    CLI11_PARSE(cli, argc, argv);

    auto cache = vfs::user::thumbnail_cache();

    std::vector<std::filesystem::path> cache_paths;
    if (size == "normal" || size == "all")
    {
        cache_paths.push_back(cache.normal);
    }
    if (size == "large" || size == "all")
    {
        cache_paths.push_back(cache.large);
    }
    if (size == "xlarge" || size == "all")
    {
        cache_paths.push_back(cache.x_large);
    }
    if (size == "xxlarge" || size == "all")
    {
        cache_paths.push_back(cache.xx_large);
    }

    struct stats
    {
        u64 good;
        u64 bad;
    };

    stats total_cache_stats;

    for (const auto& cache_path : cache_paths)
    {
        if (!std::filesystem::exists(cache_path))
        {
            continue;
        }

        stats cache_stats;
        for (const auto& dfile : std::filesystem::directory_iterator(cache_path))
        {
            const auto& path = dfile.path();

            Glib::RefPtr<Gdk::Pixbuf> thumbnail = nullptr;
            try
            {
                thumbnail = Gdk::Pixbuf::create_from_file(path);
            }
            catch (const Glib::Error& e)
            { // broken/corrupt/empty thumbnail
                if (!dryrun)
                {
                    std::filesystem::remove(path);
                }
                cache_stats.bad += 1;

                continue;
            }

            // thumbnail metadata
            std::string metadata_uri;
            const auto metadata_uri_raw = thumbnail->get_option("tEXt::Thumb::URI");
            if (!metadata_uri_raw.empty())
            {
                metadata_uri = metadata_uri_raw.data();
            }

            const auto thumbnail_real_path = metadata_uri.starts_with("file://")
                                                 ? ztd::removeprefix(metadata_uri, "file://")
                                                 : metadata_uri;
            if (!std::filesystem::exists(thumbnail_real_path))
            {
                if (!dryrun)
                {
                    std::filesystem::remove(path);
                }
                cache_stats.bad += 1;
            }
            else
            {
                cache_stats.good += 1;
            }
        }

        std::println("{}\tgood: {}\tbad: {}",
                     cache_path.string(),
                     cache_stats.good,
                     cache_stats.bad);
        total_cache_stats.good += cache_stats.good;
        total_cache_stats.bad += cache_stats.bad;
    }

    std::println("Total\tgood: {}\tbad: {}", total_cache_stats.good, total_cache_stats.bad);
}
