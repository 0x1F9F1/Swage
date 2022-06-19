#include "rsc.h"

namespace Swage::RSC7
{
    // constexpr const char* streaming_file_exts[] {"rpf", "ydr", "yft", "ydd", "ytd", "ycd", "ybn", "ybd", "ybs", "yld",
    //     "ypm", "yed", "ypt", "ymt", "ymap", "ypdb", "mrf", "ynv", "yhn", "ysc", "cut", "gfx", "ypl", "yam", "ynd",
    //     "nod", "yvr", "ywr", "ymf", "ytyp", "ynh", "yfd", "yldb"};

    u32 GetChunkCount(u32 flags)
    {
        return ((flags >> 4) & 1) + ((flags >> 5) & 3) + ((flags >> 7) & 0xF) + ((flags >> 8) & 1) +
            ((flags >> 25) & 1) + ((flags >> 26) & 1) + ((flags >> 27) & 1) + ((flags >> 11) & 0x3F) +
            ((flags >> 17) & 0x7F);
    }

    u32 GetLargeResourceSize(u8 header[16])
    {
        return u32(header[7]) | (u32(header[14]) << 8) | (u32(header[5]) << 16) | (u32(header[2]) << 24);
    }

    u64 GetResourceSize(u32 flags, u32 chunk_size)
    {
        chunk_size <<= (flags & 0xF) + 4;

        u64 total = 0;

        total += ((flags >> 4) & 0x1) * (chunk_size >> 0);
        total += ((flags >> 5) & 0x3) * (chunk_size >> 1);
        total += ((flags >> 7) & 0xF) * (chunk_size >> 2);
        total += ((flags >> 11) & 0x3F) * (chunk_size >> 3);
        total += ((flags >> 17) & 0x7F) * (chunk_size >> 4);
        total += ((flags >> 24) & 0x1) * (chunk_size >> 5);
        total += ((flags >> 25) & 0x1) * (chunk_size >> 6);
        total += ((flags >> 26) & 0x1) * (chunk_size >> 7);
        total += ((flags >> 27) & 0x1) * (chunk_size >> 8);

        return total;
    }

    Vec<datResourceChunk> GetChunkMap(u32 flags, u32 chunk_size, u64 vaddr, u64& total)
    {
        chunk_size <<= (flags & 0xF) + 4;

        const u32 counts[9] {
            (flags >> 4) & 0x1,
            (flags >> 5) & 0x3,
            (flags >> 7) & 0xF,
            (flags >> 11) & 0x3F,
            (flags >> 17) & 0x7F,
            (flags >> 24) & 0x1,
            (flags >> 25) & 0x1,
            (flags >> 26) & 0x1,
            (flags >> 27) & 0x1,
        };

        Vec<datResourceChunk> chunks;

        u64 offset = 0;

        for (u32 v : counts)
        {
            for (u32 i = 0; i < v; ++i)
            {
                chunks.push_back({vaddr + offset, chunk_size});

                offset += chunk_size;
            }

            chunk_size >>= 1;
        }

        total = offset;

        return chunks;
    }
} // namespace Swage::RSC7
