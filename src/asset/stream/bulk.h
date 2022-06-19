#pragma once

#include "../stream.h"

namespace Swage
{
    class BulkStream final : public Stream
    {
    public:
        BulkStream(Rc<Stream> handle);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize ReadBulk(void* ptr, usize len, i64 offset) override;

        bool IsSynchronized() override;

    protected:
        Rc<Stream> GetBulkStream(i64& offset, i64 size) override;

    private:
        i64 here_ {0};

        Rc<Stream> input_ {};

        usize ReadInternal(void* ptr, usize len, i64 offset);
    };
} // namespace Swage
