#include "rpf2.h"

#include "asset/stream.h"

#include "crypto/aes.h"

#include "crypto/secret.h"

namespace Swage::Rage
{
    static_assert(is_c_struct_v<fiPackHeader2, 0x18>);
    static_assert(is_c_struct_v<fiPackEntry2, 0x10>);

    static const char* GTA4_PC_KEY_HASH = "AOHXWtl`i|@X1feM%MjV";

    class fiPackfile2 final : public FileOperationsT<fiPackEntry2>
    {
    public:
        fiPackfile2(Rc<Stream> input, fiPackHeader2 header)
            : Input(std::move(input))
            , Header(header)
        {}

        Rc<Stream> Input {};
        fiPackHeader2 Header {};

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;

        void AddToVFS(VFS& vfs, const Vec<fiPackEntry2>& entries, const Vec<char>& names, String& path,
            const fiPackEntry2& directory);
    };

    Rc<Stream> fiPackfile2::Open(File& file)
    {
        const fiPackEntry2& entry = GetData(file);

        u32 size = entry.GetSize();
        u32 disk_size = entry.GetOnDiskSize();
        u32 offset = entry.GetOffset();
        i32 comp_type = 0;

        if (entry.IsResource())
        {
            if (disk_size < 0xC)
                throw std::runtime_error("Resource raw size is too small");

            offset += 0xC;
            disk_size -= 0xC;

            if (entry.IsCompressed())
                comp_type = 15;
        }
        else
        {
            if (entry.IsCompressed())
                comp_type = -15;
        }

        // TODO: Handle Header.FileDecryptionTag
        // TODO: Handle LZX compression

        ArchiveFile result = comp_type ? ArchiveFile::Deflated(offset, size, disk_size, comp_type)
                                       : ArchiveFile::Stored(offset, disk_size);

        return result.Open(Input);
    }

    void fiPackfile2::Stat(File& file, FolderEntry& output)
    {
        const fiPackEntry2& entry = GetData(file);

        output.Size = entry.GetSize();
    }

    void fiPackfile2::AddToVFS(
        VFS& vfs, const Vec<fiPackEntry2>& entries, const Vec<char>& names, String& path, const fiPackEntry2& directory)
    {
        for (u32 i = directory.GetEntryIndex(), end = i + directory.GetEntryCount(); i < end; ++i)
        {
            usize old_size = path.size();

            const fiPackEntry2& entry = entries.at(i);
            path += SubCString(names, entry.GetNameOffset());

            if (entry.IsDirectory())
            {
                path += '/';

                AddToVFS(vfs, entries, names, path, entry);
            }
            else
            {
                AddFile(vfs, path, entry);
            }

            path.resize(old_size);
        }
    }

    Rc<VirtualFileDevice> LoadRPF2(Rc<Stream> input)
    {
        constexpr u64 TOC_OFFSET = 0x800;

        fiPackHeader2 header;

        if (!input->TryReadBulk(&header, sizeof(header), 0))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x32465052)
            throw std::runtime_error("Invalid header magic");

        usize entries_size = header.EntryCount * sizeof(fiPackEntry2);

        if (header.HeaderSize <= entries_size)
            throw std::runtime_error("Invalid header size");

        Vec<fiPackEntry2> entries(header.EntryCount);

        if (!input->TryReadBulk(entries.data(), ByteSize(entries), TOC_OFFSET))
            throw std::runtime_error("Failed to read entries");

        Vec<char> names(header.HeaderSize - entries_size);

        if (!input->TryReadBulk(names.data(), ByteSize(names), TOC_OFFSET + entries_size))
            throw std::runtime_error("Failed to read names");

        if (header.HeaderDecryptionTag)
        {
            u8 key[32];

            if (!Secrets.Get(GTA4_PC_KEY_HASH, key, sizeof(key)))
                throw std::runtime_error("Missing cipher key");

            AesEcbCipher cipher(key, sizeof(key), true);

            for (usize i = 0; i < 16; ++i)
            {
                cipher.Cipher::Update(entries.data(), ByteSize(entries));
                cipher.Cipher::Update(names.data(), ByteSize(names));
            }
        }

        // rage::fiPackfile::FindEntry assumes the first entry is a directory (and ignores its name)
        const fiPackEntry2& root = entries.at(0);

        if (!root.IsDirectory())
            throw std::runtime_error("Root entry is not a directory");

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<fiPackfile2> fops = MakeRc<fiPackfile2>(std::move(input), header);

        String path;
        fops->AddToVFS(device->Files, entries, names, path, root);

        return device;
    }
} // namespace Swage::Rage
