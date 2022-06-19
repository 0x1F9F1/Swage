#pragma once

#include <SDL_log.h>

namespace Swage
{
    void SwLogMessageV(int category, SDL_LogPriority priority, fmt::string_view format_str, fmt::format_args args);

    [[noreturn]] void SwLogFatalV(
        int category, SDL_LogPriority priority, fmt::string_view format_str, fmt::format_args args);

    template <typename String, typename... Args>
    inline void SwLogMessage(int category, SDL_LogPriority priority, const String& format_str, const Args&... args)
    {
        SwLogMessageV(category, priority, format_str, fmt::make_format_args(args...));
    }

    template <typename String, typename... Args>
    inline void SwLogInfo(const String& format_str, const Args&... args)
    {
        SwLogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format_str, fmt::make_format_args(args...));
    }

    template <typename String, typename... Args>
    inline void SwLogWarning(const String& format_str, const Args&... args)
    {
        SwLogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, format_str, fmt::make_format_args(args...));
    }

    template <typename String, typename... Args>
    inline void SwLogError(const String& format_str, const Args&... args)
    {
        SwLogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, format_str, fmt::make_format_args(args...));
    }

    template <typename String, typename... Args>
    [[noreturn]] inline void SwLogFatal(const String& format_str, const Args&... args)
    {
        SwLogFatalV(
            SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, format_str, fmt::make_format_args(args...));
    }
} // namespace Swage
