#pragma once

#include <mutex>

namespace Swage
{
    using Mutex = std::mutex;

    template <typename T>
    using LockGuard = std::lock_guard<T>;

    template <typename T>
    using UniqueLock = std::unique_lock<T>;
} // namespace Swage
