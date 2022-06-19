#pragma once

#include "../transform.h"

// The number of raw (uncompressed) bytes in each block (apart the final block, which may be smaller)
#define OODLELZ_BLOCK_LEN (1 << 18)

// Each block starts with a 2 byte header specifying the compression type.
// If the compression type is memcpy, then the body of the block is just the uncomprsesed data (of length OODLELZ_BLOCK_LEN)
// Therefore, the maximum compressed length should be (OODLELZ_BLOCK_LEN + 2).
// However, it is unclear if this is actually the case, so just add a bit more.
#define OODLELZ_BLOCK_MAX_COMPLEN (OODLELZ_BLOCK_LEN + 128)

struct OodleLZDecoder;

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

    class OodleDecompressor : public BinaryTransform
    {
    public:
        OodleDecompressor(i64 size, OodleLZ_Compressor compressor = OodleLZ_Compressor_Invalid);
        ~OodleDecompressor();

        bool Reset() override;
        bool Update() override;

        usize GetOptimalBufferSize() override;

    private:
        i32 FlushOutput();
        i32 ConsumeInput();
        i32 DecodeSome();

        // The expected total size of the decompressed data
        isize real_size_ {};

        // Raw memory allocated for the decoder
        void* state_ {};

        // The actual decoder
        OodleLZDecoder* decoder_ {};

        // Total number of bytes decompressed
        isize total_out_ {};

        // The length of input data needed for the next block
        usize needed_in_ {};

        // The length of data in buffer_in_
        usize buffered_in_ {};

        // The range of pending data in buffer_out_
        usize output_start_ {};
        usize output_end_ {};

        // Used to buffer the compressed data for a block
        u8 buffer_in_[OODLELZ_BLOCK_MAX_COMPLEN];

        // Used to buffer the decompresssed data from a block
        // Also acts as sliding window
        u8 buffer_out_[OODLELZ_BLOCK_LEN];
    };
} // namespace Swage
