#pragma once

#include "../stream.h"

namespace Swage
{
    class BinaryTransform;

    class DecodeStream final : public Stream
    {
    public:
        DecodeStream(Rc<Stream> handle, Ptr<BinaryTransform> transform, i64 size, usize buffer_size = SIZE_MAX);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;

    private:
        Rc<Stream> input_;
        Ptr<BinaryTransform> transform_;

        i64 size_ {0};
        i64 current_ {0};

        Ptr<u8[]> buffer_;
        usize buffer_size_ {0};
    };
} // namespace Swage
