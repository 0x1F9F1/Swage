#pragma once

#include "../stream.h"

namespace Swage
{
    class SyncStream final : public Stream
    {
    public:
        SyncStream(Rc<Stream> input);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize ReadBulk(void* ptr, usize len, i64 offset) override;

    private:
        Rc<Stream> input_ {};
        Mutex lock_;
    };
} // namespace Swage
