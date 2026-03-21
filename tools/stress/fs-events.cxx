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

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <cstdint>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

void
worker(std::stop_token stoken, std::uint32_t thread_id, std::uint32_t file_count) noexcept
{
    using namespace std::chrono_literals;

    static thread_local std::mt19937 rng(std::random_device{}());
    // std::uniform_int_distribution<std::chrono::milliseconds::rep> dist(100, 1000);
    std::uniform_int_distribution<std::chrono::milliseconds::rep> dist(1, 50);

    auto sleep = [&]() { std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng))); };

    std::vector<std::filesystem::path> filenames;
    filenames.reserve(file_count);
    for (const auto i : std::views::iota(0u, file_count))
    {
        filenames.emplace_back(std::format("stress_{}-{}.txt", thread_id, i));
    }

    while (!stoken.stop_requested())
    {
        try
        {
            // create events
            for (const auto& filename : filenames)
            {
                if (stoken.stop_requested())
                {
                    break;
                }

                logger::info("Create: {}", filename.string());

                std::ofstream ofs(filename);
                ofs.close();

                sleep();
            }

            // modify events
            for (const auto& filename : filenames)
            {
                if (stoken.stop_requested())
                {
                    break;
                }

                logger::info("Modify: {}", filename.string());

                std::ofstream ofs(filename, std::ios_base::app);
                ofs << "append data data data";
                ofs.close();

                sleep();
            }

            // delete events
            for (const auto& filename : filenames)
            {
                if (stoken.stop_requested())
                {
                    break;
                }

                logger::info("Delete: {}", filename.string());

                std::filesystem::remove(filename);

                sleep();
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            logger::error("{}", e.what());
        }
    }

    for (const auto& filename : filenames)
    {
        std::filesystem::remove(filename);
    }
}

int
main(std::int32_t argc, char** argv)
{
    CLI::App app{"Stress Test FS Events"};

    std::uint32_t thread_count = 0;
    app.add_option("-t,--threads", thread_count, "Number of threads")->default_val(4);

    std::uint32_t minutes = 0;
    app.add_option("-m,--minutes", minutes, "Runtime in minutes")->default_val(1);

    std::uint32_t file_count = 0;
    app.add_option("-f,--files", file_count, "File count")->default_val(20);

    CLI11_PARSE(app, argc, argv);

    logger::initialize();

    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    for (const auto i : std::views::iota(0u, thread_count))
    {
        workers.emplace_back(std::jthread(
            [i, file_count](const std::stop_token& stoken)
            {
                //
                worker(stoken, i, file_count);
            }));
    }

    std::this_thread::sleep_for(std::chrono::minutes(minutes));

    for (auto& t : workers)
    {
        t.request_stop();
    }

    return EXIT_SUCCESS;
}
