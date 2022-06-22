#include "virtual.h"

#include "asset/stream.h"

namespace Swage
{
    Rc<Stream> VirtualFileDevice::Open(StringView path, bool read_only)
    {
        if (read_only)
        {
            if (VFS::File* file = Files.GetFile(path))
                return file->Ops->Open(*file);
        }

        return nullptr;
    }

    bool VirtualFileDevice::Exists(StringView path)
    {
        return Files.Exists(path);
    }

    Ptr<FileEnumerator> VirtualFileDevice::Enumerate(StringView path)
    {
        return Files.Enumerate(path);
    }

    bool VirtualFileDevice::Extension(StringView path, const ExtensionData& data)
    {
        if (VFS::File* file = Files.GetFile(path))
            return file->Ops->Extension(*file, data);

        return false;
    }
} // namespace Swage