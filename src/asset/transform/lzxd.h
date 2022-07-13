#pragma once

#include "../transform.h"

namespace Swage
{
    class LzxdDecompressor : public BinaryTransform
    {
    public:
        LzxdDecompressor(u32 window_size, u32 partition_size);
        ~LzxdDecompressor() override;

        bool Reset();
        bool Update();

    private:
        void* state_ {};
        void* context_ {};
    };
} // namespace Swage
