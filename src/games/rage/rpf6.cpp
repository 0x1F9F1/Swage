#include "rpf6.h"

#include "asset/device/virtual.h"
#include "asset/stream.h"
#include "asset/stream/decode.h"
#include "asset/stream/partial.h"
#include "asset/transform/deflate.h"
#include "asset/transform/lzxd.h"
#include "asset/transform/zstd.h"
#include "core/bits.h"
#include "crypto/secret.h"

#include "cipher16.h"

namespace Swage::Rage::RPF6
{
    static const char* DEFAULT_KEY_HASH = "AOHXWtl`i|@X1feM%MjV";
    static const char* RDR1_KEY_HASH = "AOHXWQNAqEHkQ0^;=)oc";

    static Ptr<Cipher> MakeCipher(const fiPackHeader6& header)
    {
        const char* key_hash = nullptr;

        switch (header.DecryptionTag)
        {
            case 0xFFFFFFFF: key_hash = DEFAULT_KEY_HASH; break;
            case 0xFFFFFFFD: key_hash = RDR1_KEY_HASH; break;
            default: return nullptr;
        }

        u8 key[32];

        if (!Secrets.Get(key_hash, key, sizeof(key)))
            return nullptr;

        return swnew AesEcbCipher16(key, sizeof(key), true);
    }
} // namespace Swage::Rage::RPF6

namespace Swage::Rage
{
    static_assert(is_c_struct_v<fiPackHeader6, 0x10>);
    static_assert(is_c_struct_v<fiPackEntry6, 0x14>);

    struct fiPackEntryDebug6
    {
        u32 NameOffset;
        u32 LastModified;

        u64 /*time_t */ GetLastModified() const
        {
            if (LastModified == 0)
                return 0;

            return static_cast<u64>(LastModified) + 12591158400 - 11644473600;
        }
    };

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

