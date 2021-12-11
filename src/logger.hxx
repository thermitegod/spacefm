/*
 *
 * Copyright: See the LICENSE file that came with this program
 *
 */

#pragma once

#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#endif

#include <spdlog/spdlog.h>

#include "spacefm.hxx"

namespace SpaceFM
{
    class Logger
    {
      public:
        static void Init();

        inline static Ref<spdlog::logger>&
        SpaceFMLogger()
        {
            return s_SpaceFMLogger;
        }

      private:
        static Ref<spdlog::logger> s_SpaceFMLogger;
    };
} // namespace SpaceFM

#ifndef LOGGER_ENABLE
#define LOGGER_ENABLE
#endif

#ifdef LOGGER_ENABLE

#define LOG_TRACE(...)    ::SpaceFM::Logger::SpaceFMLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::SpaceFM::Logger::SpaceFMLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     ::SpaceFM::Logger::SpaceFMLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::SpaceFM::Logger::SpaceFMLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::SpaceFM::Logger::SpaceFMLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::SpaceFM::Logger::SpaceFMLogger()->critical(__VA_ARGS__)

#else

#define LOG_TRACE(...)
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...)
#define LOG_CRITICAL(...)

#endif
