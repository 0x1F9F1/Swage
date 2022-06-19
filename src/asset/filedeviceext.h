#pragma once

namespace Swage
{
    struct FileDeviceExtension
    {
    protected:
        StringView Name;
        usize Size {0};

        FileDeviceExtension(StringView name, usize size)
            : Name(name)
            , Size(size)
        {}

    public:
        template <typename T>
        T* As()
        {
            if (Size == sizeof(T) && Name == T::ExtensionName)
                return static_cast<T*>(this);

            return nullptr;
        }
    };

    template <typename T>
    struct FileDeviceExtensionT : FileDeviceExtension
    {
        FileDeviceExtensionT()
            : FileDeviceExtension(T::ExtensionName, sizeof(T))
        {}
    };
} // namespace Swage
