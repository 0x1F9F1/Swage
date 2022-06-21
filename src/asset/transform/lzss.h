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
        u16 format_ {};

        u8 buffered_ {};
        u8 buffer_[2];

        u16 current_ {};
        u16 pending_ {};
        u8 window_[0x1000];

        void FlushPending();
    };
} // namespace Swage
