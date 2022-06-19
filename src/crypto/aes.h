#pragma once

#include "cipher.h"

#include <wolfssl/wolfcrypt/aes.h>

namespace Swage
{
    class AesEcbCipher final : public Cipher
    {
    public:
        AesEcbCipher(const u8* key, usize key_length, bool decrypt);

        usize Update(const u8* input, u8* output, usize length) override;

        usize GetBlockSize() override;

        Aes ctx_ {};
    };
} // namespace Swage
