/*
 *
 * Copyright: See the LICENSE file that came with this program
 *
 */

#include "logger.hxx"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "spacefm.hxx"

namespace SpaceFM
{
    Ref<spdlog::logger> Logger::s_SpaceFMLogger;

    void Logger::Init()
    {
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%=8l%$] [thread %t] %v");
        s_SpaceFMLogger = spdlog::stdout_color_mt("SpaceFM");
        s_SpaceFMLogger->set_level(spdlog::level::trace);
        s_SpaceFMLogger->flush_on(spdlog::level::trace);
    }
} // namespace SpaceFM
