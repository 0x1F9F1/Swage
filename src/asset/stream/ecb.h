#pragma once

#include "../stream.h"

namespace Swage
{
    class Cipher;

    class EcbCipherStream final : public Stream
    {
    public:
        EcbCipherStream(Rc<Stream> input, Ptr<Cipher> cipher);
        ~EcbCipherStream() override;

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;

    private:
        Rc<Stream> input_ {};

        Ptr<Cipher> cipher_ {};
        usize block_size_ {0};

        usize padding_ {0};
        usize buffered_ {0};
        u8 buffer_[32];

        void RefillBuffer();
        usize ReadBuffered(void* ptr, usize len);
    };
} // namespace Swage
