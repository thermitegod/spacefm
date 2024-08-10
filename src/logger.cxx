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

#include <filesystem>

#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "logger.hxx"

void
logger::initialize(const spdlog::level::level_enum level,
                   const std::filesystem::path& logfile) noexcept
{
    spdlog::sink_ptr file_sink = nullptr;
    if (!logfile.empty())
    {
        file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
    }

    {
        std::vector<spdlog::sink_ptr> sinks;
        if (file_sink)
        {
            sinks.push_back(file_sink);
        }
        const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
        const auto logger = std::make_shared<spdlog::logger>("basic", sinks.cbegin(), sinks.cend());
        logger->set_level(level);
        logger->flush_on(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] %v");
        spdlog::register_logger(logger);
    }

    {
        std::vector<spdlog::sink_ptr> sinks;
        if (file_sink)
        {
            sinks.push_back(file_sink);
        }
        const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
        const auto logger = std::make_shared<spdlog::logger>("ptk", sinks.cbegin(), sinks.cend());
        logger->set_level(level);
        logger->flush_on(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [ptk] %v");
        spdlog::register_logger(logger);
    }

    {
        std::vector<spdlog::sink_ptr> sinks;
        if (file_sink)
        {
            sinks.push_back(file_sink);
        }
        const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
        const auto logger = std::make_shared<spdlog::logger>("vfs", sinks.cbegin(), sinks.cend());
        logger->set_level(level);
        logger->flush_on(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [vfs] %v");
        spdlog::register_logger(logger);
    }
}
