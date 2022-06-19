#pragma once

#include <tuple>

namespace Swage
{
    template <typename T1, typename T2>
    using Pair = std::pair<T1, T2>;

    template <typename... Types>
    using Tuple = std::tuple<Types...>;

    template <typename T>
    inline void HashCombine(std::size_t& seed, const T& value)
    {
        seed ^= std::hash<T> {}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <typename... T>
    struct HashTuple
    {
        template <std::size_t... Is>
        static inline std::size_t hash_unpack(Tuple<T...> const& tt, std::index_sequence<Is...>)
        {
            std::size_t seed = 0;

            using unpack = char[];

            (void) unpack {(HashCombine(seed, std::get<Is>(tt)), 0)..., 0};

            return seed;
        }

        inline std::size_t operator()(Tuple<T...> const& tt) const
        {
            return hash_unpack(tt, std::index_sequence_for<T...> {});
        }
    };
} // namespace Swage
