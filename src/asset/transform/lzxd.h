#pragma once

#include "../transform.h"

namespace Swage
{
    class LzxdDecompressor : public BinaryTransform
    {
    public:
        LzxdDecompressor();
        ~LzxdDecompressor() override;

        bool Reset();
        bool Update();

    private:
        void* state_ {};
        void* context_ {};
    };
} // namespace Swage
