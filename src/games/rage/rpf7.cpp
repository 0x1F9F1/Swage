#include "rpf7.h"

#include "asset/device/virtual.h"

#include "asset/stream/buffered.h"
#include "asset/stream/decode.h"
#include "asset/stream/ecb.h"
#include "asset/stream/memory.h"
#include "asset/stream/partial.h"

#include "asset/transform/deflate.h"
#include "asset/transform/lzxd.h"

#include "core/bits.h"

#include "crypto/aes.h"
#include "crypto/secret.h"
#include "crypto/tfit.h"

#include "rsc.h"
#include "utils.h"

#include <stdexcept>

namespace Swage::Rage::RPF7
{
#include "hashes/GTA5.h"

    static const char* GTA5_PS3_KEY_HASH = "AOHXW_ffoV0?kI{q=88_";
    static const char* GTA5_360_KEY_HASH = "AOHXW4B1`n4eiV>==o;n";
    static const char* LAUNCHER_KEY_HASH = "AOHXWRw-qk@gxZ~7^V<L";
    static const char* RAGE_KEY_HASH = "AOHXWBq$eTk1<dNnZje@";

    static u32 GTA5_PC_TFIT_KEYS[101][20][4];
    static u32 GTA5_PC_TFIT_TABLES[17][16][256];
    static bool GTA5_PC_KEYS_LOADED;

    static u8 GTA5_PS4_AES_KEYS[101][32];
    static bool GTA5_PS4_KEYS_LOADED;

    static u8 GTA5_PS3_AES_KEY[32];
    static bool GTA5_PS3_KEY_LOADED;

    static u8 GTA5_360_AES_KEY[32];
    static bool GTA5_360_KEY_LOADED;

    static u8 LAUNCHER_AES_KEY[32];
    static bool LAUNCHER_KEY_LOADED;

    static u8 RAGE_AES_KEY[32];
    static bool RAGE_KEY_LOADED;

#define X(NAME, HASH)                                \
    {                                                \
        if (!Secrets.Get(HASH, &NAME, sizeof(NAME))) \
            return false;                            \
    }

    static bool LoadKeys_GTA5_PC()
    {
        for (usize i = 0; i < 101; ++i)
            X(GTA5_PC_TFIT_KEYS[i], GTA5_PC_KEY_HASHES[i]);

        for (usize i = 0; i < 17; ++i)
            for (usize j = 0; j < 16; ++j)
                X(GTA5_PC_TFIT_TABLES[i][j], GTA5_PC_TABLE_HASHES[i][j]);

        return true;
    }

    void FindKeys_GTA5_PC(SecretFinder& finder)
    {
        for (usize i = 0; i < 101; ++i)
            finder.Add(GTA5_PC_KEY_HASHES[i]);

        for (usize i = 0; i < 17; ++i)
            for (usize j = 0; j < 16; ++j)
                finder.Add(GTA5_PC_TABLE_HASHES[i][j]);
    }

    static bool LoadKeys_GTA5_PS4()
    {
        for (usize i = 0; i < 101; ++i)
            X(GTA5_PS4_AES_KEYS[i], GTA5_PS4_KEY_HASHES[i]);

        return true;
    }

#undef X

    void LoadKeys()
    {
        GTA5_PC_KEYS_LOADED = LoadKeys_GTA5_PC();
        GTA5_PS4_KEYS_LOADED = LoadKeys_GTA5_PS4();

#define X(NAME, HASH) Secrets.Get(HASH, &NAME, sizeof(NAME))

        GTA5_PS3_KEY_LOADED = X(GTA5_PS3_AES_KEY, GTA5_PS3_KEY_HASH);
        GTA5_360_KEY_LOADED = X(GTA5_360_AES_KEY, GTA5_360_KEY_HASH);
        LAUNCHER_KEY_LOADED = X(LAUNCHER_AES_KEY, LAUNCHER_KEY_HASH);
        RAGE_KEY_LOADED = X(RAGE_AES_KEY, RAGE_KEY_HASH);

#undef X
    }

