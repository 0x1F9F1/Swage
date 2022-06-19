#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
} // namespace Swage

namespace Swage::Angel
{
    struct AresHeader
    {
        u32 Magic;
        u32 FileCount;
        u32 RootCount;
        u32 NamesSize;
    };

    struct VirtualFileINode
    {
        u32 dword0;
        u32 dword4;
        u32 dword8;

        u32 GetOffset() const
        {
            return dword0;
        }

        u32 GetSize() const
        {
            return dword4 & 0x7FFFFF;
        }

        u32 GetExtOffset() const
        {
            return (dword4 >> 23) & 0x1FF;
        }

        bool IsDirectory() const
        {
            return (dword8 & 1) != 0;
        }

        u32 GetNameInteger() const
        {
            return (dword8 >> 1) & 0x1FFF;
        }

        u32 GetNameOffset() const
        {
            return (dword8 >> 14) & 0x3FFFF;
        }
    };

    Rc<FileDevice> LoadARES(Rc<Stream> input);
} // namespace Swage::Angel
