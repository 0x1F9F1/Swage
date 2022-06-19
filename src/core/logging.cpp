#include "logging.h"

#include <cstdarg>
#include <cstdlib>

namespace Swage
{
    void SwLogMessageV(int category, SDL_LogPriority priority, fmt::string_view format_str, fmt::format_args args)
    {
        if ((int) priority < 0 || priority >= SDL_NUM_LOG_PRIORITIES)
        {
            return;
        }

        if (priority < SDL_LogGetPriority(category))
        {
            return;
        }

        fmt::basic_memory_buffer<char, SDL_MAX_LOG_MESSAGE> buffer;
        fmt::vformat_to(std::back_inserter(buffer), format_str, args);

        buffer.push_back('\0');

        char* message = buffer.data();
        usize len = buffer.size() - 1;

        /* Chop off final endline. */
        if ((len > 0) && (message[len - 1] == '\n'))
        {
            message[--len] = '\0';
            if ((len > 0) && (message[len - 1] == '\r'))
            { /* catch "\r\n", too. */
                message[--len] = '\0';
            }
        }

        SDL_LogOutputFunction callback;
        void* userdata;
        SDL_LogGetOutputFunction(&callback, &userdata);

        callback(userdata, category, priority, message);
    }

    void SwLogFatalV(int category, SDL_LogPriority priority, fmt::string_view format_str, fmt::format_args args)
    {
        SwLogMessageV(category, priority, format_str, args);

        std::abort();
    }
} // namespace Swage
