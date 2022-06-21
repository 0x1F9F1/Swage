#pragma once

#ifdef _MSC_VER
#    include <stdlib.h>
#    pragma intrinsic(_byteswap_ushort)
#    pragma intrinsic(_byteswap_ulong)
#    pragma intrinsic(_byteswap_uint64)
#    pragma intrinsic(memcpy)
#endif

namespace Swage::bits
{
    template <typename T>
    T bswap(T value) noexcept = delete;

    template <typename T>
    u8 bsr(T v) noexcept;

    template <typename T>
    u8 bsr_ceil(T v) noexcept;

    template <typename... Args>
    SW_FORCEINLINE void bswapv(Args&... args) noexcept
    {
        ((args = bswap<Args>(args)), ...);
    }

    template <typename T>
    struct packed
    {
        u8 data[sizeof(T)];

        SW_FORCEINLINE operator T() const noexcept
        {
            T value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }

        SW_FORCEINLINE packed& operator=(const T& value) noexcept
        {
            std::memcpy(data, &value, sizeof(value));
        }
    };

    template <typename T>
    using le = T;

    template <typename T>
    using ple = packed<le<T>>;

    template <typename To, typename From>
    [[nodiscard]] SW_FORCEINLINE To bit_cast(const From& src) noexcept
    {
        static_assert(sizeof(To) == sizeof(From), "sizeof(To) != sizeof(From)");
        static_assert(std::is_trivially_copyable<From>::value, "From is not trivially copyable");
        static_assert(std::is_trivially_copyable<To>::value, "To is not trivially copyable");

        alignas(To) unsigned char dst[sizeof(To)];
        std::memcpy(&dst, &src, sizeof(To));
        return reinterpret_cast<To&>(dst);
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE u8 bswap<u8>(u8 value) noexcept
    {
        return value;
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE i8 bswap<i8>(i8 value) noexcept
    {
        return value;
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE u16 bswap<u16>(u16 value) noexcept
    {
#ifdef _MSC_VER
        return _byteswap_ushort(value);
#else
        return (value >> 8) | (value << 8);
#endif
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE i16 bswap<i16>(i16 value) noexcept
    {
        return static_cast<i16>(bswap<u16>(static_cast<u16>(value)));
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE u32 bswap<u32>(u32 value) noexcept
    {
#ifdef _MSC_VER
        return _byteswap_ulong(value);
#else
        return (value << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | (value >> 24);
#endif
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE i32 bswap<i32>(i32 value) noexcept
    {
        return static_cast<i32>(bswap<u32>(static_cast<u32>(value)));
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE u64 bswap<u64>(u64 value) noexcept
    {
#ifdef _MSC_VER
        return _byteswap_uint64(value);
#else
        return (value >> 56) | ((value & 0x00FF000000000000) << 40) | ((value & 0x0000FF0000000000) << 24) |
            ((value & 0x000000FF00000000) << 8) | ((value & 0x00000000FF000000) >> 8) |
            ((value & 0x0000000000FF0000) >> 24) | ((value & 0x000000000000FF00) >> 40) | (value << 56);
#endif
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE i64 bswap<i64>(i64 value) noexcept
    {
        return static_cast<i64>(bswap<u64>(static_cast<u64>(value)));
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE f32 bswap<f32>(f32 value) noexcept
    {
        return bit_cast<f32>(bswap<u32>(bit_cast<u32>(value)));
    }

    template <>
    [[nodiscard]] SW_FORCEINLINE f64 bswap<f64>(f64 value) noexcept
    {
        return bit_cast<f64>(bswap<u64>(bit_cast<u64>(value)));
    }

#define X(N)   \
    t = n + N; \
    n = (v >> t) ? t : n

    template <>
    [[nodiscard]] inline u8 bsr<u32>(u32 v)
    {
        usize n = 0;
        usize t;

        X(16);
        X(8);
        X(4);
        X(2);
        X(1);

        return static_cast<u8>(n);
    }

    template <>
    [[nodiscard]] inline u8 bsr<u64>(u64 v)
    {
#if SIZE_MAX >> 32
        usize n = 0;
        usize t;

        X(32);
        X(16);
        X(8);
        X(4);
        X(2);
        X(1);

        return static_cast<u8>(n);
#else
        usize n = 0;
        u32 lower = static_cast<u32>(v);

        if (u32 upper = static_cast<u32>(v >> 32))
        {
            lower = upper;
            n = 32;
        }

        return static_cast<u8>(bsr(lower) + n);
#endif
    }

#undef X

    template <typename T>
    [[nodiscard]] inline u8 bsr_ceil(T v) noexcept
    {
        return v ? (bsr<T>(v - 1) + 1) : 0;
    }
} // namespace Swage::bits
