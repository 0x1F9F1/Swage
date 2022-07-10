#pragma once

#include "asset/device/archive.h"

namespace Swage::Rage
{
    struct fiPackHeader2
    {
        u32 Magic;
        u32 HeaderSize;
        u32 EntryCount;

        u32 dwordC;
        u32 HeaderDecryptionTag;

        // Used in game.rpf for GTA IV to signal all file should be decrypted immediately (requires/assumes the whole RPF is preloaded into memory)
        u32 FileDecryptionTag;
    };

    struct fiPackEntry2
    {
        // NameOffset
        u32 dword0;

        // OnDiskSize
        u32 dword4;

        // ResourceType:8 (Resource)
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

        u32 GetNameOffset() const
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
            return (dwordC & 0x7FF) << (((dwordC >> 11) & 0xF) + 8);
        }

        u32 GetPhysicalSize() const
        {
            return ((dwordC >> 15) & 0x7FF) << (((dwordC >> 26) & 0xF) + 8);
        }
    };

    Rc<VirtualFileDevice> LoadRPF2(Rc<Stream> input);
} // namespace Swage::Rage
