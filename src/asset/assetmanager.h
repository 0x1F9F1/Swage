#pragma once

#include "fileapi.h"

namespace Swage
{
    class AssetManager
    {
    public:
        static void Init();

        static void Mount(String prefix, bool read_only, Rc<FileDevice> fs);

        static void Shutdown();

        static StringView BasePath();
        static StringView PrefPath();

        static Rc<Stream> Open(StringView path, bool read_only = true);
        static Rc<Stream> Create(StringView path, bool write_only = true, bool truncate = true);

        static bool Exists(StringView path);
    };
} // namespace Swage
