#pragma once

namespace Swage::Rage
{
    inline u32 atStringHash(StringView string)
    {
        u32 hash = 0;

        for (const char v : string)
        {
            hash += NormalizeCaseAndSlash[static_cast<unsigned char>(v)];
            hash += hash << 10;
            hash ^= hash >> 6;
        }

        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;

        return hash;
    }
} // namespace Swage::Rage
