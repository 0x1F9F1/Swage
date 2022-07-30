#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
} // namespace Swage

namespace Swage::Rage
{
    struct IMGHeader3
    {
        u32 Magic;
        u32 Version;
        u32 EntryCount;
        u32 HeaderSize;
        u16 EntrySize;
        u16 word12;
    };

    struct IMGEntry3
    {
        u32 dword0;
        u32 dword4;
        u32 dword8;
        u16 wordC;
        u16 wordE;

        u32 GetVirtualSize() const
        {
            return (dword0 & 0x7FF) << (((dword0 >> 11) & 0xF) + 8);
        }

        u32 GetPhysicalSize() const
        {
            return ((dword0 >> 15) & 0x7FF) << (((dword0 >> 26) & 0xF) + 8);
        }

        u32 GetResourceType() const
        {
            return dword4;
        }

        u32 GetOffset() const
        {
            return dword8 << 11;
        }

        u32 GetOnDiskSize() const
        {
            return (static_cast<u32>(wordC) << 11) - (wordE & 0x7FF);
        }

        bool IsResource() const
        {
            return wordE & 0x2000;
        }

        bool IsOldResource() const
        {
            return wordE & 0x4000;
        }

        u32 GetSize() const
        {
            if (IsResource())
            {
                u32 virt_size = GetVirtualSize();
                u32 phys_size = GetPhysicalSize();

                return virt_size + phys_size;
            }
            else
            {
                return GetOnDiskSize();
            }
        }
    };

    Rc<FileDevice> LoadIMG3(Rc<Stream> input);
} // namespace Swage::Rage
