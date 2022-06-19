#pragma once

#include <string>
#include <string_view>

namespace Swage
{
    using String = std::string;
    using StringView = std::string_view;

    String Concat(std::initializer_list<StringView> values);

    bool StartsWith(StringView haystack, StringView needle);
    bool EndsWith(StringView haystack, StringView needle);

    bool StartsWithI(StringView haystack, StringView needle);
    bool EndsWithI(StringView haystack, StringView needle);

    template <typename... Args>
    inline String Concat(const Args&... args)
    {
        return Concat({StringView(args)...});
    }

    inline constexpr char ToLower(char in) noexcept
    {
        return (in >= 'A' && in <= 'Z') ? (in - ('A' - 'a')) : in;
    }

    bool StringICompareEqual(StringView lhs, StringView rhs);
    bool StringICompareLess(StringView lhs, StringView rhs);

    // 'A' to 'Z' -> 'a' to 'z', '\' -> '/'
    extern const u8 NormalizeCaseAndSlash[256];

    bool PathCompareEqual(StringView lhs, StringView rhs);
    bool PathCompareLess(StringView lhs, StringView rhs);

    // '\' -> '/'
    void PathNormalizeSlash(String& path);
    void PathNormalizeSlash(char* path);

    template <typename T>
    StringView SubCString(const T& data, usize start);

    inline void PathNormalizeSlash(String& path)
    {
        for (char& v : path)
        {
            if (v == '\\')
                v = '/';
        }
    }

    inline void PathNormalizeSlash(char* path)
    {
        for (; *path; ++path)
        {
            if (*path == '\\')
                *path = '/';
        }
    }

    template <typename T>
    inline StringView SubCString(const T& data, usize start)
    {
        StringView result = StringView(data.data(), data.size());
        result = result.substr(start);
        return result.substr(0, result.find('\0'));
    }
} // namespace Swage

[[nodiscard]] inline Swage::String operator"" _s(const char* str, std::size_t len)
{
    return Swage::String(str, len);
}

[[nodiscard]] inline constexpr Swage::StringView operator"" _sv(const char* str, std::size_t len)
{
    return Swage::StringView(str, len);
}