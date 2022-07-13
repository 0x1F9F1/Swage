#include "rpf6.h"

#include "asset/stream.h"
#include "core/bits.h"
#include "crypto/aes.h"
#include "crypto/secret.h"

namespace Swage::Rage
{
    static_assert(is_c_struct_v<fiPackHeader6, 0x10>);
    static_assert(is_c_struct_v<fiPackEntry6, 0x14>);

    static const char* DEFAULT_KEY_HASH = "AOHXWtl`i|@X1feM%MjV";
    static const char* RDR1_KEY_HASH = "AOHXWQNAqEHkQ0^;=)oc";

    class fiPackfile6 final : public FileOperationsT<fiPackEntry6>
    {
    public:
        fiPackfile6(Rc<Stream> input, fiPackHeader6 header)
            : Input(std::move(input))
            , Header(header)
        {}

        Rc<Stream> Input {};
        fiPackHeader6 Header {};

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;

        void AddToVFS(VFS& vfs, const Vec<fiPackEntry6>& entries, String& path, const fiPackEntry6& directory);
    };

    Rc<Stream> fiPackfile6::Open(File& file)
    {
        const fiPackEntry6& entry = GetData(file);

        u32 size = entry.GetSize();
        u32 raw_size = entry.GetOnDiskSize();
        u64 offset = entry.GetOffset();
        i32 comp_type = 0;

        offset <<= 3;

        if (entry.IsResource())
        {
            u32 rsc_size = entry.HasExtendedFlags() ? 0x10 : 0xC;

            if (raw_size < rsc_size)
                throw std::runtime_error("Resource raw size is too small");

            offset += rsc_size;
            raw_size -= rsc_size;

            // TODO: Handle LZXD compression
            if (entry.IsCompressed())
                comp_type = 15;
        }
        else
        {
            if (entry.IsCompressed())
                comp_type = -15;
        }

        ArchiveFile result = comp_type ? ArchiveFile::Deflated(offset, size, raw_size, comp_type)
                                       : ArchiveFile::Stored(offset, raw_size);

        return result.Open(Input);
    }

    void fiPackfile6::Stat(File& file, FolderEntry& output)
    {
        const fiPackEntry6& entry = GetData(file);

        output.Size = entry.GetSize();
    }

    void fiPackfile6::AddToVFS(VFS& vfs, const Vec<fiPackEntry6>& entries, String& path, const fiPackEntry6& directory)
    {
        for (u32 i = directory.GetEntryIndex(), end = i + directory.GetEntryCount(); i < end; ++i)
        {
            usize old_size = path.size();

            const fiPackEntry6& entry = entries.at(i);

            fmt::format_to(std::back_inserter(path), "${:08X}", entry.GetHash());

            if (entry.IsDirectory())
            {
                path += '/';

                AddToVFS(vfs, entries, path, entry);
            }
            else
            {
                AddFile(vfs, path, entry);
            }

            path.resize(old_size);
        }
    }

    Rc<VirtualFileDevice> LoadRPF6(Rc<Stream> input)
    {
        fiPackHeader6 header;

        if (!input->Rewind() || !input->TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x36465052)
            throw std::runtime_error("Invalid header magic");

        bits::bswapv(header.Magic, header.EntryCount, header.NamesOffset, header.DecryptionTag);

        // Decryption cannot be performed in-place as the size has to be aligned to 16 bytes.
        usize entries_size = (header.EntryCount * sizeof(fiPackEntry6) + 0xF) & ~u32(0xF);
        Ptr<u8[]> entries_data(new u8[entries_size]);

        if (!input->TryRead(entries_data.get(), entries_size))
            throw std::runtime_error("Failed to read entries");

        if (header.DecryptionTag)
        {
            const char* key_hash = nullptr;

            switch (header.DecryptionTag)
            {
                case 0xFFFFFFFF: key_hash = DEFAULT_KEY_HASH; break;
                case 0xFFFFFFFD: key_hash = RDR1_KEY_HASH; break;
            }

            if (key_hash == nullptr)
                throw std::runtime_error(fmt::format("Unknown decryption tag 0x{:08X}", header.DecryptionTag));

            u8 key[32];

            if (!Secrets.Get(key_hash, key, sizeof(key)))
                throw std::runtime_error(
                    fmt::format("Missing decryption tag 0x{:08X} ({})", header.DecryptionTag, key_hash));

            AesEcbCipher cipher(key, sizeof(key), true);

            for (usize i = 0; i < 16; ++i)
                cipher.Cipher::Update(entries_data.get(), entries_size);
        }

        Vec<fiPackEntry6> entries(header.EntryCount);

        if (ByteSize(entries) > entries_size)
            throw std::runtime_error("Invalid entry count");

        std::memcpy(entries.data(), entries_data.get(), ByteSize(entries));

        for (fiPackEntry6& entry : entries)
            bits::bswapv(entry.dword0, entry.dword4, entry.dword8, entry.dwordC, entry.dword10);

        // rage::fiPackfile::FindEntry assumes the first entry is a directory (and ignores its name)
        const fiPackEntry6& root = entries.at(0);

        if (!root.IsDirectory())
            throw std::runtime_error("Root entry is not a directory");

        Rc<VirtualFileDevice> device = swref VirtualFileDevice();
        Rc<fiPackfile6> fops = swref fiPackfile6(std::move(input), header);

        String path;
        fops->AddToVFS(device->Files, entries, path, root);

        return device;
    }
} // namespace Swage::Rage
