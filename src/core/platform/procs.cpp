#include "procs.h"

#include <SDL_loadso.h>

namespace Swage
{
    void* LoadProcs(const char* lib_name, const char* const* proc_names, ProcAddr* procs)
    {
        void* module = SDL_LoadObject(lib_name);

        if (module == nullptr)
            return module;

        for (usize i = 0; proc_names[i]; ++i)
        {
            if (void* proc = SDL_LoadFunction(module, proc_names[i]))
            {
                procs[i] = reinterpret_cast<ProcAddr>(proc);
            }
            else
            {
                for (; i; --i)
                    procs[i - 1] = nullptr;

                SDL_UnloadObject(module);
                module = nullptr;
                break;
            }
        }

        return module;
    }
} // namespace Swage