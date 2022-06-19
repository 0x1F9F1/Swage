#pragma once

#include "cipher.h"

namespace Swage
{
    struct Tfit2Context
    {
        u64 InitTables[16][256];

        struct Round
        {
            u64 Lookup[4096];

            struct Block
            {
                u64 Masks[16];
                u32 Xor;
            } Blocks[16];
        } Rounds[17];

        u64 EndMasks[16][8];
        u8 EndTables[16][256];
        u8 EndXor[16];
    };

    struct Tfit2Key
    {
        u64 Data[18][2];
    };

    class Tfit2CbcCipher final : public Cipher
    {
    public:
        Tfit2CbcCipher(const Tfit2Key* key, const u8 iv[16], const Tfit2Context* ctx);

        usize Update(const u8* input, u8* output, usize length) override;

        usize GetBlockSize() override;

    private:
        const u64 (*keys_)[2] {};
        const Tfit2Context* ctx_ {};
        u8 iv_[16] {};
    };
} // namespace Swage
