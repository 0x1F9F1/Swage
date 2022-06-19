#pragma once

#include "../transform.h"

#include <zlib-ng.h>

namespace Swage
{
    class DeflateDecompressor : public BinaryTransform
    {
    public:
        DeflateDecompressor(i32 window_bits);
        ~DeflateDecompressor() override;

        bool Reset() override;
        bool Update() override;

    private:
        zng_stream inflater_ {};
    };
} // namespace Swage
