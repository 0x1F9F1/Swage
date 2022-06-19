#pragma once

#include <optional>

namespace Swage
{
    template <typename T>
    using Option = std::optional<T>;

    inline constexpr std::nullopt_t None = std::nullopt;
} // namespace Swage
