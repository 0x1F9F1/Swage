#include "local.h"

#include "../findhandle.h"
#include "../stream.h"

#ifdef _WIN32
#    define SW_USE_NATIVE_IO
#endif

#ifdef SW_USE_NATIVE_IO
#    include "../stream/winapi.h"
#else
#    include "../stream/rwops.h"
#endif

namespace Swage
{
    Rc<Stream> LocalFileDevice::Open(StringView path, bool read_only)
    {
#ifdef SW_USE_NATIVE_IO
        return Win32FileOpen(path, read_only);
#else
        if (path.size() > 255)
            return nullptr;

        char pathz[256];
        std::memcpy(pathz, path.data(), path.size());
        pathz[path.size()] = '\0';

        return RWopsStream::Open(pathz, read_only);
#endif
    }

    Rc<Stream> LocalFileDevice::Create(StringView path, bool write_only, bool truncate)
    {
#ifdef SW_USE_NATIVE_IO
        return Win32FileCreate(path, write_only, truncate);
#else
        if (path.size() > 255)
            return nullptr;

        char pathz[256];
        std::memcpy(pathz, path.data(), path.size());
        pathz[path.size()] = '\0';

        return RWopsStream::Create(pathz, write_only, truncate);
#endif
    }

    bool LocalFileDevice::Exists(StringView path)
    {
#ifdef SW_USE_NATIVE_IO
        return Win32FileExists(path);
#else
        return FileDevice::Exists(path);
#endif
    }

    Ptr<FileEnumerator> LocalFileDevice::Enumerate(StringView path)
    {
#ifdef SW_USE_NATIVE_IO
        return Win32FileFind(path);
#else
        return FileDevice::Find(path);
#endif
    }

    LocalFileDevice::LocalFileDevice() = default;

    Rc<LocalFileDevice> LocalFileDevice::instance_ {new LocalFileDevice()};
} // namespace Swage
