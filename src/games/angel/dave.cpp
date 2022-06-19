#include "dave.h"

#include "asset/device/archive.h"
#include "asset/stream.h"

namespace Swage::Angel
{
    static_assert(sizeof(DaveHeader) == 0x10);
    static_assert(sizeof(DaveEntry) == 0x10);

    static void ReadDaveStringRaw(const Vec<char>& names, usize offset, u32 bit_index, String& output)
    {
        // Modified base-64, using only 48 characters
        static constexpr const char DaveEncoding[48 + 16 + 1] {
            "\0 #$()-./?0123456789_abcdefghijklmnopqrstuvwxyz~!!!!!!!!!!!!!!!!"};

        while (true)
        {
            usize here = offset + (bit_index >> 3);

            u8 bits = 0;

            switch (bit_index & 0x7)
            {
                case 0: // Next: 6
                    bits = u8(names.at(here) & 0x3F);
                    break;

                case 2: // Next: 0
                    bits = u8(names.at(here)) >> 2;
                    break;

                case 4: // Next: 2
                    bits = (u8(names.at(here)) >> 4) | (u8(names.at(here + 1) & 0x3) << 4);
                    break;

                case 6: // Next: 4
                    bits = (u8(names.at(here)) >> 6) | (u8(names.at(here + 1) & 0xF) << 2);
                    break;
            }

            bit_index += 6;

            if (bits != 0)
            {
                output += DaveEncoding[bits];
            }
            else
            {
                break;
            }
        };
    }

    static void ReadDaveString(const Vec<char>& names, usize offset, String& output)
    {
        u8 bits = 0;
        u8 first = u8(names.at(offset));

        if ((first & 0x3F) < 0x38)
        {
            output.clear();
        }
        else
        {
            u8 second = u8(names.at(offset + 1));
            u8 index = (first & 0x7) | ((first & 0xC0) >> 3) | ((second & 0x7) << 5);
            output.resize(index);
            bits = 12;
        }

        ReadDaveStringRaw(names, offset, bits, output);
    }

    class DaveArchive final : public FileArchive
    {
    public:
        DaveArchive(Rc<Stream> input);
    };

    DaveArchive::DaveArchive(Rc<Stream> input)
        : FileArchive(std::move(input))
    {}

    Rc<FileDevice> LoadDave(Rc<Stream> input)
    {
        constexpr u64 TOC_OFFSET = 0x800;

        DaveHeader header;

        if (!input->TryReadBulk(&header, sizeof(header), 0))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x45564144 && header.Magic != 0x65766144)
            throw std::runtime_error("Invalid header magic");

        Vec<DaveEntry> entries(header.FileCount);

        if (!input->TryReadBulk(entries.data(), ByteSize(entries), TOC_OFFSET))
            throw std::runtime_error("Failed to read entries");

        Vec<char> names(header.NamesSize);

        if (!input->TryReadBulk(names.data(), ByteSize(names), TOC_OFFSET + header.NamesOffset))
            throw std::runtime_error("Failed to read names");

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<DaveArchive> fops = MakeRc<DaveArchive>(std::move(input));

        String name;
        bool packed_names = header.Magic == 0x65766144;

        for (const DaveEntry& entry : entries)
        {
            if (packed_names)
                ReadDaveString(names, entry.NameOffset, name);
            else
                name = SubCString(names, entry.NameOffset);

            PathNormalizeSlash(name);

            fops->AddFile(device->Files, name,
                (entry.Size != entry.RawSize) ? ArchiveFile::Deflated(entry.DataOffset, entry.Size, entry.RawSize, -15)
                                              : ArchiveFile::Stored(entry.DataOffset, entry.Size));
        }

        return device;
    }
} // namespace Swage::Angel
