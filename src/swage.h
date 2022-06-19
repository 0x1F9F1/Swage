#pragma once

#include "app.h"

namespace Swage
{
    class Application;

    Ptr<Application> CreateApplication();
    const char* GetApplicationName();
} // namespace Swage
