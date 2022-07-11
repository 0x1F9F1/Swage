#include "rpf8.h"

#include "asset/device/virtual.h"

#include "asset/stream.h"
#include "asset/stream/buffered.h"
#include "asset/stream/decode.h"
#include "asset/stream/ecb.h"
#include "asset/stream/partial.h"

#include "asset/transform/deflate.h"
#include "asset/transform/oodle.h"

#include "asset/glob.h"

#include "crypto/secret.h"
#include "crypto/tfit.h"
#include "crypto/tfit2.h"

#include "core/bits.h"

#include "rsc.h"
#include "utils.h"

namespace Swage::Rage::RPF8
{
    // rage::AES::UnpackConfig
    // head_length  in [0, 0x1000, 0x4000, 0x10000]
    // block_length in [0, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000]
    // block_stride in [0, 0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x60000, 0x70000, 0x80000]
    static void UnpackConfig(u8 config, i64& head_length, i64& block_length, i64& block_stride)
    {
        if (u8 head_config = config & 0b11) // Config[0:2]
        {
            head_length = i64(0x400) << (head_config * 2);
        }

        if (u8 length_config = (config >> 2) & 0b111) // Config[2:5]
        {
            u8 stride_config = (config >> 5) & 0b111; // Config[5:8]
            block_length = i64(0x400) << length_config;
            block_stride = i64(stride_config + 1) << 16;
        }
    }

    static Vec<Pair<u64, u64>> UnpackConfig(u8 config, i64 file_size, i64 chunk_size)
    {
        Vec<Pair<u64, u64>> results;
        i64 offset = 0;

        const auto Update = [&](i64 start, i64 end) {
            start = std::max<i64>(start, offset);
            end = std::min<i64>(end, file_size);

            if (start < end)
            {
                results.push_back({static_cast<u64>(start), static_cast<u64>(end)});
                offset = end;
            }
        };

        i64 tail_length = 0x400;
        i64 tail_offset = file_size - tail_length;

        i64 head_length = 0;
        i64 block_length = 0;
        i64 block_stride = 0;

        UnpackConfig(config, head_length, block_length, block_stride);

        Update(0, head_length);

        if (head_length < tail_offset)
        {
            if (block_length || block_stride)
            {
                SwAssert(offset <= block_stride);
                // SwAssert(block_length <= block_stride);

                if ((block_stride == chunk_size) && (tail_offset < block_stride) && (block_stride < file_size))
                {
                    // There is a bug in rage::AES::Cipher.
                    // Under certain conditions, the ranges of data it ciphers can change depending on how you call it.
                    // SwLogInfo("{:02X} {:X}", config, file_size);

                    // This should mirror what the game does, but a few files still fail to decompress
                    // (and some others probably just decompress incorrectly, as there are no CRCs)
                    offset = block_stride;
                }
                else
                {
                    for (i64 block = offset ? block_stride : 0; block + block_length <= tail_offset;
                         block += block_stride)
                    {
                        Update(block, block + block_length);
                    }
                }
            }

            Update(tail_offset, file_size);
        }

        return results;
    }

    class StridedCipher final : public Cipher
    {
    public:
        u64 Offset {};
        Ptr<Cipher> Impl;
        Vec<Pair<u64, u64>> Blocks;

        StridedCipher(u8 config, i64 size, Ptr<Cipher> cipher, i64 chunk_size);

        usize Update(const u8* input, u8* output, usize length) override;
        usize GetBlockSize() override;

    private:
        void UpdateBlocks(const u8* input, u8* output, usize length);
    };

    StridedCipher::StridedCipher(u8 config, i64 size, Ptr<Cipher> cipher, i64 chunk_size)
        : Impl(std::move(cipher))
        , Blocks(UnpackConfig(config, size, chunk_size))
    {}

    usize StridedCipher::Update(const u8* input, u8* output, usize length)
    {
        UpdateBlocks(input, output, length);

        return length;
    }

    usize StridedCipher::GetBlockSize()
    {
        return 16;
    }

    void StridedCipher::UpdateBlocks(const u8* input, u8* output, usize length)
    {
        const auto update = [&](u64 offset, bool cipher) {
            SwAssert(offset >= Offset);

            if (usize n = static_cast<usize>(std::min<u64>(offset - Offset, length)))
            {
                if (cipher)
                    Impl->Update(input, output, n);
                else if (input != output)
                    std::memcpy(output, input, n);

                Offset += n;
                input += n;
                output += n;
                length -= n;
            }

            return Offset == offset;
        };

        for (const auto& [start, end] : Blocks)
        {
            if (Offset >= end)
                continue;

            if (Offset < start)
            {
                if (!update(start, false))
                    return;
            }

            if (!update(end, true))
                return;
        }

        update(UINT64_MAX, false);
    }

#include "hashes/RDR2.h"

