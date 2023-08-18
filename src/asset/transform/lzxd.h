#pragma once

#include "../transform.h"

namespace Swage
{
    Ptr<BinaryTransform> CreateLzxdDecompressor(u32 window_size, u32 partition_size);
} // namespace Swage
