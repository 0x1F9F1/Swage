#pragma once

#include "../stream.h"

#include <cstdio>

namespace Swage
{
    class StdFileStream final : public Stream
    {
    public:
        StdFileStream(std::FILE* handle);
        ~StdFileStream() override;

        static Rc<StdFileStream> Open(const char* path, bool read_only);
        static Rc<StdFileStream> Create(const char* path, bool write_only, bool truncate);

        static Rc<StdFileStream> TempFile();

        i64 Seek(i64 offset, SeekWhence whence) override;
        i64 Tell() override;

        usize Read(void* ptr, usize len) override;
        usize Write(const void* ptr, usize len) override;

    private:
        std::FILE* handle_ {};
    };
} // namespace Swage
