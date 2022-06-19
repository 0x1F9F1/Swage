#include "tfit.h"

namespace Swage
{
    static SW_FORCEINLINE void TFIT_DecryptRoundA(u8 data[16], const u32 key[4], const u32 table[16][256])
    {
        u32 temp[4] {
            table[0][data[0]] ^ table[1][data[1]] ^ table[2][data[2]] ^ table[3][data[3]] ^ key[0],
            table[4][data[4]] ^ table[5][data[5]] ^ table[6][data[6]] ^ table[7][data[7]] ^ key[1],
            table[8][data[8]] ^ table[9][data[9]] ^ table[10][data[10]] ^ table[11][data[11]] ^ key[2],
            table[12][data[12]] ^ table[13][data[13]] ^ table[14][data[14]] ^ table[15][data[15]] ^ key[3],
        };

        std::memcpy(data, temp, 16);
    }

    static SW_FORCEINLINE void TFIT_DecryptRoundB(u8 data[16], const u32 key[4], const u32 table[16][256])
    {
        u32 temp[4] {
            table[0][data[0]] ^ table[1][data[7]] ^ table[2][data[10]] ^ table[3][data[13]] ^ key[0],
            table[4][data[1]] ^ table[5][data[4]] ^ table[6][data[11]] ^ table[7][data[14]] ^ key[1],
            table[8][data[2]] ^ table[9][data[5]] ^ table[10][data[8]] ^ table[11][data[15]] ^ key[2],
            table[12][data[3]] ^ table[13][data[6]] ^ table[14][data[9]] ^ table[15][data[12]] ^ key[3],
        };

        std::memcpy(data, temp, 16);
    }

    static SW_FORCEINLINE void TFIT_DecryptBlock(
        const u8 input[16], u8 output[16], const u32 keys[17][4], const u32 tables[17][16][256])
    {
        u8 temp[16];
        std::memcpy(temp, input, 16);

        TFIT_DecryptRoundA(temp, keys[0], tables[0]);
        TFIT_DecryptRoundA(temp, keys[1], tables[1]);

        for (usize i = 2; i < 16; ++i)
            TFIT_DecryptRoundB(temp, keys[i], tables[i]);

        TFIT_DecryptRoundA(temp, keys[16], tables[16]);

        std::memcpy(output, temp, 16);
    }

    constexpr usize TFIT_BLOCK_SIZE = 16;

    TfitEcbCipher::TfitEcbCipher(const u32 keys[17][4], const u32 tables[17][16][256])
        : keys_(keys)
        , tables_(tables)
    {}

    usize TfitEcbCipher::Update(const u8* input, u8* output, usize length)
    {
        for (usize blocks = length / TFIT_BLOCK_SIZE; blocks;
             --blocks, input += TFIT_BLOCK_SIZE, output += TFIT_BLOCK_SIZE)
        {
            TFIT_DecryptBlock(input, output, keys_, tables_);
        }

        return length;
    }

    usize TfitEcbCipher::GetBlockSize()
    {
        return TFIT_BLOCK_SIZE;
    }

    TfitCbcCipher::TfitCbcCipher(const u32 keys[17][4], const u32 tables[17][16][256], const u8 iv[16])
        : keys_(keys)
        , tables_(tables)
    {
        std::memcpy(iv_, iv, 0x10);
    }

    usize TfitCbcCipher::Update(const u8* input, u8* output, usize length)
    {
        u8 iv[2][16];

        u8* current_iv = iv_;

        for (usize blocks = length / TFIT_BLOCK_SIZE; blocks;
             --blocks, input += TFIT_BLOCK_SIZE, output += TFIT_BLOCK_SIZE)
        {
            u8* const next_iv = iv[blocks & 1];

            std::memcpy(next_iv, input, TFIT_BLOCK_SIZE);

            TFIT_DecryptBlock(input, output, keys_, tables_);

            for (usize j = 0; j < 16; ++j)
                output[j] ^= current_iv[j];

            current_iv = next_iv;
        }

        std::memcpy(iv_, current_iv, 16);

        return length;
    }

    usize TfitCbcCipher::GetBlockSize()
    {
        return TFIT_BLOCK_SIZE;
    }
} // namespace Swage
