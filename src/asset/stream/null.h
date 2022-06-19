#pragma once

#include "../stream.h"

namespace Swage
{
    class NullStream final : public Stream
    {
    public:
        NullStream(i64 size = 0);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize Write(const void* ptr, usize len) override;

    private:
        i64 current_ {};
        i64 size_ {};
    };
} // namespace Swage