        void AddToVFS(VFS& vfs, const Vec<fiPackEntry6>& entries, String& path, const fiPackEntry6& directory,
            const Callback<void(String& path, u32 index)>& get_name);
    };

    Rc<Stream> fiPackfile6::Open(File& file)
    {
        const fiPackEntry6& entry = GetData(file);

        u32 size = entry.GetSize();
        u32 raw_size = entry.GetOnDiskSize();
        u64 offset = entry.GetOffset();

        Ptr<BinaryTransform> transform;

        if (entry.IsResource())
        {
            u32 rsc_size = entry.HasExtendedFlags() ? 0x10 : 0xC;

            if (raw_size < rsc_size)
                throw std::runtime_error("Resource raw size is too small (RSC header)");

            offset += rsc_size;
            raw_size -= rsc_size;

            if (entry.IsCompressed())
            {
                u8 magic[4];

                if (!Input->TryReadBulk(&magic, sizeof(magic), offset))
                    std::memset(magic, 0, sizeof(magic));

                if ((magic[3] == 0xFD) && (magic[2] == 0x2F) && (magic[1] == 0xB5) && ((magic[0] & 0xF0) == 0x20))
                {
                    transform = CreateZstdDecompressor();
                }
                else if ((magic[0] == 0x0F) && (magic[1] == 0xF5) && (magic[2] == 0x12) && (magic[3] == 0xF1))
                {
                    if (raw_size < 8)
                        throw std::runtime_error("Resource raw size is too small (LZXD header)");

                    offset += 8;
                    raw_size -= 8;

                    // bits::be<u32> lzxd_raw_size;
                    // SwAssert(Input->TryReadBulk(&lzxd_raw_size, sizeof(lzxd_raw_size), offset - 4) &&
                    //     (raw_size == lzxd_raw_size));

                    transform = CreateLzxdDecompressor(0, 0);
                }
                else if ((magic[0] == 0x78) && (magic[1] == 0xDA))
                {
                    transform = CreateDeflateDecompressor(15);
                }
                else
                {
                    SwLogError("Unrecognized file compression: {:02X} {:02X} {:02X} {:02X}", magic[0], magic[1],
                        magic[2], magic[3]);
                }
            }
        }
        else if (entry.IsCompressed())
        {
            u8 magic[4];

            if (!Input->TryReadBulk(&magic, sizeof(magic), offset))
                std::memset(magic, 0, sizeof(magic));

            if ((magic[3] == 0xFD) && (magic[2] == 0x2F) && (magic[1] == 0xB5) && ((magic[0] & 0xF0) == 0x20))
            {
                transform = CreateZstdDecompressor();
            }
            else
            {
                transform = CreateDeflateDecompressor(-15);
            }
        }

        Rc<Stream> result = swref PartialStream(offset, raw_size, Input);

        if (transform)
            result = swref DecodeStream(std::move(result), std::move(transform), size);

        return result;
    }

    void fiPackfile6::Stat(File& file, FolderEntry& output)
    {
        const fiPackEntry6& entry = GetData(file);

        output.Size = entry.GetSize();
    }

    void fiPackfile6::AddToVFS(VFS& vfs, const Vec<fiPackEntry6>& entries, String& path, const fiPackEntry6& directory,
        const Callback<void(String& path, u32 index)>& get_name)
    {
        for (u32 i = directory.GetEntryIndex(), end = i + directory.GetEntryCount(); i < end; ++i)
        {
            usize old_size = path.size();

            get_name(path, i);

            const fiPackEntry6& entry = entries.at(i);

            if (entry.IsDirectory())
            {
                path += '/';

                AddToVFS(vfs, entries, path, entry, get_name);
            }
            else
            {
                AddFile(vfs, path, entry);
            }

            path.resize(old_size);
        }
    }

    Rc<FileDevice> LoadRPF6(Rc<Stream> input)
    {
        fiPackHeader6 header;

        if (!input->Rewind() || !input->TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x36465052)
            throw std::runtime_error("Invalid header magic");

        bits::bswapv(header.Magic, header.EntryCount, header.DebugDataOffset, header.DecryptionTag);

        // Decryption cannot be performed in-place as the size has to be aligned to 16 bytes.
        Vec<u8> raw_entries((header.EntryCount * sizeof(fiPackEntry6) + 0xF) & ~usize(0xF));

        if (!input->TryRead(raw_entries.data(), ByteSize(raw_entries)))
            throw std::runtime_error("Failed to read entries");

        Vec<u8> debug_data;
        i64 debug_data_offset = static_cast<i64>(header.DebugDataOffset) * 8;

        if (debug_data_offset)
        {
            debug_data.resize(static_cast<usize>(input->Size() - debug_data_offset));

            if (!input->TryReadBulk(debug_data.data(), ByteSize(debug_data), debug_data_offset))
                throw std::runtime_error("Failed to read data");
        }

        if (header.DecryptionTag)
        {
            Ptr<Cipher> cipher = RPF6::MakeCipher(header);

            if (!cipher)
                throw std::runtime_error(
                    fmt::format("Unknown header encryption 0x{:08X} (or missing key)", header.DecryptionTag));

            cipher->Update(raw_entries.data(), ByteSize(raw_entries));

            if (debug_data_offset)
                cipher->Update(debug_data.data(), ByteSize(debug_data) & ~0xF);
        }

        Vec<fiPackEntry6> entries(header.EntryCount);

        if (ByteSize(raw_entries) < ByteSize(entries))
            throw std::runtime_error("Invalid entry count");

        std::memcpy(entries.data(), raw_entries.data(), ByteSize(entries));

        for (fiPackEntry6& entry : entries)
            bits::bswapv(entry.dword0, entry.dword4, entry.dword8, entry.dwordC, entry.dword10);

        Vec<fiPackEntryDebug6> debug_entries;
        Vec<char> debug_names;

        if (debug_data_offset)
        {
            debug_entries.resize(header.EntryCount);

            if (ByteSize(debug_data) < ByteSize(debug_entries))
                throw std::runtime_error("Invalid entry count");

            debug_names.resize(ByteSize(debug_data) - ByteSize(debug_entries));

            std::memcpy(debug_entries.data(), debug_data.data(), ByteSize(debug_entries));
            std::memcpy(debug_names.data(), debug_data.data() + ByteSize(debug_entries), ByteSize(debug_names));

            for (fiPackEntryDebug6& entry : debug_entries)
                bits::bswapv(entry.NameOffset, entry.LastModified);
        }

        // rage::fiPackfile::FindEntry assumes the first entry is a directory (and ignores its name)
        const fiPackEntry6& root = entries.at(0);

        if (!root.IsDirectory())
            throw std::runtime_error("Root entry is not a directory");

        Rc<VirtualFileDevice> device = swref VirtualFileDevice();
        Rc<fiPackfile6> fops = swref fiPackfile6(std::move(input), header);

        const auto get_name = [&](String& path, usize index) {
            if (debug_data_offset)
            {
                const fiPackEntryDebug6& debug_entry = debug_entries.at(index);

                path += SubCString(debug_names, debug_entry.NameOffset);
            }
            else
            {
                const fiPackEntry6& entry = entries.at(index);

                // TODO: Dehash names
                fmt::format_to(std::back_inserter(path), "${:08X}", entry.GetHash());
            }
        };

        String path;
        fops->AddToVFS(device->Files, entries, path, root, get_name);

        return device;
    }
} // namespace Swage::Rage
