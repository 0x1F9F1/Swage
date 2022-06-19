#include "zip.h"

#include "archive.h"

#include "asset/stream.h"

#include "asset/stream/buffered.h"
#include "asset/stream/decode.h"
#include "asset/stream/partial.h"

#include "asset/transform/deflate.h"

#include "core/bits.h"

namespace Swage::Zip
{
    // http://www.pkware.com/documents/casestudies/APPNOTE.TXT

    using namespace bits;

    struct ZIPENDLOCATOR
    {
        ple<u32> Signature; // 0x06054B50
        ple<u16> DiskNumber;
        ple<u16> StartDiskNumber;
        ple<u16> EntriesOnDisk;
        ple<u16> EntriesInDirectory;
        ple<u32> DirectorySize;
        ple<u32> DirectoryOffset;
        ple<u16> CommentLength;
    };

    static_assert(sizeof(ZIPENDLOCATOR) == 0x16);

    struct ZIP64ENDLOCATOR
    {
        ple<u32> Signature; // 0x07064B50
        ple<u32> StartDiskNumber;
        ple<i64> DirectoryOffset;
        ple<u32> EntriesInDirectory;
    };

    static_assert(sizeof(ZIP64ENDLOCATOR) == 0x14);

    struct ZIP64ENDLOCATORRECORD
    {
        ple<u32> Signature; // 0x06064B50
        ple<i64> DirectoryRecordSize;
        ple<u16> VersionMadeBy;
        ple<u16> VersionToExtract;
        ple<u32> DiskNumber;
        ple<u32> StartDiskNumber;
        ple<i64> EntriesOnDisk;
        ple<i64> EntriesInDirectory;
        ple<i64> DirectorySize;
        ple<i64> DirectoryOffset;
    };

    static_assert(sizeof(ZIP64ENDLOCATORRECORD) == 0x38);

    struct ZIPDIRENTRY
    {
        ple<u32> Signature; // 0x02014B50
        ple<u16> VersionMadeBy;
        ple<u16> VersionToExtract;
        ple<u16> Flags;
        ple<u16> Compression;
        ple<u16> FileTime;
        ple<u16> FileDate;
        ple<u32> CRC;
        ple<u32> CompressedSize;
        ple<u32> UncompressedSize;
        ple<u16> FileNameLength;
        ple<u16> ExtraFieldLength;
        ple<u16> FileCommentLength;
        ple<u16> DiskNumberStart;
        ple<u16> InternalAttributes;
        ple<u32> ExternalAttributes;
        ple<u32> HeaderOffset;
    };

    static_assert(sizeof(ZIPDIRENTRY) == 0x2E);

    struct ZIPFILERECORD
    {
        ple<u32> Signature; // 0x04034B50
        ple<u16> Version;
        ple<u16> Flags;
        ple<u16> Compression;
        ple<u16> FileTime;
        ple<u16> FileDate;
        ple<u32> CRC;
        ple<u32> CompressedSize;
        ple<u32> UncompressedSize;
        ple<u16> FileNameLength;
        ple<u16> ExtraFieldLength;
    };

    static_assert(sizeof(ZIPFILERECORD) == 0x1E);

    struct ZIPEXTRAFIELD
    {
        ple<u16> HeaderId;
        ple<u16> DataSize;
    };

    static_assert(sizeof(ZIPEXTRAFIELD) == 0x4);

    static i64 FindEndOfCentralDirectory(Rc<Stream> input)
    {
        i64 const size = input->Size();

        if (size < sizeof(ZIPENDLOCATOR))
            return -1;

        i64 here = size - sizeof(ZIPENDLOCATOR); // Highest possible EOCD offset

        { // Fast path for files with no comment
            ZIPENDLOCATOR eocd;

            if (!input->TryReadBulk(&eocd, sizeof(eocd), here))
                return -1;

            if (eocd.Signature == 0x06054B50)
                return here;
        }

        u8 buffer[4096 + 3];

        for (i64 const lower = std::max<i64>(0, here - 0xFFFF); here > lower;)
        {
            usize len = static_cast<usize>(std::min<i64>(here - lower, sizeof(buffer) - 3));

            here -= len;

            if (!input->TryReadBulk(buffer, len + 3, here))
                break;

            for (usize j = len; j--;)
            {
                if ((buffer[j + 0] == 0x50) && (buffer[j + 1] == 0x4B) && (buffer[j + 2] == 0x05) &&
                    (buffer[j + 3] == 0x06))
                    return here + j;
            }
        }

        return -1;
    }

