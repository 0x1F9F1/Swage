#include "swage.h"

#include "asset/assetmanager.h"

#include <SDL.h>

namespace Swage
{
    void MainInit()
    {
        SDL_Init(SDL_INIT_VIDEO);

        {
            SDL_version compiled;
            SDL_version linked;

            SDL_VERSION(&compiled);
            SDL_GetVersion(&linked);

            if (std::memcmp(&linked, &compiled, sizeof(SDL_version)))
            {
                SwLogInfo("SDL2 {}.{}.{} Linked, {}.{}.{} Compiled", linked.major, linked.minor, linked.patch,
                    compiled.major, compiled.minor, compiled.patch);
            }
        }

        SwLogInfo("System Info: {} CPU, {} MB", SDL_GetCPUCount(), SDL_GetSystemRAM());

        AssetManager::Init();
    }

    void MainShutdown()
    {
        AssetManager::Shutdown();

        SDL_Quit();
    }
} // namespace Swage

using namespace Swage;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    MainInit();

    i32 result = -1;

#ifndef _DEBUG
    try
    {
#endif
        Ptr<Application> app = CreateApplication();

        result = app->Init();

        if (result == 0)
        {
            while (app->Update())
                ;

            result = app->Shutdown();
        }
#ifndef _DEBUG
    }
    catch (const std::exception& ex)
    {
        SwLogFatal("Exception '{}' in Run", ex.what());
    }
    catch (...)
    {
        SwLogFatal("Unknown Exception in Run");
    }
#endif

    MainShutdown();

    return result;
}