    static const char* RAGE_IV_HASH = "5C8xGcF{L^AxqTQd!ofA";

    static HashMap<usize, Tfit2Key> RDR2_PC_KEYS;
    static Tfit2Context RDR2_PC_CONTEXT;
    static u8 RDR2_PC_IV[16];
    static bool RDR2_PC_KEYS_LOADED;

    static u32 RDR2_ANDROID_TFIT_KEYS[164][20][4];
    static u32 RDR2_ANDROID_TFIT_TABLES[17][16][256];
    static u8 RDR2_ANDROID_IV[16];
    static bool RDR2_ANDROID_KEYS_LOADED;

    static u32 RDR2_PS4_TFIT_KEYS[164][20][4];
    static u32 RDR2_PS4_TFIT_TABLES[17][16][256];
    static u8 RDR2_PS4_IV[16];
    static bool RDR2_PS4_KEYS_LOADED;

#define X(NAME, HASH)                                \
    do                                               \
    {                                                \
        if (!Secrets.Get(HASH, &NAME, sizeof(NAME))) \
            return false;                            \
    } while (false)

    static bool LoadKeys_RDR2_PC()
    {
        Tfit2Key key;

        for (usize i = 0; i < 163; ++i)
        {
            if (!Secrets.Get(RDR2_PC_KEY_HASHES[i], &key, sizeof(key)))
                return false;

            RDR2_PC_KEYS.emplace(i, key);
        }

        if (Secrets.Get(RDR2_PC_KEY_HASHES[163], &key, sizeof(key)))
            RDR2_PC_KEYS.emplace(0xC0, key);

        if (Secrets.Get(RDR2_PC_KEY_HASHES[164], &key, sizeof(key)))
            RDR2_PC_KEYS.emplace(0xC5, key);

        if (Secrets.Get(RDR2_PC_KEY_HASHES[165], &key, sizeof(key)))
            RDR2_PC_KEYS.emplace(0xC6, key);

#define Y(NAME) X(RDR2_PC_CONTEXT.NAME, RDR2_PC_TFIT2_HASHES.NAME)

        Y(InitTables);

        for (usize i = 0; i < 17; ++i)
        {
            Y(Rounds[i].Lookup);

            for (usize j = 0; j < 16; ++j)
            {
                Y(Rounds[i].Blocks[j].Masks);
                Y(Rounds[i].Blocks[j].Xor);
            }
        }

        Y(EndMasks);
        Y(EndTables);
        Y(EndXor);

#undef Y

        X(RDR2_PC_IV, RAGE_IV_HASH);

        return true;
    }

    static bool LoadKeys_RDR2_ANDROID()
    {
        for (usize i = 0; i < 164; ++i)
            X(RDR2_ANDROID_TFIT_KEYS[i], RDR2_ANDROID_KEY_HASHES[i]);

        for (usize i = 0; i < 17; ++i)
            for (usize j = 0; j < 16; ++j)
                X(RDR2_ANDROID_TFIT_TABLES[i][j], RDR2_TFIT_TABLE_HASHES[i][j]);

        X(RDR2_ANDROID_IV, RAGE_IV_HASH);

        return true;
    }

    static bool LoadKeys_RDR2_PS4()
    {
        for (usize i = 0; i < 164; ++i)
            X(RDR2_PS4_TFIT_KEYS[i], RDR2_PS4_KEY_HASHES[i]);

        for (usize i = 0; i < 17; ++i)
            for (usize j = 0; j < 16; ++j)
                X(RDR2_PS4_TFIT_TABLES[i][j], RDR2_TFIT_TABLE_HASHES[i][j]);

        X(RDR2_PS4_IV, RAGE_IV_HASH);

        return true;
    }

#undef X

#define X(NAME) finder.Add(NAME)

    void FindKeys_RDR2_PC(SecretFinder& finder)
    {
        for (usize i = 0; i < 166; ++i)
            X(RDR2_PC_KEY_HASHES[i]);

#define Y(NAME) X(RDR2_PC_TFIT2_HASHES.NAME)

        Y(InitTables);

        for (usize i = 0; i < 17; ++i)
        {
            Y(Rounds[i].Lookup);

            for (usize j = 0; j < 16; ++j)
            {
                Y(Rounds[i].Blocks[j].Masks);
                Y(Rounds[i].Blocks[j].Xor);
            }
        }

        Y(EndMasks);
        Y(EndTables);
        Y(EndXor);

#undef Y

        X(RAGE_IV_HASH);
    }

