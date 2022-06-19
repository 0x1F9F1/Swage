#pragma once

#if defined(__GNUC__) || defined(__clang__)
#    define SW_FORCEINLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#    define SW_FORCEINLINE __pragma(warning(suppress : 4714)) inline __forceinline
#else
#    define SW_FORCEINLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define SW_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#    define SW_NOINLINE __declspec(noinline)
#else
#    define SW_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define SW_LIKELY(x) __builtin_expect(static_cast<bool>(x), 1)
#    define SW_UNLIKELY(x) __builtin_expect(static_cast<bool>(x), 0)
#else
#    define SW_LIKELY(x) static_cast<bool>(x)
#    define SW_UNLIKELY(x) static_cast<bool>(x)
#endif

#if defined(__cplusplus) && defined(__has_cpp_attribute)
#    define SW_HAS_ATTRIBUTE(attrib, value) (__has_cpp_attribute(attrib) >= value)
#else
#    define SW_HAS_ATTRIBUTE(attrib, value) (0)
#endif

#if SW_HAS_ATTRIBUTE(likely, 201803L)
#    define SW_ATTR_LIKELY likely
#else
#    define SW_ATTR_LIKELY
#endif

#if SW_HAS_ATTRIBUTE(unlikely, 201803L)
#    define SW_ATTR_UNLIKELY unlikely
#else
#    define SW_ATTR_UNLIKELY
#endif