#pragma once

#include "asset/device/virtual.h"
#include "core/callback.h"

namespace Swage
{
    class BinaryTransform;

    struct ArchiveFile
    {
        using TransformCallback = StaticFunc<Ptr<BinaryTransform>()>;

        u64 Offset {};
        u64 Size {};
        u64 RawSize {};
        TransformCallback Transform {};

        constexpr ArchiveFile() = default;

        ArchiveFile(u64 offset, u64 size, u64 raw_size, TransformCallback transform);

        static ArchiveFile Stored(u64 offset, u64 size);
        static ArchiveFile Deflated(u64 offset, u64 size, u64 raw_size, i32 window_bits);

        Rc<Stream> Open(Rc<Stream> input) const;
    };

    class FileArchive : public FileOperationsT<ArchiveFile>
    {
    public:
        FileArchive(Rc<Stream> input);

        Rc<Stream> Open(File& file) override;
        void Stat(File& file, FolderEntry& entry) override;

        Rc<Stream> Input;
    };

    inline ArchiveFile::ArchiveFile(u64 offset, u64 size, u64 raw_size, TransformCallback transform)
        : Offset(offset)
        , Size(size)
        , RawSize(raw_size)
        , Transform(transform)
    {}

    inline ArchiveFile ArchiveFile::Stored(u64 offset, u64 size)
    {
        return ArchiveFile(offset, size, size, nullptr);
    }
} // namespace Swage
