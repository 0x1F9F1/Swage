#pragma once

#include <unordered_set>

namespace Swage
{
    template <typename Key>
    using HashSet = std::unordered_set<Key>;
} // namespace Swage