    static Ptr<Cipher> MakeCipher(const fiPackHeader7& header, u32 key)
    {
        u32 tag = header.DecryptionTag;

        if (tag == 0 || tag == 0x4E45504F || tag == 0x50584643)
            return nullptr;

        u32 id = tag & 0xFFFFFFF;
        // bool sixteen_rounds = (tag >> 28) == 0xF; // Repeat decryption 16 times
        // bool is_tfit = (tag & 0xFF00000) == 0xFE00000;

        switch (id)
        {
            case 0xFEFFFFF: {
                if (!GTA5_PC_KEYS_LOADED)
                    throw std::runtime_error("GTA5 PC keys not loaded");

                SwAssert(key < 101);

                return swnew TfitEcbCipher(GTA5_PC_TFIT_KEYS[key] + 1, GTA5_PC_TFIT_TABLES);
            }

            case 0xFFEFFFF: {
                if (!GTA5_PS4_KEYS_LOADED)
                    throw std::runtime_error("GTA5 PS4 keys not loaded");

                SwAssert(key < 101);

                return swnew AesEcbCipher(GTA5_PS4_AES_KEYS[key], 32, true);
            }

            case 0xFFFFFF8: {
                if (!GTA5_PS3_KEY_LOADED)
                    throw std::runtime_error("GTA5 PS3 key not loaded");

                return swnew AesEcbCipher(GTA5_PS3_AES_KEY, 32, true);
            }

            case 0xFFFFFF7: {
                if (header.GetPlatformBit())
                {
                    if (!GTA5_360_KEY_LOADED)
                        throw std::runtime_error("GTA5 360 key not loaded");

                    return swnew AesEcbCipher(GTA5_360_AES_KEY, 32, true);
                }
                else
                {
                    if (!LAUNCHER_KEY_LOADED)
                        throw std::runtime_error("Launcher key not loaded");

                    return swnew AesEcbCipher(LAUNCHER_AES_KEY, 32, true);
                }
            }

            case 0xFFFFFF9: {
                if (!RAGE_KEY_LOADED)
                    throw std::runtime_error("Rage default key not loaded");

                return swnew AesEcbCipher(RAGE_AES_KEY, 32, true);
            }
        }

        throw std::runtime_error("Unknown cipher tag");
    }

    static u32 CalculateKeyIndex(StringView path, u64 file_size)
    {
        // rage::pgStreamer::Read
        // u32 hash = rage::atStringHash(rage::fiAssetManager::FileName(path), 0);
        auto [_, file] = SplitPath(path);
        u32 hash = atStringHash(file);

        // rage::pgStreamer::DeviceHandler::ProcessDecompression
        return static_cast<u32>(hash + file_size) % 101;
    }

    static bool BruteFindCipher(const fiPackHeader7& header, const fiPackEntry7& enc_root, Ptr<Cipher>& cipher)
    {
        // "Next Gen" encryption uses 101 keys, selected based on the file size and name.
        // If decryption failed, brute force through all the keys to see if one matches.
        // This is particularly useful for bgscripts
        // NOTE: This impl assumes the cipher uses ECB (no IV)

        // Check if the original cipher is correct
        {
            fiPackEntry7 root = enc_root;
            cipher->Update(&root, sizeof(root));

            if (root.IsDirectory())
                return true;
        }

        // Burte force through all of the keys
        for (u32 header_key = 0; header_key < 101; ++header_key)
        {
            if (cipher = RPF7::MakeCipher(header, header_key))
            {
                fiPackEntry7 root = enc_root;
                cipher->Update(&root, sizeof(root));

                if (root.IsDirectory())
                    return true;
            }
        }

        cipher = nullptr;
        return false;
    }
} // namespace Swage::Rage::RPF7

namespace Swage::Rage
{
    static_assert(is_c_struct_v<fiPackHeader7, 0x10>);
    static_assert(is_c_struct_v<fiPackEntry7, 0x10>);

    bool fiPackEntry7::GetResourceFileHeader(datResourceFileHeader& info) const
    {
        if (!IsResource())
            return false;

        info.Magic = 0x37435352;
        info.Flags = GetResourceVersion();

        info.ResourceInfo.VirtualFlags = GetVirtualFlags();
        info.ResourceInfo.PhysicalFlags = GetPhysicalFlags();

        return true;
    }

