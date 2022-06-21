#include "pak.h"

#include "asset/device/archive.h"
#include "asset/stream/buffered.h"
#include "core/bits.h"

namespace Swage::Bohemia
{
    using namespace bits;

    struct IFF_Chunk
    {
        be<u32> ID;
        be<u32> Size;
    };

    static_assert(sizeof(IFF_Chunk) == 0x8);

    struct IFF_Form : IFF_Chunk
    {
        be<u32> FormType;
    };

    static_assert(sizeof(IFF_Form) == 0xC);

    struct PackHeader
    {
        le<u32> PackItemVersion;
        le<u32> field_4;
        le<u32> field_8;
        le<u32> field_C;
        le<u32> field_10;
        le<u32> field_14;
        le<u32> field_18;
    };

    static_assert(sizeof(PackHeader) == 0x1C);

    struct PackItem
    {
        u8 Type;
        u8 NameLength;
    };

    static_assert(sizeof(PackItem) == 0x2);

    struct PackItemDirectory
    {
        le<u32> NumItems;
    };

    static_assert(sizeof(PackItemDirectory) == 0x4);

    struct PackItemFile
    {
        ple<u32> Offset;
        ple<u32> DiskSize;
        ple<u32> Size;
        ple<u32> field_C;
        ple<u16> field_10;
        ple<u8> CompFlags1; // 2 bits
        ple<u8> CompFlags2; // 4 bits
    };

    static_assert(sizeof(PackItemFile) == 0x14);

    struct PackItemFile_10000 : PackItemFile
    {};

    static_assert(sizeof(PackItemFile_10000) == 0x14);

    struct PackItemFile_10001 : PackItemFile_10000
    {
        u8 TimeStamp[7];
    };

    static_assert(sizeof(PackItemFile_10001) == 0x1B);

    struct PackItemFile_10002 : PackItemFile_10000
    {
        ple<u32> TimeStamp;
    };

    static_assert(sizeof(PackItemFile_10002) == 0x18);

    struct PackItemFile_10003 : PackItemFile_10000
    {
        ple<u32> TimeStamp;
    };

    static_assert(sizeof(PackItemFile_10003) == 0x18);

    class PakArchive final : public FileArchive
    {
    public:
        PakArchive(Rc<Stream> input);

        void AddToVFS(VFS& vfs, BufferedStream& stream, PackHeader& header, String& path, u32 num_items);
    };

    PakArchive::PakArchive(Rc<Stream> input)
        : FileArchive(std::move(input))
    {}

    void PakArchive::AddToVFS(VFS& vfs, BufferedStream& stream, PackHeader& header, String& path, u32 num_items)
    {
        for (u32 i = 0; i < num_items; ++i)
        {
            usize old_size = path.size();

            PackItem item;
            if (!stream.TryRead(&item, sizeof(item)))
                throw std::runtime_error("Failed to read item");

            path.resize(old_size + item.NameLength);
            if (!stream.TryRead(path.data() + old_size, item.NameLength))
                throw std::runtime_error("Failed to read item name");

            if (item.Type == 0) // Directory
            {
                PackItemDirectory dir;
                if (!stream.TryRead(&dir, sizeof(dir)))
                    throw std::runtime_error("Failed to read directory");

                if (!path.empty())
                    path += '/';

                AddToVFS(vfs, stream, header, path, dir.NumItems);
            }
            else if (item.Type == 1) // File
            {
                PackItemFile file;

                switch (header.PackItemVersion)
                {
                    case 0x10000: {
                        PackItemFile_10000 file_10000;

                        if (!stream.TryRead(&file_10000, sizeof(file_10000)))
                            throw std::runtime_error("Failed to read file header");

                        file = file_10000;
                        break;
                    }

                    case 0x10001: {
                        PackItemFile_10001 file_10001;

                        if (!stream.TryRead(&file_10001, sizeof(file_10001)))
                            throw std::runtime_error("Failed to read file header");

                        file = file_10001;
                        break;
                    }

                    case 0x10002: {
                        PackItemFile_10003 file_10002;

                        if (!stream.TryRead(&file_10002, sizeof(file_10002)))
                            throw std::runtime_error("Failed to read file header");

                        file = file_10002;
                        break;
                    }

                    case 0x10003: {
                        PackItemFile_10003 file_10003;

                        if (!stream.TryRead(&file_10003, sizeof(file_10003)))
                            throw std::runtime_error("Failed to read file header");

                        file = file_10003;
                        break;
                    }

                    default: {
                        throw std::runtime_error("Invalid pack item version");
                    }
                }

                ArchiveFile info;

                if (file.CompFlags2 != 0)
                {
                    if (file.CompFlags1 == 1)
                        info = ArchiveFile::Deflated(file.Offset, file.Size, file.DiskSize, 15);
                    else
                        info = ArchiveFile::Stored(file.Offset, file.Size); // TODO: Handle other compressors?
                }
                else
                {
                    info = ArchiveFile::Stored(file.Offset, file.Size);
                }

                AddFile(vfs, path, info);
            }

            path.resize(old_size);
        }
    }

    Rc<FileDevice> LoadPAK(Rc<Stream> input)
    {
        BufferedStream stream(input);
        stream.Rewind();

        IFF_Form header;

        if (!stream.TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        if (header.ID != 0x464F524D) // FORM
            throw std::runtime_error("Invalid header magic");

        if (header.FormType != 0x50414331) // PAC1
            throw std::runtime_error("Invalid header file type");

        u64 next_item = stream.Tell();
        PackHeader pack_header {};

        while (true)
        {
            IFF_Chunk item;

            if (!stream.TrySeek(next_item) || !stream.TryRead(&item, sizeof(item)))
                break;

            next_item = stream.Tell() + item.Size;

            switch (item.ID)
            {
                case 0x48454144: // HEAD
                {
                    // A header for the PAC1 file
                    if (!stream.TryRead(&pack_header, sizeof(pack_header)))
                        throw std::runtime_error("Failed to read HEAD");

                    break;
                }

                case 0x44415441: // DATA
                {
                    // All of the actual file data

                    break;
                }

                case 0x46494C45: // FILE
                {
                    // A VFS tree

                    Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
                    Rc<PakArchive> fops = MakeRc<PakArchive>(input);

                    String path;
                    fops->AddToVFS(device->Files, stream, pack_header, path, 1);

                    return device;
                }
            }
        }

        return nullptr;
    }
} // namespace Swage::Bohemia
