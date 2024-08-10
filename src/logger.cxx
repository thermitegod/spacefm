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

#include <unordered_map>

#include <magic_enum.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

void
logger::initialize(const std::unordered_map<std::string, std::string>& options,
                   const std::filesystem::path& logfile) noexcept
{
    struct default_logger_options_data
    {
        spdlog::level::level_enum default_level;
        std::string_view format;
    };
    static constexpr ztd::
        map<logger::domain, default_logger_options_data, magic_enum::enum_count<logger::domain>()>
            default_logger_options{{{
                {logger::domain::basic,
                 {spdlog::level::trace, "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] %v"}},

                {logger::domain::autosave,
                 {spdlog::level::off, "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [autosave] %v"}},

                {logger::domain::signals,
                 {spdlog::level::off, "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [signals] %v"}},

                {logger::domain::ptk,
                 {spdlog::level::trace, "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [ptk] %v"}},

                {logger::domain::vfs,
                 {spdlog::level::trace, "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [thread %t] [vfs] %v"}},
            }}};

    spdlog::sink_ptr file_sink = nullptr;
    if (!logfile.empty())
    {
        file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
    }

    for (const auto domain_name : magic_enum::enum_names<logger::domain>())
    {
        std::vector<spdlog::sink_ptr> sinks;
        if (file_sink)
        {
            sinks.push_back(file_sink);
        }

        const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
        const auto logger =
            std::make_shared<spdlog::logger>(domain_name.data(), sinks.cbegin(), sinks.cend());

        const auto domain_enum = magic_enum::enum_cast<logger::domain>(domain_name).value();

        try
        {
            const auto level = options.at(domain_name.data());
            logger->set_level(magic_enum::enum_cast<spdlog::level::level_enum>(level).value());
        }
        catch (const std::out_of_range& e)
        { // use default loglevel
            logger->set_level(default_logger_options.at(domain_enum).default_level);
        }

        // logger->flush_on(level);
        logger->set_pattern(default_logger_options.at(domain_enum).format.data());

        spdlog::register_logger(logger);
    }
}