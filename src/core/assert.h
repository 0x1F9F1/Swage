#pragma once

#include <SDL_assert.h>

#ifdef SWAGE_NO_ASSERTIONS
#    define SwAssert(condition)          \
        do                               \
        {                                \
            if (!(condition))            \
            {                            \
                SDL_TriggerBreakpoint(); \
            }                            \
        } while (SDL_NULL_WHILE_LOOP_CONDITION)

#    define SwDebugAssert(condition) SDL_disabled_assert(condition)
#else
#    define SwAssert SDL_enabled_assert
#    define SwDebugAssert SDL_assert
#endif