    void FindKeys_RDR2_PS4(SecretFinder& finder)
    {
        for (usize i = 0; i < 164; ++i)
            X(RDR2_PS4_KEY_HASHES[i]);

        for (usize i = 0; i < 17; ++i)
            for (usize j = 0; j < 16; ++j)
                X(RDR2_TFIT_TABLE_HASHES[i][j]);

        X(RAGE_IV_HASH);
    }

#undef X

    void LoadKeys()
    {
        RDR2_PC_KEYS_LOADED = LoadKeys_RDR2_PC();
        RDR2_ANDROID_KEYS_LOADED = LoadKeys_RDR2_ANDROID();
        RDR2_PS4_KEYS_LOADED = LoadKeys_RDR2_PS4();
    }

    Option<Ptr<Cipher>> MakeCipher(u16 platform, u16 tag)
    {
        if (tag == 0xFF)
            return nullptr;

        if (platform == 'y')
        {
            if (!RDR2_PC_KEYS_LOADED)
                throw std::runtime_error("RDR2 PC keys not loaded");

            if (auto iter = RDR2_PC_KEYS.find(tag); iter != RDR2_PC_KEYS.end())
                return swnew Tfit2CbcCipher(&iter->second, RDR2_PC_IV, &RDR2_PC_CONTEXT);
        }
        else if (platform == 'a')
        {
            if (!RDR2_ANDROID_KEYS_LOADED)
                throw std::runtime_error("RDR2 Android keys not loaded");

            if (tag >= 163)
            {
                if (tag == 0xC0)
                    tag = 163;
                else
                    return None;
            }

            return swnew TfitCbcCipher(RDR2_ANDROID_TFIT_KEYS[tag] + 1, RDR2_ANDROID_TFIT_TABLES, RDR2_ANDROID_IV);
        }
        else if (platform == 'o')
        {
            if (!RDR2_PS4_KEYS_LOADED)
                throw std::runtime_error("RDR2 PS4 keys not loaded");

            if (tag >= 163)
            {
                if (tag == 0xC0)
                    tag = 163;
                else
                    return None;
            }

            return swnew TfitCbcCipher(RDR2_PS4_TFIT_KEYS[tag] + 1, RDR2_PS4_TFIT_TABLES, RDR2_PS4_IV);
        }

        return None;
    }

    static constexpr StringView BaseRageExts[14] {
        "rpf", "#mf", "#dr", "#ft", "#dd", "#td", "#bn", "#bd", "#pd", "#bs", "#sd", "#mt", "#sc", "#cs"};

    static constexpr StringView ExtraRageExts[24] {"mrf", "cut", "gfx", "#cd", "#ld", "#pmd", "#pm", "#ed", "#pt",
        "#map", "#typ", "#ch", "#ldb", "#jd", "#ad", "#nv", "#hn", "#pl", "#nd", "#vr", "#wr", "#nh", "#fd", "#as"};

    static String GetFileExt(u8 id, char platform)
    {
        String result;

        if (id >= 0 && id <= 13)
            result = BaseRageExts[id];
        else if (id >= 64 && id <= 87)
            result = ExtraRageExts[id - 64];
        else /*if (id == 255)*/
            result = "bin";

        for (char& c : result)
            if (c == '#')
                c = platform;

        return result;
    }

    static bool CompareFileExt(StringView lhs, StringView rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (usize i = 0; i < lhs.size(); ++i)
        {
            const char a = lhs[i];
            const char b = rhs[i];

            if ((a != b) && (b != '#'))
                return false;
        }

        return true;
    }

    static u8 GetFileExtId(StringView ext)
    {
        for (u8 i = 0; i < 14; ++i)
            if (CompareFileExt(ext, BaseRageExts[i]))
                return i;

        for (u8 i = 0; i < 24; ++i)
            if (CompareFileExt(ext, ExtraRageExts[i]))
                return i + 64;

        // SwAssert(ext == "bin");

        return 255;
    }

    static std::unordered_map<Tuple<u32, u8>, String, HashTuple<u32, u8>> KnownFiles;

    static std::unordered_map<u32, String> PossibleFiles;

    static Pair<StringView, StringView> SplitFileName(StringView name)
    {
        auto folder = name.rfind('/');

        if (folder == StringView::npos)
            folder = 0;
        else
            folder += 1;

        if (auto ext = name.rfind('.'); (ext != String::npos) && (ext >= folder))
        {
            return {name.substr(0, ext), name.substr(ext + 1)};
        }

        return {name, {}};
    }

