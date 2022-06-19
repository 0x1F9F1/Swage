#include "ares.h"

#include "asset/device/archive.h"
#include "asset/stream.h"

#include <charconv>

namespace Swage::Angel
{
    static_assert(sizeof(AresHeader) == 0x10);
    static_assert(sizeof(VirtualFileINode) == 0xC);

    class AresArchive final : public FileArchive
    {
    public:
        AresArchive(Rc<Stream> input);

        void AddToVFS(VFS& vfs, const Vec<VirtualFileINode>& entries, const Vec<char>& names, String& path, u32 index);
    };

    AresArchive::AresArchive(Rc<Stream> input)
        : FileArchive(std::move(input))
    {}

    void AresArchive::AddToVFS(
        VFS& vfs, const Vec<VirtualFileINode>& entries, const Vec<char>& names, String& path, u32 index)
    {
        usize old_size = path.size();

        const VirtualFileINode& node = entries.at(index);

        for (const char c : SubCString(names, node.GetNameOffset()))
        {
            if (c == '\1')
            {
                char name_integer[16];
                auto [p, ec] = std::to_chars(name_integer, name_integer + 16, node.GetNameInteger());
                SwDebugAssert(ec == std::errc());

                path.append(name_integer, p - name_integer);
            }
            else
            {
                path += c;
            }
        }

        if (u32 ext_offset = node.GetExtOffset(); ext_offset != 0)
        {
            path += '.';
            path += SubCString(names, ext_offset);
        }

        u32 offset = node.GetOffset();
        u32 size = node.GetSize();

        if (node.IsDirectory())
        {
            path += '/';

            for (u32 i = 0; i < size; ++i)
                AddToVFS(vfs, entries, names, path, offset + i);
        }
        else
        {
            AddFile(vfs, path, ArchiveFile::Stored(offset, size));
        }

        path.resize(old_size);
    }

    Rc<FileDevice> LoadARES(Rc<Stream> input)
    {
        AresHeader header;

        if (!input->Rewind() || !input->Read(&header, sizeof(header)))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x53455241)
            throw std::runtime_error("Invalid header magic");

        Vec<VirtualFileINode> entries(header.FileCount);

        if (!input->TryRead(entries.data(), ByteSize(entries)))
            throw std::runtime_error("Failed to read entries");

        Vec<char> names(header.NamesSize);

        if (!input->TryRead(names.data(), ByteSize(names)))
            throw std::runtime_error("Failed to read names");

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<AresArchive> fops = MakeRc<AresArchive>(std::move(input));

        String path;

        for (u32 i = 0; i < header.RootCount; ++i)
            fops->AddToVFS(device->Files, entries, names, path, i);

        return device;
    }
} // namespace Swage::Angel
