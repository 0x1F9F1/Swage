#pragma once

#include "asset/filedeviceext.h"

namespace Swage::Rage
{
    struct datResourceInfo
    {
        u32 VirtualFlags {};
        u32 PhysicalFlags {};
    };

    struct datResourceFileHeader
    {
        u32 Magic {};
        u32 Flags {};
        datResourceInfo ResourceInfo {};

        static constexpr StringView ExtensionName = "datResourceFileHeader";
    };

    struct datResourceChunk
    {
        u64 Addr;
        u64 Size;
    };
} // namespace Swage::Rage

namespace Swage::Rage::RSC7
{
    u32 GetChunkCount(u32 flags);
    u32 GetLargeResourceSize(u8 header[16]);

    u64 GetResourceSize(u32 flags, u32 chunk_size);

    Vec<datResourceChunk> GetChunkMap(u32 flags, u32 chunk_size, u64 vaddr, u64& total);
} // namespace Swage::Rage::RSC7
