#pragma once

#include "../stream.h"

namespace Swage
{
    class MemoryStream : public Stream
    {
    public:
        MemoryStream(void* data = nullptr, usize size = 0);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize ReadBulk(void* ptr, usize len, i64 offset) override;

        i64 CopyTo(Stream& output) override;
        bool IsSynchronized() override;

    protected:
        u8* data_ {};
        i64 size_ {};
        i64 current_ {};

        usize ReadInternal(void* ptr, usize len, i64 offset);
    };
} // namespace Swage
