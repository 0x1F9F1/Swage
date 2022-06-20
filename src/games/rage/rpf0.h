#pragma once

#include "asset/device/archive.h"

namespace Swage::Rage
{
    struct fiPackHeader0
    {
        u32 Magic;
        u32 HeaderSize;
        u32 EntryCount;

        // u32 dwordC;
        // u32 HeaderDecryptionTag;

        // Used in game.rpf for GTA IV to signal all file should be decrypted immediately (requires/assumes the whole RPF is preloaded into memory)
        // u32 BodyDecryptionTag;
    };

    struct fiPackEntry0
    {
        // NameOffset:31
        // IsDirectory:1
        u32 dword0;

        // Offset (Binary)
        // EntryIndex (Directory)
        u32 dword4;

        // OnDiskSize (Binary)
        // EntryCount (Directory)
        u32 dword8;

        // Size (Binary)
        // EntryCount (Directory)
        u32 dwordC;

        u32 GetNameOffset() const
        {
            return dword0 & 0x7FFFFFFF;
        }

        u32 GetOnDiskSize() const
        {
            return dword8;
        }

        u32 GetOffset() const
        {
            return dword4;
        }

        bool IsDirectory() const
        {
            return dword0 & 0x80000000;
        }

        u32 GetSize() const
        {
            return dwordC;
        }

        u32 GetEntryIndex() const
        {
            return dword4;
        }

        u32 GetEntryCount() const
        {
            return dword8;
        }
    };

    Rc<VirtualFileDevice> LoadRPF0(Rc<Stream> input);
} // namespace Swage::Rage
