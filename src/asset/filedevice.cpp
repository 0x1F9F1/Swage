#include "filedevice.h"

#include "findhandle.h"
#include "stream.h"

namespace Swage
{
    /*
        int open(const char* pathname, int flags);
        int open(const char* pathname, int flags, mode_t mode);

        int creat(const char* pathname, mode_t mode);

        int openat(int dirfd, const char* pathname, int flags);
        int openat(int dirfd, const char* pathname, int flags, mode_t mode);

        std::FILE* fopen(const char* filename, const char* mode);

        HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
    */

    Rc<Stream> FileDevice::Open(StringView, bool)
    {
        return nullptr;
    }

    Rc<Stream> FileDevice::Create(StringView, bool, bool)
    {
        return nullptr;
    }

    bool FileDevice::Exists(StringView path)
    {
        return Open(path, true) != nullptr;
    }

    Ptr<FileEnumerator> FileDevice::Enumerate([[maybe_unused]] StringView path)
    {
        return nullptr;
    }

    bool FileDevice::Extension([[maybe_unused]] StringView path, [[maybe_unused]] FileDeviceExtension& ext)
    {
        return false;
    }
} // namespace Swage