    static void Replace(String& subject, StringView search, StringView replace)
    {
        for (usize pos = 0; (pos = subject.find(search, pos)) != String::npos; pos += replace.length())
        {
            subject.replace(pos, search.length(), replace);
        }
    }

    static Tuple<StringView, u32, u8> NormalisePath(String& path)
    {
        // TODO: Support other platforms
        Replace(path, "%PLATFORM%", "x64");

        for (char& c : path)
            c = ToLower(c);

        const auto [name, ext] = SplitFileName(path);

        u8 ext_id = GetFileExtId(ext);

        StringView result = (ext_id != 0xFF) ? name : path;

        u32 hash = atStringHash(result);

        return {result, hash, ext_id};
    }

    static bool AddPossibleFileName(String path)
    {
        const auto [name, hash, ext] = NormalisePath(path);

        return PossibleFiles.try_emplace(hash, name).second;
    }

    void LoadFileList(BufferedStream& input)
    {
        for (String path; input.GetLine(path); path.clear())
        {
            const auto [name, hash, ext] = NormalisePath(path);

            KnownFiles.try_emplace(Tuple<u32, u8> {hash, ext}, name);
        }
    }

    void SaveFileList(BufferedStream& output)
    {
        for (const auto& file : KnownFiles)
        {
            const auto [hash, ext_id] = file.first;

            output.PutString(file.second);

            if (ext_id != 0xFF)
            {
                output.PutCh('.');
                output.PutString(GetFileExt(ext_id, '#'));
            }

            output.PutCh('\n');
        }
    }

    void LoadPossibleFileList(BufferedStream& input)
    {
        for (String line; input.GetLine(line); line.clear())
        {
            AddPossibleFileName(line);
        }
    }

    static String GetFileName(u32 hash, u8 ext, char platform)
    {
        static Mutex name_lock;
        LockGuard<Mutex> guard(name_lock);

        auto find = KnownFiles.find(Tuple<u32, u8> {hash, ext});

        if (find == KnownFiles.end())
        {
            if (auto poss_find = PossibleFiles.find(hash); poss_find != PossibleFiles.end())
            {
                find = KnownFiles.emplace_hint(find, Tuple<u32, u8> {hash, ext}, poss_find->second);

                SwLogInfo("Added possible name: {}", poss_find->second);
            }
            else
            {
                return fmt::format("hash/{:08X}.{}", hash, GetFileExt(ext, platform));
            }
        }

        String result = find->second;

        if (ext != 0xFF)
        {
            result += '.';
            result += GetFileExt(ext, platform);
        }

        return result;
    }
} // namespace Swage::Rage::RPF8

namespace Swage::Rage
{
    static_assert(is_c_struct_v<fiPackHeader8, 0x10>);
    static_assert(is_c_struct_v<fiPackEntry8, 0x18>);

    bool fiPackEntry8::GetResourceFileHeader(datResourceFileHeader& info) const
    {
        if (!IsResource())
            return false;

#if 1
        u32 bits0 = GetResourceId();
        u32 bits8 = ((0 - 1) & 0x1F) << 8;
        u32 bits15 = false << 15;
        u32 bits16 = (0xFF + 1) << 16;
#else
        u32 bits0 = GetResourceId();
        u32 bits8 = ((GetCompressorId() - 1) & 0x1F) << 8;
        u32 bits15 = IsSignatureProtected() << 15;
        u32 bits16 = (GetEncryptionKeyId() + 1) << 16;
#endif

        info.Magic = 0x38435352; // RSC8
        info.Flags = bits0 | bits8 | bits15 | bits16;

        info.ResourceInfo.VirtualFlags = GetVirtualFlags();
        info.ResourceInfo.PhysicalFlags = GetPhysicalFlags();

        return true;
    }

    // TODO: Handle lookup by name and hash
    class fiPackfile8 final : public FileOperationsT<fiPackEntry8>
    {
    public:
        fiPackfile8(Rc<Stream> input, fiPackHeader8 header);

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;
        bool Extension(File& file, const ExtensionData& data) override;

        void AddToVFS(VFS& vfs, const fiPackEntry8& entry);

        Rc<Stream> Input {};
        fiPackHeader8 Header {};
    };

    fiPackfile8::fiPackfile8(Rc<Stream> input, fiPackHeader8 header)
        : Input(std::move(input))
        , Header(header)
    {}

