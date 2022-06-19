#pragma once

#include "../filedevice.h"

namespace Swage
{
    class LocalFileDevice final : public FileDevice
    {
    public:
        Rc<Stream> Open(StringView path, bool read_only) override;
        Rc<Stream> Create(StringView path, bool write_only, bool truncate) override;

        bool Exists(StringView path) override;

        Ptr<FileEnumerator> Enumerate(StringView path) override;

        friend const Rc<LocalFileDevice>& LocalFiles();

    private:
        LocalFileDevice();

        static Rc<LocalFileDevice> instance_;
    };

    inline const Rc<LocalFileDevice>& LocalFiles()
    {
        return LocalFileDevice::instance_;
    }
} // namespace Swage
