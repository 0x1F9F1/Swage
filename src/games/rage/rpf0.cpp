#include "rpf0.h"

#include "asset/stream.h"

namespace Swage::Rage
{
    static_assert(sizeof(fiPackHeader0) == 0xC);
    static_assert(sizeof(fiPackEntry0) == 0x10);

    class fiPackfile0 final : public FileArchive
    {
    public:
        fiPackfile0(Rc<Stream> input)
            : FileArchive(std::move(input))
        {}

        void AddToVFS(VFS& vfs, const Vec<fiPackEntry0>& entries, const Vec<char>& names, String& path,
            const fiPackEntry0& directory);
    };

    void fiPackfile0::AddToVFS(
        VFS& vfs, const Vec<fiPackEntry0>& entries, const Vec<char>& names, String& path, const fiPackEntry0& directory)
    {
        for (u32 i = directory.GetEntryIndex(), end = i + directory.GetEntryCount(); i < end; ++i)
        {
            usize old_size = path.size();

            const fiPackEntry0& entry = entries.at(i);
            path += SubCString(names, entry.GetNameOffset());

            if (entry.IsDirectory())
            {
                path += '/';

                AddToVFS(vfs, entries, names, path, entry);
            }
            else
            {
                u32 size = entry.GetSize();
                u32 disk_size = entry.GetOnDiskSize();
                u32 offset = entry.GetOffset();

                AddFile(vfs, path,
                    (size != disk_size) ? ArchiveFile::Deflated(offset, size, disk_size, -15)
                                        : ArchiveFile::Stored(offset, size));
            }

            path.resize(old_size);
        }
    }

    Rc<VirtualFileDevice> LoadRPF0(Rc<Stream> input)
    {
        constexpr u64 TOC_OFFSET = 0x800;

        fiPackHeader0 header;

        if (!input->TryReadBulk(&header, sizeof(header), 0))
            throw std::runtime_error("Failed to read header");

        if (header.Magic != 0x30465052)
            throw std::runtime_error("Invalid header magic");

        u32 entries_size = header.EntryCount * sizeof(fiPackEntry0);

        if (header.HeaderSize <= entries_size)
            throw std::runtime_error("Invalid header size");

        Vec<fiPackEntry0> entries(header.EntryCount);

        if (!input->TryReadBulk(entries.data(), ByteSize(entries), TOC_OFFSET))
            throw std::runtime_error("Failed to read entries");

        u32 names_size = header.HeaderSize - entries_size;
        Vec<char> names(names_size);

        if (!input->TryReadBulk(names.data(), ByteSize(names), TOC_OFFSET + header.HeaderSize - names_size))
            throw std::runtime_error("Failed to read names");

        // rage::fiPackfile::FindEntry assumes the first entry is a directory (and ignores its name)
        const fiPackEntry0& root = entries.at(0);

        if (!root.IsDirectory())
            throw std::runtime_error("Root entry is not a directory");

        Rc<VirtualFileDevice> device = MakeRc<VirtualFileDevice>();
        Rc<fiPackfile0> fops = MakeRc<fiPackfile0>(std::move(input));

        String path;
        fops->AddToVFS(device->Files, entries, names, path, root);

        return device;
    }
} // namespace Swage::Rage
