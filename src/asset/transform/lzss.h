#pragma once

#include "../transform.h"

namespace Swage
{
    class LzssDecompressor : public BinaryTransform
    {
    public:
        LzssDecompressor();

        bool Reset() override;
        bool Update() override;

    private:
        u16 format_ {0};

        u8 buffered_ {0};
        u8 buffer_[2];

        u16 current_ {0};
        u16 pending_ {0};
        u8 window_[0x1000];

        void FlushPending();
    };
} // namespace Swage
