#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
} // namespace Swage

namespace Swage::Angel
{
    struct DaveHeader
    {
        u32 Magic;
        u32 FileCount;
        u32 NamesOffset;
        u32 NamesSize;
    };

    struct DaveEntry
    {
        u32 NameOffset;
        u32 DataOffset;
        u32 Size;
        u32 RawSize;
    };

    Rc<FileDevice> LoadDave(Rc<Stream> input);
} // namespace Swage::Angel
