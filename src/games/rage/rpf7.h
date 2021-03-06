#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
    class SecretFinder;
} // namespace Swage

namespace Swage::Rage
{
    struct datResourceFileHeader;

    struct fiPackHeader7
    {
        u32 Magic;
        u32 EntryCount;

        // NamesLength:28
        // NameShift:3
        // PlatformBit:1
        u32 dwordC;
        u32 DecryptionTag;

        u32 GetNamesLength() const
        {
            return dwordC & 0xFFFFFFF;
        }

        bool GetPlatformBit() const
        {
            return dwordC & 0x80000000;
        }

        u8 GetNameShift() const
        {
            return (dwordC >> 28) & 0x7;
        }
    };

    struct fiPackEntry7
    {
        // NameOffset:16
        // OnDiskSize:24
        // Offset:23
        // IsResource:1
        u64 qword0;

        // Size (Binary)
        // VirtualFlags (Resource)
        // EntryIndex (Directory)
        u32 dword8;

        // DecryptionTag (Binary)
        // PhysicalFlags (Resource)
        // EntryCount (Directory)
        u32 dwordC;

        u16 GetNameOffset() const
        {
            return qword0 & 0xFFFF;
        }

        u32 GetOnDiskSize() const
        {
            return (qword0 >> 16) & 0xFFFFFF;
        }

        u32 GetOffset() const
        {
            return ((qword0 >> 40) & 0x7FFFFF) << 9;
        }

        bool IsDirectory() const
        {
            return GetOffset() == 0xFFFFFE00;
        }

        bool IsResource() const
        {
            return (qword0 >> 63) == 1;
        }

        u32 GetVirtualFlags() const
        {
            return dword8;
        }

        u32 GetPhysicalFlags() const
        {
            return dwordC;
        }

        u32 GetDecryptionTag() const
        {
            return dwordC;
        }

        void SetDecryptionTag(u32 tag)
        {
            dwordC = tag;
        }

        u32 GetSize() const
        {
            return dword8;
        }

        u8 GetResourceVersion() const
        {
            u8 upper = (dword8 >> 28);
            u8 lower = (dwordC >> 28);

            return (upper << 4) | lower;
        }

        u32 GetEntryIndex() const
        {
            return dword8;
        }

        u32 GetEntryCount() const
        {
            return dwordC;
        }

        bool GetResourceFileHeader(datResourceFileHeader& info) const;
    };

    Rc<FileDevice> LoadRPF7(Rc<Stream> input, StringView name);

    namespace RPF7
    {
        void LoadKeys();

        void FindKeys_GTA5_PC(SecretFinder& finder);
    } // namespace RPF7
} // namespace Swage::Rage
