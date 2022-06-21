#pragma once

#include "asset/filedeviceext.h"

namespace Swage::Rage
{
    struct datResourceInfo
    {
        u32 VirtualFlags {0};
        u32 PhysicalFlags {0};
    };

    struct datResourceFileHeader
    {
        u32 Magic {0};
        u32 Flags {0};
        datResourceInfo ResourceInfo;
    };

    struct datResourceChunk
    {
        u64 Addr;
        u64 Size;
    };

    struct ResourceFileHeaderExtension : FileDeviceExtensionT<ResourceFileHeaderExtension>
    {
        datResourceFileHeader Header;

        static constexpr const StringView ExtensionName = "ResourceFileHeader";
    };
} // namespace Swage::Rage

namespace Swage::Rage::RSC7
{
    u32 GetChunkCount(u32 flags);
    u32 GetLargeResourceSize(u8 header[16]);

    u64 GetResourceSize(u32 flags, u32 chunk_size);

    Vec<datResourceChunk> GetChunkMap(u32 flags, u32 chunk_size, u64 vaddr, u64& total);
} // namespace Swage::RSC7
