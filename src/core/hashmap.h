#pragma once

#include <unordered_map>

namespace Swage
{
    template <typename Key, typename Value>
    using HashMap = std::unordered_map<Key, Value>;
} // namespace Swage
