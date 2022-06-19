#include "archive.h"

#include "../filedevice.h"
#include "../findhandle.h"
#include "../stream.h"

#include "../stream/decode.h"
#include "../stream/partial.h"

#include "../transform/deflate.h"
#include "../transform/lzss.h"

namespace Swage
{
    ArchiveFile ArchiveFile::Deflated(u64 offset, u64 size, u64 raw_size, i32 window_bits)
    {
        return ArchiveFile(
            offset, size, raw_size, [window_bits] { return MakeUnique<DeflateDecompressor>(window_bits); });
    }

    Rc<Stream> ArchiveFile::Open(Rc<Stream> input) const
    {
        Rc<Stream> result = MakeRc<PartialStream>(Offset, RawSize, std::move(input));

        if (Transform)
            result = MakeRc<DecodeStream>(std::move(result), Transform(), Size);

        return result;
    }

    FileArchive::FileArchive(Rc<Stream> input)
        : Input(std::move(input))
    {}

    Rc<Stream> FileArchive::Open(File& file)
    {
        ArchiveFile& data = GetData(file);

        return data.Open(Input);
    }

    void FileArchive::Stat(File& file, FolderEntry& entry)
    {
        ArchiveFile& data = GetData(file);

        entry.Size = data.Size;
    }
} // namespace Swage
