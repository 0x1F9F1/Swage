#include "cipher16.h"

namespace Swage::Rage
{
    AesEcbCipher16::AesEcbCipher16(const u8* key, usize key_length, bool decrypt)
        : cipher_(key, key_length, decrypt)
    {}

    usize AesEcbCipher16::Update(const u8* input, u8* output, usize length)
    {
        for (usize i = 0; i < 16; ++i)
            length = cipher_.Update(input, output, length);

        return length;
    }

    usize AesEcbCipher16::GetBlockSize()
    {
        return cipher_.GetBlockSize();
    }
} // namespace Swage::Rage
