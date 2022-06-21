#include "pbo.h"

#include "asset/device/virtual.h"
#include "asset/stream/buffered.h"
#include "asset/stream/decode.h"
#include "asset/stream/partial.h"
#include "asset/transform/lzss.h"

#include "core/bits.h"

namespace Swage::Bohemia
{
    using namespace bits;

    struct PboEntry
    {
        u32 Offset {};
        u32 PackingMethod {};
        u32 Size {};
        u32 RawSize {};
    };

    class PboArchive final : public FileOperationsT<PboEntry>
    {
    public:
        PboArchive(Rc<Stream> input)
            : Input(std::move(input))
        {}

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;

        Rc<Stream> Input {};
        u64 DataOffset {};
    };

    Rc<Stream> PboArchive::Open(File& file)
    {
        PboEntry& data = GetData(file);

        Rc<Stream> result = MakeRc<PartialStream>(DataOffset + data.Offset, data.RawSize, Input);

        if (data.PackingMethod == 0x43707273)
            result = MakeRc<DecodeStream>(std::move(result), MakeUnique<LzssDecompressor>(), data.Size);

        return result;
    }

    void PboArchive::Stat(File& file, FolderEntry& entry)
    {
        PboEntry& data = GetData(file);

        entry.Size = data.Size;
    }

    static void ReadExtensions(BufferedStream& input)
    {
        String name;
        String value;

        while (true)
        {
            name.clear();
            value.clear();

            input.GetLine(name, '\0');

            if (name.empty())
                break;

            input.GetLine(value, '\0');
        }
    }

    struct RawPboEntry
    {
        le<u32> PackingMethod;
        le<u32> OriginalSize;
        le<u32> Reserved;
        le<u32> TimeStamp;
        le<u32> DataSize;

        bool IsVers() const
        {
            return (PackingMethod == 0x56657273) && (TimeStamp == 0) && (DataSize == 0);
        }
    };

    static_assert(sizeof(RawPboEntry) == 0x14);

    Rc<FileDevice> LoadPBO(Rc<Stream> input)
    {
        BufferedStream stream(input);
        stream.Rewind();

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<PboArchive> fops = MakeRc<PboArchive>(std::move(input));

        String name;
        name.reserve(128);

        bool at_start = true;
        u32 data_offset = 0;

        while (true)
        {
            name.clear();
            stream.GetLine(name, '\0');

            RawPboEntry entry;

            if (!stream.TryRead(&entry, sizeof(entry)))
                break;

            if (name.empty())
            {
                if (!entry.IsVers())
                    break;

                if (at_start)
                {
                    ReadExtensions(stream);
                }
                else
                {
                    // SwLogError("Invalid Vers Header");
                }

                continue;
            }

            PboEntry file;
            file.Offset = data_offset;
            file.PackingMethod = entry.PackingMethod;

            if (entry.PackingMethod == 0x43707273)
            {
                u32 raw_size = entry.DataSize;

                if (raw_size >= 4)
                    raw_size -= 4; // 32-bit checksum

                file.Size = entry.OriginalSize;
                file.RawSize = raw_size;
            }
            else
            {
                file.Size = entry.DataSize;
                file.RawSize = entry.DataSize;
            }

            PathNormalizeSlash(name);

            fops->AddFile(device->Files, name, file);

            data_offset += entry.DataSize;
            at_start = false;
        }

        fops->DataOffset = stream.Tell();

        return device;
    }
} // namespace Swage::Bohemia
