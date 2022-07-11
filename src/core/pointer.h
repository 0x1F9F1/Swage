#pragma once

#include <memory>

namespace Swage
{
    template <typename T>
    using Ptr = std::unique_ptr<T>;

    struct PtrMaker
    {
        template <typename T>
        SW_FORCEINLINE Ptr<T> operator->*(T* ptr) const noexcept
        {
            return Ptr<T>(ptr);
        }
    };
} // namespace Swage

#define swnew ::Swage::PtrMaker {}->*new