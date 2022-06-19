#pragma once

#include "rsc.h"

namespace Swage
{
    class Stream;
    class FileDevice;

    struct SecretId;
    class SecretFinder;

    class BufferedStream;
} // namespace Swage

namespace Swage::Rage
{
    struct fiPackHeader8
    {
        u32 Magic;
        u32 EntryCount;
        u32 NamesLength;
        u16 DecryptionTag;

        // switch (platform)
        // {
        //     case "android": return 'a';
        //     case "ps3": case "psn": return 'c';
        //     case "durango": case "xboxone": return 'd';
        //     case "orbis": case "ps4": return 'o';
        //     case "ios": return 's';
        //     case "psp2": case "vita": return 'v';
        //     case "win32pc": case "x86": case "pc": case "win32": return 'w';
        //     case "xenon": case "xbox360": return 'x';
        //     case "win64pc": case "x64": case "win64": case "linux": return 'y';
        //     default: return 'u';
        // }
        u16 PlatformId;

        // rage::fiPackfile8::SignatureVerify
        // Hash(this, 0x10)
        // Hash(&Entries, sizeof(fiPackEntry8) * EntryCount)
        // u8 RSASignature[0x100];

        // Entries are sorted based on their hash, which is calculated based on the file path, minus the extension if Entry.GetFileExtId() != 0xFF
        // In R* RPFs the data of the files appears to be sorted based on the file name (not including path), which could be useful for dehashing names
        // fiPackEntry8 Entries[EntryCount];
    };

    struct fiPackEntry8
    {
    private:
        // FileHash:32
        // EncryptionConfig:8
        // EncryptionKeyId:8
        // FileExtId:8
        // IsResource:1
        // IsSignatureProtected:1
        u64 qword0;

        // OnDiskSize:28
        // Offset:31
        // CompresorId:5
        u64 qword8;

        // rage::datResourceInfo
        // ResIdUpper:4
        // Size1:28
        // ResIdLower:4
        // Size2:28
        u64 qword10;

    public:
        u32 GetHash() const
        {
            return qword0 & 0xFFFFFFFF;
        }

        u8 GetEncryptionConfig() const
        {
            return (qword0 >> 32) & 0xFF;
        }

        u8 GetEncryptionKeyId() const
        {
            return (qword0 >> 40) & 0xFF;
        }

        u8 GetFileExtId() const
        {
            return (qword0 >> 48) & 0xFF;
        }

        bool IsResource() const
        {
            return (qword0 >> 56) & 1; // 0x100000000000000
        }

        bool IsSignatureProtected() const
        {
            return (qword0 >> 57) & 1; // 0x200000000000000
        }

        u32 GetOnDiskSize() const
        {
            return (qword8 & 0xFFFFFFF) << 4;
        }

        u64 GetOffset() const
        {
            return ((qword8 >> 28) & 0x7FFFFFFF) << 4;
        }

        // rage::eCompressorID
        // 0: Uncompressed
        // 1: Deflate
        // 2: Oodle
        u8 GetCompressorId() const
        {
            return (qword8 >> 59) & 0x1F;
        }

        // No RPFs currently use directories
        bool IsDirectory() const
        {
            return GetFileExtId() == 0xFE;
        }

        bool IsEncrypted() const
        {
            return GetEncryptionKeyId() != 0xFF;
        }

        u32 GetVirtualFlags() const
        {
            return qword10 & 0xFFFFFFFF;
        }

        u32 GetPhysicalFlags() const
        {
            return qword10 >> 32;
        }

        u32 GetVirtualSize() const
        {
            return qword10 & 0xFFFFFFF0;
        }

        u32 GetPhysicalSize() const
        {
            return (qword10 >> 32) & 0xFFFFFFF0;
        }

        u8 GetResourceId() const
        {
            u8 upper = (qword10 & 0xF);
            u8 lower = (qword10 >> 32) & 0xF;

            return (upper << 4) | lower;
        }

        u64 GetSize() const
        {
            if (IsDirectory())
                return 0;

            if (IsResource())
            {
                u64 size0 = GetVirtualSize();
                u64 size1 = GetPhysicalSize();

                return size0 + size1;
            }
            else
            {
                return qword10;
            }
        }

        bool GetResourceFileHeader(datResourceFileHeader& info) const
        {
            if (!IsResource())
                return false;

#if 1
            u32 bits0 = GetResourceId();
            u32 bits8 = ((0 - 1) & 0x1F) << 8;
            u32 bits15 = false << 15;
            u32 bits16 = (0xFF + 1) << 16;
#else
            u32 bits0 = GetResourceId();
            u32 bits8 = ((GetCompressorId() - 1) & 0x1F) << 8;
            u32 bits15 = IsSignatureProtected() << 15;
            u32 bits16 = (GetEncryptionKeyId() + 1) << 16;
#endif

            info.Magic = 0x38435352; // RSC8
            info.Flags = bits0 | bits8 | bits15 | bits16;

            info.ResourceInfo.VirtualFlags = GetVirtualFlags();
            info.ResourceInfo.PhysicalFlags = GetPhysicalFlags();

            return true;
        }
    };

    Rc<FileDevice> LoadRPF8(Rc<Stream> input);

    namespace RPF8
    {
        void LoadKeys();

        void FindKeys_RDR2_PS4(SecretFinder& finder);
        void FindKeys_RDR2_PC(SecretFinder& finder);

        void LoadFileList(BufferedStream& input);
        void SaveFileList(BufferedStream& output);

        void LoadPossibleFileList(BufferedStream& input);
    } // namespace RPF8
} // namespace Swage::Rage
