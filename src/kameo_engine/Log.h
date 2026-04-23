#pragma once
//
// Logging helpers for rekameo. Wraps rex::logging into short macros so
// runtime code can log against a named category.

#include <string>
#include <rex/cvar.h>
#include <rex/logging.h>

namespace rekameo::log {
    inline const rex::LogCategoryId Main = rex::RegisterLogCategory("rekameo");
}

#define REKAMEO_TRACE(...) REXLOG_CAT_TRACE(::rekameo::log::Main, __VA_ARGS__)
#define REKAMEO_DEBUG(...) REXLOG_CAT_DEBUG(::rekameo::log::Main, __VA_ARGS__)
#define REKAMEO_INFO(...)  REXLOG_CAT_INFO (::rekameo::log::Main, __VA_ARGS__)
#define REKAMEO_WARN(...)  REXLOG_CAT_WARN (::rekameo::log::Main, __VA_ARGS__)
#define REKAMEO_ERROR(...) REXLOG_CAT_ERROR(::rekameo::log::Main, __VA_ARGS__)

enum class LogLevel { Trace, Debug, Info, Warn, Error };

inline void Log(LogLevel level, const std::string& message) {
    switch (level) {
        case LogLevel::Trace: REKAMEO_TRACE(message.c_str()); break;
        case LogLevel::Debug: REKAMEO_DEBUG(message.c_str()); break;
        case LogLevel::Info:  REKAMEO_INFO (message.c_str()); break;
        case LogLevel::Warn:  REKAMEO_WARN (message.c_str()); break;
        case LogLevel::Error: REKAMEO_ERROR(message.c_str()); break;
    }
}
