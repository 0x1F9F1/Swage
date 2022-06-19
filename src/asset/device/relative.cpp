#include "relative.h"

#include "../findhandle.h"
#include "../stream.h"

namespace Swage
{
    RelativeFileDevice::RelativeFileDevice(String prefix, Rc<FileDevice> device)
        : prefix_(std::move(prefix))
        , device_(std::move(device))
    {}

    Rc<Stream> RelativeFileDevice::Open(StringView path, bool read_only)
    {
        return device_->Open(ToAbsolute(path), read_only);
    }

    Rc<Stream> RelativeFileDevice::Create(StringView path, bool write_only, bool truncate)
    {
        return device_->Create(ToAbsolute(path), write_only, truncate);
    }

    bool RelativeFileDevice::Exists(StringView path)
    {
        return device_->Exists(ToAbsolute(path));
    }

    Ptr<FileEnumerator> RelativeFileDevice::Enumerate(StringView path)
    {
        return device_->Enumerate(ToAbsolute(path));
    }

    String RelativeFileDevice::ToAbsolute(StringView path)
    {
        return Concat(prefix_, path);
    }
} // namespace Swage
