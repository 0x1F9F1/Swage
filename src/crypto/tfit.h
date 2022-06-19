#pragma once

#include "cipher.h"

namespace Swage
{
    class TfitEcbCipher final : public Cipher
    {
    public:
        TfitEcbCipher(const u32 keys[17][4], const u32 tables[17][16][256]);

        usize Update(const u8* input, u8* output, usize length) override;

        usize GetBlockSize() override;

        const u32 (*keys_)[4] {};
        const u32 (*tables_)[16][256] {};
    };

    class TfitCbcCipher final : public Cipher
    {
    public:
        TfitCbcCipher(const u32 keys[17][4], const u32 tables[17][16][256], const u8 iv[16]);

        usize Update(const u8* input, u8* output, usize length) override;

        usize GetBlockSize() override;

        const u32 (*keys_)[4] {};
        const u32 (*tables_)[16][256] {};

        u8 iv_[16] {};
    };
} // namespace Swage
