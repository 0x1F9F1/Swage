#pragma once

#include "asset/device/archive.h"

namespace Swage::Rage
{
    struct fiPackHeader6
    {
        u32 Magic;
        u32 EntryCount;
        u32 NamesOffset;
        u32 DecryptionTag;
    };

    struct fiPackEntry6
    {
        // NameHash
        u32 dword0;

        // OnDiskSize
        u32 dword4;

        // ResourceVersion:8 (Resource)
        // Offset:23 (Resource)
        // Offset:31 (Binary)
        // EntryIndex:31 (Directory)
        // IsDirectory:1
        u32 dword8;

        // Size:30 (Binary)
        // EntryCount:30 (Directory)
        // ResourceFlags:30 (Resource)
        // IsCompressed:1
        // IsResource:1
        u32 dwordC;

        // VirtualFlags:14 (Resource)
        // PhysicalFlags:14 (Resource)
        // MainChunkOffset:3 (Resource)
        // HasResourceFlags:1 (Resource)
        u32 dword10;

        u32 GetHash() const
        {
            return dword0;
        }

        u32 GetOnDiskSize() const
        {
            return dword4;
        }

        u32 GetOffset() const
        {
            return IsResource() ? (dword8 & 0x7FFFFF00) : (dword8 & 0x7FFFFFFF);
        }

        u32 GetEntryIndex() const
        {
            return dword8 & 0x7FFFFFFF;
        }

        bool IsDirectory() const
        {
            return dword8 & 0x80000000;
        }

        u8 GetResourceVersion() const
        {
            return static_cast<u8>(dword8 & 0xFF);
        }

        u32 GetSize() const
        {
            if (IsResource())
                return GetVirtualSize() + GetPhysicalSize();

            return dwordC & 0x3FFFFFFF;
        }

        u32 GetEntryCount() const
        {
            return dwordC & 0x3FFFFFFF;
        }

        u32 GetResourceFlags() const
        {
            return dwordC & 0x3FFFFFFF;
        }

        bool IsResource() const
        {
            return dwordC & 0x80000000;
        }

        bool IsCompressed() const
        {
            // if (IsResource())
            //     return false;

            return dwordC & 0x40000000;
        }

        u32 GetVirtualSize() const
        {
            if (HasExtendedFlags())
                return (dword10 & 0x3FFF) << 12;
            else
                return (dwordC & 0x7FF) << (((dwordC >> 11) & 0xF) + 8);
        }

        u32 GetPhysicalSize() const
        {
            if (HasExtendedFlags())
                return ((dword10 >> 14) & 0x3FFF) << 12;
            else
                return ((dwordC >> 15) & 0x7FF) << (((dwordC >> 26) & 0xF) + 8);
        }

        bool HasExtendedFlags() const
        {
            return (dword10 & 0x80000000) == 0x80000000;
        }

        u32 GetMainChunkOffset() const
        {
            if (HasExtendedFlags())
            {
                u32 vsize = GetVirtualSize();
                u32 main_chunk_size = 0x1000 << ((dword10 >> 28) & 0x7);

                if (vsize > 0x80000)
                    vsize = 0x80000;

                if (main_chunk_size <= vsize)
                    return vsize & ~u32((main_chunk_size << 1) - 1);
            }

            return 0;
        }
    };

    Rc<VirtualFileDevice> LoadRPF6(Rc<Stream> input);
} // namespace Swage::Rage
