#include "pak.h"

#include "asset/device/archive.h"
#include "asset/stream/buffered.h"
#include "core/bits.h"

namespace Swage::Bohemia
{
    struct FormHeader
    {
        u32 Magic;
        u32 FileSize;
        u32 FileType;
    };

    static_assert(sizeof(FormHeader) == 0xC);

    struct FormItem
    {
        u32 Type;
        u32 Size;
    };

    static_assert(sizeof(FormItem) == 0x8);

    struct FormItem_HEAD
    {
        u32 PackItemVersion;
        u32 field_4;
        u32 field_8;
        u32 field_C;
        u32 field_10;
        u32 field_14;
        u32 field_18;
    };

    static_assert(sizeof(FormItem_HEAD) == 0x1C);

    struct PackItem
    {
        u8 Type;
        u8 NameLength;
    };

    static_assert(sizeof(PackItem) == 0x2);

    struct PackItemDirectory
    {
        u32 NumItems;
    };

    struct PackItemFile_10003
    {
        u32 Offset;
        u32 DiskSize;
        u64 Size;
        u16 field_10;
        u8 CompType;
        u8 field_13;
        u32 TimeStamp;
    };

    static_assert(sizeof(PackItemFile_10003) == 0x18);

    class PakArchive final : public FileArchive
    {
    public:
        PakArchive(Rc<Stream> input);

        void AddToVFS(VFS& vfs, BufferedStream& stream, FormItem_HEAD& header, String& path, u32 num_items);
    };

    PakArchive::PakArchive(Rc<Stream> input)
        : FileArchive(std::move(input))
    {}

    void PakArchive::AddToVFS(VFS& vfs, BufferedStream& stream, FormItem_HEAD& header, String& path, u32 num_items)
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
                ArchiveFile info;

                switch (header.PackItemVersion)
                {
                    case 0x10003: {
                        PackItemFile_10003 file;

                        if (!stream.TryRead(&file, sizeof(file)))
                            throw std::runtime_error("Failed to read file");

                        if (file.CompType == 1)
                            info = ArchiveFile::Deflated(file.Offset, file.Size, file.DiskSize, 15);
                        else
                            info = ArchiveFile::Stored(file.Offset, file.Size);

                        break;
                    }

                    default: {
                        throw std::runtime_error("Invalid pack item version");
                    }
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

        FormHeader header;

        if (!stream.TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        bits::bswapv(header.Magic, header.FileSize, header.FileType);

        if (header.Magic != 0x464F524D) // FORM
            throw std::runtime_error("Invalid header magic");

        if (header.FileType != 0x50414331) // PAC1
            throw std::runtime_error("Invalid header file type");

        u64 next_item = stream.Tell();
        FormItem_HEAD head {};

        while (true)
        {
            FormItem item;

            if (!stream.TrySeek(next_item) || !stream.TryRead(&item, sizeof(item)))
                break;

            bits::bswapv(item.Type, item.Size);
            next_item = stream.Tell() + item.Size;

            switch (item.Type)
            {
                case 0x48454144: // HEAD
                {
                    // A header for the PAC1 file
                    if (!stream.TryRead(&head, sizeof(head)))
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
                    fops->AddToVFS(device->Files, stream, head, path, 1);

                    return device;
                }
            }
        }

        return nullptr;
    }
} // namespace Swage::Bohemia
