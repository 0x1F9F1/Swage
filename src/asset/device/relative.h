#pragma once

#include "../filedevice.h"

namespace Swage
{
    class RelativeFileDevice final : public FileDevice
    {
    public:
        RelativeFileDevice(String prefix, Rc<FileDevice> device);

        Rc<Stream> Open(StringView path, bool read_only) override;
        Rc<Stream> Create(StringView path, bool write_only, bool truncate) override;

        bool Exists(StringView path) override;

        Ptr<FileEnumerator> Enumerate(StringView path) override;

    private:
        String prefix_ {};
        Rc<FileDevice> device_;

        String ToAbsolute(StringView path);
    };
} // namespace Swage
