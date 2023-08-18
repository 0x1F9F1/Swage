#pragma once

#include "../transform.h"

// The number of raw (uncompressed) bytes in each block (apart the final block, which may be smaller)
#define OODLELZ_BLOCK_LEN (1 << 18)

// Each block starts with a 2 byte header specifying the compression type.
// If the compression type is memcpy, then the body of the block is just the uncomprsesed data (of length OODLELZ_BLOCK_LEN)
// Therefore, the maximum compressed length should be (OODLELZ_BLOCK_LEN + 2).
// However, it is unclear if this is actually the case, so just add a bit more.
#define OODLELZ_BLOCK_MAX_COMPLEN (OODLELZ_BLOCK_LEN + 128)

namespace Swage
{
    enum OodleLZ_Compressor : i32
    {
        OodleLZ_Compressor_Invalid = -1,

        OodleLZ_Compressor_LZH,
        OodleLZ_Compressor_LZHLW,
        OodleLZ_Compressor_LZNIB,
        OodleLZ_Compressor_None,
        OodleLZ_Compressor_LZB16,
        OodleLZ_Compressor_LZBLW,
        OodleLZ_Compressor_LZA,
        OodleLZ_Compressor_LZNA,
        OodleLZ_Compressor_Kraken,
        OodleLZ_Compressor_Mermaid,
        OodleLZ_Compressor_BitKnit,
        OodleLZ_Compressor_Selkie,
        OodleLZ_Compressor_Hydra,
        OodleLZ_Compressor_Leviathan,
    };

    Ptr<BinaryTransform> CreateOodleDecompressor(i64 size, OodleLZ_Compressor compressor = OodleLZ_Compressor_Invalid);
} // namespace Swage