    static bool IsMaskedFileType(StringView path, StringView type)
    {
        auto index = path.rfind('.');
        if (index == StringView::npos)
            return false;

        auto ext = path.substr(index + 1);

        if (ext.size() != type.size())
            return false;

        for (usize i = 0; i < type.size(); ++i)
        {
            if (type[i] != '#' && ToLower(ext[i]) != ToLower(type[i]))
                return false;
        }

        return true;
    }

    class fiPackfile7 final : public FileOperationsT<fiPackEntry7>
    {
    public:
        fiPackfile7(Rc<Stream> input, fiPackHeader7 header, Vec<char> names);

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;
        bool Extension(File& file, const ExtensionData& data) override;

        StringView GetEntryName(const fiPackEntry7& entry);
        u64 GetEntrySize(const fiPackEntry7& entry);

        void AddToVFS(VFS& vfs, const Vec<fiPackEntry7>& entries, String& path, u32 index);

        Rc<Stream> Input {};

        fiPackHeader7 Header {};
        Vec<char> Names {};

        u32 VirtPageSize {};
        u32 PhysPageSize {};
    };

    fiPackfile7::fiPackfile7(Rc<Stream> input, fiPackHeader7 header, Vec<char> names)
        : Input(std::move(input))
        , Header(header)
        , Names(std::move(names))
    {
        if (Header.DecryptionTag == 0xFFFFFF8) // PS3
        {
            VirtPageSize = 0x1000;
            PhysPageSize = 0x1580;
        }
        else
        {
            VirtPageSize = 0x2000;
            PhysPageSize = 0x2000;
        }
    }

    StringView fiPackfile7::GetEntryName(const fiPackEntry7& entry)
    {
        return SubCString(Names, static_cast<usize>(entry.GetNameOffset()) << Header.GetNameShift());
    }

    u64 fiPackfile7::GetEntrySize(const fiPackEntry7& entry)
    {
        if (entry.IsResource())
        {
            u32 virt_flags = entry.GetVirtualFlags();
            u32 phys_flags = entry.GetPhysicalFlags();

            u64 virt_size = RSC7::GetResourceSize(virt_flags, VirtPageSize);
            u64 phys_size = RSC7::GetResourceSize(phys_flags, PhysPageSize);

            return virt_size + phys_size;
        }
        else
        {
            return entry.GetSize();
        }
    }

    Rc<Stream> fiPackfile7::Open(File& file)
    {
        const fiPackEntry7& entry = GetData(file);

        i64 offset = entry.GetOffset();
        i64 size = GetEntrySize(entry);
        i64 raw_size = entry.GetOnDiskSize();

        Ptr<Cipher> cipher;

        if (raw_size == 0)
        {
            if (entry.IsResource() || entry.GetDecryptionTag())
                throw std::runtime_error("Invalid raw resource");

            return swref PartialStream(offset, size, Input);
        }

        StringView name = GetEntryName(entry);
        i64 key_index = -1;

        if (entry.IsResource()) // rage::pgStreamer::Open, rage::pgStreamer::DeviceHandler::ProcessRequest
        {
            if (raw_size == 0xFFFFFF)
            {
                u8 rsc_header[0x10];

                if (!Input->TryReadBulk(rsc_header, sizeof(rsc_header), offset))
                    throw std::runtime_error("Failed to read large resource header");

                raw_size = RSC7::GetLargeResourceSize(rsc_header);
            }

            if (raw_size < 0x10)
                throw std::runtime_error("Resource raw size is too small");

            if (Header.DecryptionTag) // NOTE: entry.GetDecryptionTag() is only for non-resource files
            {
                // See CStreamedScripts::GetStreamerReadFlags -> rage::pgStreamer::Read (Flag 4) -> rage::pgStreamer::DeviceHandler::ProcessDecompression
                // if (entry.GetResourceVersion() == 9 || entry.GetResourceVersion() == 10 || entry.GetResourceVersion() == 11) // #SC (Scripts)
                if (IsMaskedFileType(name, "#sc"))
                    key_index = raw_size;
            }

            offset += 0x10;
            raw_size -= 0x10;
        }
        else
        {
            if (entry.GetDecryptionTag())
                key_index = size;
        }

        Rc<Stream> result = swref PartialStream(offset, raw_size, Input);

        if (key_index != -1)
            result = swref EcbCipherStream(
                std::move(result), RPF7::MakeCipher(Header, RPF7::CalculateKeyIndex(name, key_index)));

        if (Header.GetPlatformBit())
            result = swref DecodeStream(std::move(result), swnew LzxdDecompressor(64 * 1024, 256 * 1024), size);
        else
            result = swref DecodeStream(std::move(result), swnew DeflateDecompressor(-15), size);

        return result;
    }

