#pragma once

#include <map>

namespace Swage
{
    template <typename Key, typename Value>
    using Map = std::map<Key, Value, std::less<>>;
} // namespace Swage
