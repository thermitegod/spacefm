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
#include <memory>
#include <print>
#include <string>
#include <unordered_map>

#include <gdkmm.h>
#include <glibmm.h>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/user-dirs.hxx"

#include "logger.hxx"

int
main(int argc, char** argv)
{
    // required to get Gdk::Pixbuf working
    // Failed to wrap object of type 'GdkPixbuf'. Hint: this error is commonly caused by failing to call a library init() function.
    auto app = Gtk::Application::create();

    logger::initialize();

    CLI::App cli{"Generate thumbnails for DIR"};

    std::array<std::string, 4> valid_sizes = {
        "normal",
        "large",
        "xlarge",
        "xxlarge",
    };

    std::string size;
    cli.add_option("--size", size, "Set thumbnail size")
        ->required(true)
        ->check(CLI::IsMember(valid_sizes));

    std::filesystem::path path;
    cli.add_option("path", path, "[DIR]")->expected(1);

    CLI11_PARSE(cli, argc, argv);

    if (path.empty() || !std::filesystem::exists(path))
    {
        std::println("Bad path {}", path.string());
        return EXIT_FAILURE;
    }

    auto thumbnail_size = std::invoke(
        [](std::string_view size) noexcept
        {
            if (size == "normal")
            {
                return 128;
            }
            else if (size == "large")
            {
                return 256;
            }
            else if (size == "xlarge")
            {
                return 512;
            }
            else if (size == "xxlarge")
            {
                return 1024;
            }
            std::unreachable();
        },
        size);

    std::vector<std::shared_ptr<vfs::file>> files;
    for (const auto& dfile : std::filesystem::directory_iterator(path))
    {
        files.push_back(vfs::file::create(dfile.path()));
    }

    std::unordered_map<std::string, std::shared_ptr<vfs::thumbnail::thumbnailer>> thumbnailers;

    std::array<std::filesystem::path, 2> dirs;
    dirs[0] = "/usr/share/thumbnailers";
    dirs[1] = vfs::user::data() / "thumbnailers";

    for (const auto& dir : dirs)
    {
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
        {
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".thumbnailer")
            {
                auto thumbnailer = vfs::thumbnail::thumbnailer::create(entry.path());
                if (thumbnailer)
                {
                    for (const auto& mime_type : thumbnailer->mime_types())
                    {
                        // logger::debug<logger::vfs>("Registered thumbnailer for MIME type: {} -> {}", mime_type, thumbnailer->try_exec());
                        thumbnailers[mime_type] = thumbnailer;
                    }
                }
            }
        }
    }

    std::ranges::for_each(files,
                          [&](auto& file)
                          {
                              const auto mime_type = file->mime_type()->type().data();
                              if (!thumbnailers.contains(mime_type))
                              {
                                  // logger::debug<logger::vfs>("No registered thumbnailer for MIME type: {}", mime_type);
                                  return;
                              }
                              auto thumbnailer = thumbnailers[mime_type];

                              file->load_thumbnail(thumbnailer, thumbnail_size);
                          });

    return EXIT_SUCCESS;
}
