#pragma once

#include "../transform.h"

namespace Swage
{
    Ptr<BinaryTransform> CreateDeflateDecompressor(i32 window_bits);
} // namespace Swage
