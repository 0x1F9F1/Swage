#pragma once

namespace Swage
{
    extern const u64 CRC64Table[256];

    inline u64 CRC64(const void* data, usize length, u64 crc = 0)
    {
        const u8* data8 = static_cast<const u8*>(data);

        crc = ~crc;

        for (usize i = 0; i < length; i++)
            crc = (crc >> 8) ^ CRC64Table[(crc ^ data8[i]) & 0xFF];

        return ~crc;
    }
} // namespace Swage