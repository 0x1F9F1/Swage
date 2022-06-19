#include "app.h"

namespace Swage
{
    Application::Application()
    {
        SwAssert(instance_ == nullptr);

        instance_ = this;
    }

    Application::~Application()
    {
        SwAssert(instance_ == this);

        instance_ = nullptr;
    }

    Application* Application::instance_ {};
} // namespace Swage
