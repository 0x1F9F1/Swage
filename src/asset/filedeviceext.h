#pragma once

namespace Swage
{
    struct ExtensionData
    {
        template <typename T>
        ExtensionData(T* data)
            : ExtensionData(T::ExtensionName, data)
        {}

        template <typename T>
        ExtensionData(StringView name, T* data)
            : ExtensionData(name, data, sizeof(*data))
        {}

        ExtensionData(StringView name, void* data, usize size)
            : Name(name)
            , Data(data)
            , Size(size)
        {}

        template <typename T>
        T* As() const
        {
            return As<T>(T::ExtensionName);
        }

        template <typename T>
        T* As(StringView name) const
        {
            if (name == Name && sizeof(T) == Size)
                return static_cast<T*>(Data);

            return nullptr;
        }

        StringView const Name;
        void* const Data;
        usize const Size;
    };
} // namespace Swage