    static i64 FindEndOfCentralDirectory64(Rc<Stream> input, i64 eocd_offset)
    {
        if (eocd_offset < sizeof(ZIP64ENDLOCATOR) + sizeof(ZIP64ENDLOCATORRECORD))
            return -1;

        ZIP64ENDLOCATOR eocd64;

        if (!input->TryReadBulk(&eocd64, sizeof(eocd64), eocd_offset - sizeof(eocd64)))
            return -1;

        if (eocd64.Signature != 0x07064B50)
            return -1;

        return eocd64.DirectoryOffset;
    }
} // namespace Swage::Zip

namespace Swage
{
    using namespace Zip;

    struct ZipFileEntry : ArchiveFile
    {
        u64 RecordOffset;
    };

    class ZipArchive final : public FileOperationsT<ZipFileEntry>
    {
    public:
        ZipArchive(Rc<Stream> input)
            : Input(std::move(input))
        {}

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;

        Rc<Stream> Input {};
    };

    Rc<Stream> ZipArchive::Open(File& file)
    {
        ZipFileEntry& node = GetData(file);

        // FIXME: This is not thread-safe
        if (node.Offset == UINT64_MAX)
        {
            ZIPFILERECORD record;

            if (!Input->TryReadBulk(&record, sizeof(record), node.RecordOffset))
                throw std::runtime_error("Failed to read zip file record");

            if (record.Signature != 0x04034B50)
                throw std::runtime_error("Invalid zip file record");

            node.Offset = node.RecordOffset + sizeof(record) + record.FileNameLength + record.ExtraFieldLength;
        }

        return node.Open(Input);
    }

    void ZipArchive::Stat(File& file, FolderEntry& entry)
    {
        ZipFileEntry& node = GetData(file);

        entry.Size = node.Size;
    }

    Rc<FileDevice> LoadZip(Rc<Stream> input)
    {
        i64 eocd_offset = FindEndOfCentralDirectory(input);

        if (eocd_offset == -1)
            throw std::runtime_error("Failed to find end of central directory");

        i64 cd_offset = -1;

        if (i64 eocd64_offset = FindEndOfCentralDirectory64(input, eocd_offset); eocd64_offset != -1)
        {
            ZIP64ENDLOCATORRECORD eocd64;

            if (!input->TryReadBulk(&eocd64, sizeof(eocd64), eocd64_offset))
                throw std::runtime_error("Failed to read ZIP64ENDLOCATORRECORD");

            if (eocd64.Signature != 0x06064B50)
                throw std::runtime_error("Invalid ZIP64ENDLOCATORRECORD Signature");

            if (eocd64.DiskNumber != eocd64.StartDiskNumber)
                throw std::runtime_error("Multi-disk zips not supported");

            if (eocd64.DirectoryRecordSize < sizeof(eocd64) - 12)
                throw std::runtime_error("Invalid ZIP64ENDLOCATORRECORD DirectoryRecordSize");

            cd_offset = eocd64.DirectoryOffset;
        }
        else
        {
            ZIPENDLOCATOR eocd;

            if (!input->TryReadBulk(&eocd, sizeof(eocd), eocd_offset))
                throw std::runtime_error("Failed to read ZIPENDLOCATOR");

            if (eocd.Signature != 0x06054B50)
                throw std::runtime_error("Invalid ZIPENDLOCATOR Signature");

            if (eocd.DiskNumber != eocd.StartDiskNumber)
                throw std::runtime_error("Multi-disk zips not supported");

            cd_offset = eocd.DirectoryOffset;
        }

        BufferedStream stream(input);

        if (!stream.TrySeek(cd_offset))
            throw std::runtime_error("Failed to seek to central directory");

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<ZipArchive> fops = MakeRc<ZipArchive>(std::move(input));

        String name;

        while (true)
        {
            ZIPDIRENTRY entry;

            if (!stream.TryRead(&entry, sizeof(entry)) || entry.Signature != 0x02014B50)
                break;

            // TODO: Handle Zip64 Files (CompressedSize > 0xFFFFFFFF || UncompresedSize > 0xFFFFFFFF)

            if (entry.FileNameLength)
            {
                name.resize(entry.FileNameLength);

                if (!stream.TryRead(name.data(), ByteSize(name)))
                    break;

                if (name.back() != '/' && name.back() != '\\')
                {
                    PathNormalizeSlash(name);

                    ArchiveFile info;

                    if (entry.Compression == 8)
                        info = ArchiveFile::Deflated(UINT64_MAX, entry.UncompressedSize, entry.CompressedSize, -15);
                    else
                        info = ArchiveFile::Stored(UINT64_MAX, entry.UncompressedSize);

                    fops->AddFile(device->Files, name, info, entry.HeaderOffset);
                }
            }

            stream.Seek(entry.ExtraFieldLength + entry.FileCommentLength, SeekWhence::Cur);
        }

        return device;
    }
} // namespace Swage
