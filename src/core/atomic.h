#pragma once

#include <atomic>

namespace Swage
{
    template <typename T>
    using Atomic = std::atomic<T>;
}