    Rc<Stream> fiPackfile8::Open(File& file)
    {
        const fiPackEntry8& entry = GetData(file);

        u64 size = entry.GetSize();
        u64 raw_size = entry.GetOnDiskSize();
        u64 offset = entry.GetOffset();
        bool is_compressed = entry.GetCompressorId() != 0;

        if (entry.IsSignatureProtected())
        {
            if (raw_size < 0x100)
                throw std::runtime_error("Signature protected file is too small");

            raw_size -= 0x100;
        }

        // See rage::pgStreamer::DeviceHandler::ProcessRequest
        if (entry.IsResource())
        {
            if (raw_size < 16)
                throw std::runtime_error("Resource raw size is too small");

            offset += 16;
            raw_size -= 16;
        }

        Rc<Stream> result = swref PartialStream(offset, is_compressed ? raw_size : size, Input);

        if (Option<Ptr<Cipher>> cipher = RPF8::MakeCipher(Header.PlatformId, entry.GetEncryptionKeyId()))
        {
            if (*cipher)
            {
                i64 chunk_size = entry.IsResource()
                    // pgStreamer reads compressed data in chunks of 0x80000
                    ? (is_compressed ? 0x80000 : size)
                    // fiPackFile8 reads compressed data in chunks of 0x2000
                    // fiStream reads data in chunks of 0x1000 (depending on reads)
                    : (is_compressed ? 0x2000 : 0x1000);

                // FIXME: EcbCipherStream assumes the cipher has no IV
                result = swref EcbCipherStream(std::move(result),
                    swnew RPF8::StridedCipher(entry.GetEncryptionConfig(), raw_size, std::move(*cipher), chunk_size));
            }
        }
        else
        {
            throw std::runtime_error(fmt::format("Unknown cipher key for entry 0x{:08X}: 0x{:02X}, 0x{:02X}",
                entry.GetHash(), Header.PlatformId, entry.GetEncryptionKeyId()));
        }

        switch (entry.GetCompressorId())
        {
            case 1: // Deflate
                result = swref DecodeStream(std::move(result), swnew DeflateDecompressor(-15), size);
                break;

            case 2: // Oodle
                result = swref DecodeStream(std::move(result), swnew OodleDecompressor(size), size);
                break;
        }

        return result;
    }

    void fiPackfile8::Stat(File& file, FolderEntry& stat)
    {
        const fiPackEntry8& entry = GetData(file);

        stat.Size = entry.GetSize();
    }

    bool fiPackfile8::Extension(File& file, const ExtensionData& data)
    {
        const fiPackEntry8& entry = GetData(file);

        if (datResourceFileHeader* header = data.As<datResourceFileHeader>())
            return entry.GetResourceFileHeader(*header);

        return false;
    }

    void fiPackfile8::AddToVFS(VFS& vfs, const fiPackEntry8& entry)
    {
        String path = RPF8::GetFileName(entry.GetHash(), entry.GetFileExtId(), static_cast<char>(Header.PlatformId));

        AddFile(vfs, path, entry);
    }

    Rc<FileDevice> LoadRPF8(Rc<Stream> input)
    {
        fiPackHeader8 header;

        if (!input->Rewind() || !input->TryRead(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x52504638)
            throw std::runtime_error("Invalid header magic");

        u8 rsa_signature[0x100];

        if (!input->TryRead(&rsa_signature, sizeof(rsa_signature)))
            throw std::runtime_error("Failed to read RSA signature");

        Vec<fiPackEntry8> entries(header.EntryCount);

        if (!input->TryRead(entries.data(), ByteSize(entries)))
            throw std::runtime_error("Failed to read entries");

        if (Option<Ptr<Cipher>> cipher = RPF8::MakeCipher(header.PlatformId, header.DecryptionTag))
        {
            if (*cipher)
                (*cipher)->Update(entries.data(), ByteSize(entries));
        }
        else
        {
            throw std::runtime_error(fmt::format(
                "Unknown cipher key for header: 0x{:02X}, 0x{:02X}", header.PlatformId, header.DecryptionTag));
        }

#if 0
        if (header.DecryptionTag == 0xFF)
        {
            Vec<char> names(header.NamesLength);

            if (input->TryReadBulk(names.data(), names.size(), input->Size() - header.NamesLength))
            {
                StringView names_view(names.data(), names.size());

                for (usize i = 0; i < names_view.size();)
                {
                    usize next = names_view.find('\0', i);

                    if (next == i || next == StringView::npos)
                        break;

                    StringView name = names_view.substr(i, next - i);

                    i = next + 1;
                }
            }
        }
#endif

        Rc<VirtualFileDevice> device = swref VirtualFileDevice();
        Rc<fiPackfile8> fops = swref fiPackfile8(std::move(input), header);

        for (const fiPackEntry8& entry : entries)
        {
            if (!entry.IsDirectory())
                fops->AddToVFS(device->Files, entry);
        }

        return device;
    }
} // namespace Swage::Rage