    void fiPackfile7::Stat(File& file, FolderEntry& output)
    {
        const fiPackEntry7& entry = GetData(file);

        output.Size = GetEntrySize(entry);
    }

    bool fiPackfile7::Extension(File& file, const ExtensionData& data)
    {
        const fiPackEntry7& entry = GetData(file);

        if (datResourceFileHeader* header = data.As<datResourceFileHeader>())
            return entry.GetResourceFileHeader(*header);

        return false;
    }

    void fiPackfile7::AddToVFS(VFS& vfs, const Vec<fiPackEntry7>& entries, String& path, u32 index)
    {
        usize old_size = path.size();

        const fiPackEntry7& entry = entries.at(index);

        path += GetEntryName(entry);

        if (entry.IsDirectory())
        {
            if (index)
                path += '/';

            for (u32 i = entry.GetEntryIndex(), end = i + entry.GetEntryCount(); i < end; ++i)
                AddToVFS(vfs, entries, path, i);
        }
        else
        {
            AddFile(vfs, path, entry);
        }

        path.resize(old_size);
    }

    Rc<FileDevice> LoadRPF7(Rc<Stream> input, StringView name)
    {
        fiPackHeader7 header;

        if (!input->Rewind() || !input->TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        bool swap_endian = header.Magic == 0x37465052;

        if (swap_endian)
            bits::bswapv(header.Magic, header.EntryCount, header.dwordC, header.DecryptionTag);

        if (header.Magic != 0x52504637)
            throw std::runtime_error("Invalid header magic");

        if (header.EntryCount == 0)
            throw std::runtime_error("Archive has no entries");

        Vec<fiPackEntry7> entries(header.EntryCount);

        if (!input->TryRead(entries.data(), ByteSize(entries)))
            throw std::runtime_error("Failed to read entries");

        Vec<char> names(header.GetNamesLength());

        if (!input->TryRead(names.data(), ByteSize(names)))
            throw std::runtime_error("Failed to read names");

        u32 key_index = RPF7::CalculateKeyIndex(name, input->Size());

        if (Ptr<Cipher> cipher = RPF7::MakeCipher(header, key_index))
        {
            if (u32 tag = header.DecryptionTag; (tag == 0xFEFFFFF || tag == 0xFFEFFFF) && !swap_endian)
            {
                if (!RPF7::BruteFindCipher(header, entries.at(0), cipher))
                    throw std::runtime_error(fmt::format("Unknown header encryption 0x{:08x} (or missing key)", tag));
            }

            cipher->Update(entries.data(), ByteSize(entries));
            cipher->Update(names.data(), names.size());
        }

        for (fiPackEntry7& entry : entries)
        {
            if (swap_endian)
                bits::bswapv(entry.qword0, entry.dword8, entry.dwordC);

            if (!entry.IsDirectory() && !entry.IsResource() && entry.GetDecryptionTag() == 1)
                entry.SetDecryptionTag(header.DecryptionTag);
        }

        if (!entries[0].IsDirectory())
            throw std::runtime_error("Root entry is not a directory");

        Rc<VirtualFileDevice> device = swref VirtualFileDevice();
        Rc<fiPackfile7> fops = swref fiPackfile7(std::move(input), header, std::move(names));

        String path;
        fops->AddToVFS(device->Files, entries, path, 0);

        return device;
    }
} // namespace Swage::Rage
