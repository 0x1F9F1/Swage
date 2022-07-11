#include "stdio.h"

#ifdef _WIN32
#    pragma warning(disable : 4996) // This function or variable may be unsafe. Consider using ... instead.
#endif

namespace Swage
{
    StdFileStream::StdFileStream(std::FILE* handle)
        : handle_(handle)
    {}

    StdFileStream::~StdFileStream()
    {
        if (handle_ != nullptr)
            std::fclose(handle_);
    }

    Rc<StdFileStream> StdFileStream::Open(const char* path, bool read_only)
    {
        std::FILE* handle = std::fopen(path, read_only ? "rb" : "r+b");

        if (handle == nullptr)
            return nullptr;

        return swref StdFileStream(handle);
    }

    Rc<StdFileStream> StdFileStream::Create(const char* path, bool write_only, bool truncate)
    {
        std::FILE* handle = std::fopen(path, truncate ? (write_only ? "wb" : "w+b") : "r+b");

        if (handle == nullptr && !truncate)
            handle = std::fopen(path, write_only ? "wb" : "w+b");

        if (handle == nullptr)
            return nullptr;

        return swref StdFileStream(handle);
    }

    Rc<StdFileStream> StdFileStream::TempFile()
    {
        std::FILE* handle = std::tmpfile();

        if (!handle)
            return nullptr;

        return swref StdFileStream(handle);
    }

    i64 StdFileStream::Seek(i64 offset, SeekWhence whence)
    {
        int origin = -1;

        switch (whence)
        {
            case SeekWhence::Set: origin = SEEK_SET; break;
            case SeekWhence::Cur: origin = SEEK_CUR; break;
            case SeekWhence::End: origin = SEEK_END; break;
        }

        if ((origin != SEEK_CUR) || (offset != 0))
        {
            if ((offset < LONG_MIN) || (offset > LONG_MAX))
                return -1;

            if (std::fseek(handle_, static_cast<long>(offset), origin) != 0)
                return -1;
        }

        return Tell();
    }

    i64 StdFileStream::Tell()
    {
        return static_cast<i64>(std::ftell(handle_));
    }

    usize StdFileStream::Read(void* ptr, usize len)
    {
        return std::fread(ptr, 1, len, handle_);
    }

    usize StdFileStream::Write(const void* ptr, usize len)
    {
        return std::fwrite(ptr, 1, len, handle_);
    }
} // namespace Swage
