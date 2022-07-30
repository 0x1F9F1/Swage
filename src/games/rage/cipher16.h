#pragma once

#include "crypto/aes.h"

namespace Swage::Rage
{
    class AesEcbCipher16 final : public Cipher
    {
    public:
        AesEcbCipher16(const u8* key, usize key_length, bool decrypt);

    private:
        AesEcbCipher cipher_;

        usize Update(const u8* input, u8* output, usize length) override;
        usize GetBlockSize() override;
    };
} // namespace Swage::Rage
