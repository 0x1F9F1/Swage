#include "img3.h"

#include "asset/stream.h"
#include "crypto/aes.h"
#include "crypto/cipher.h"
#include "crypto/secret.h"

namespace Swage::Rage
{
    static_assert(is_c_struct_v<IMGHeader3, 0x14>);
    static_assert(is_c_struct_v<IMGEntry3, 0x10>);

    class IMGArchive3 final : public FileOperationsT<IMGEntry3>
    {
    public:
        IMGArchive3(Rc<Stream> input);

        Rc<Stream> Input {};

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;
    };

    IMGArchive3::IMGArchive3(Rc<Stream> input)
        : Input(std::move(input))
    {}

    Rc<Stream> IMGArchive3::Open(File& file)
    {
        const IMGEntry3& entry = GetData(file);

        u32 size = entry.GetSize();
        u32 offset = entry.GetOffset();
        u32 raw_size = entry.GetOnDiskSize();

        ArchiveFile result;

        if (entry.IsResource())
        {
            // TODO: Merge this with RPF2 resource handling?

            // SwAssert((entry.dword0 & 0xC0000000) == 0xC0000000);

            if (raw_size < 0xC)
                throw std::runtime_error("Resource raw size is too small");

            offset += 0xC;
            raw_size -= 0xC;

            // TODO: Handle LZXD compression
            result = ArchiveFile::Deflated(offset, raw_size, size, 15);
        }
        else
        {
            result = ArchiveFile::Stored(offset, raw_size);
        }

        return result.Open(Input);
    }

    void IMGArchive3::Stat(File& file, FolderEntry& output)
    {
        const IMGEntry3& entry = GetData(file);

        output.Size = entry.GetSize();
    }

    static const char* DEFAULT_KEY_HASH = "AOHXWtl`i|@X1feM%MjV";

    static void Cipher16(Cipher& cipher, void* data, usize length)
    {
        for (usize i = 0; i < 16; ++i)
            cipher.Update(data, length);
    }

    Rc<VirtualFileDevice> LoadIMG3(Rc<Stream> input)
    {
        constexpr u64 TOC_OFFSET = 0x14;

        IMGHeader3 header;

        if (!input->TryReadBulk(&header, sizeof(header), 0x0))
            throw std::runtime_error("Failed to read header");

        Ptr<Cipher> cipher;

        if (header.Magic != 0xA94E2A52)
        {
            u8 key[32];

            if (!Secrets.Get(DEFAULT_KEY_HASH, key, sizeof(key)))
                throw std::runtime_error("Invalid header magic (or missing key)");

            cipher = swnew AesEcbCipher(key, sizeof(key), true);

            Cipher16(*cipher, &header, 0x10);

            if (header.Magic != 0xA94E2A52)
                throw std::runtime_error("Invalid header magic");
        }

        // SwAssert(header.Version == 3);
        // SwAssert(header.EntrySize == 0x10);

        Vec<IMGEntry3> entries(header.EntryCount);

        if (!input->TryReadBulk(entries.data(), ByteSize(entries), TOC_OFFSET))
            throw std::runtime_error("Failed to read entries");

        u32 names_offset = header.EntryCount * header.EntrySize;

        if (header.HeaderSize < names_offset)
            throw std::runtime_error("Invalid header size");

        Vec<char> names(header.HeaderSize - names_offset);

        if (!input->TryReadBulk(names.data(), ByteSize(names), TOC_OFFSET + names_offset))
            throw std::runtime_error("Failed to read names");

        if (cipher)
        {
            Cipher16(*cipher, entries.data(), ByteSize(entries));
            Cipher16(*cipher, names.data(), ByteSize(names));
        }

        Rc<VirtualFileDevice> device = swref VirtualFileDevice();
        Rc<IMGArchive3> fops = swref IMGArchive3(std::move(input));

        usize name_offset = 0;

        for (const IMGEntry3& entry : entries)
        {
            // SwAssert(name_offset < names.size());
            StringView name = SubCString(names, name_offset);
            fops->AddFile(device->Files, name, entry);
            name_offset += name.size() + 1;
        }

        return device;
    }
} // namespace Swage::Rage
