#include "stream.h"

#include "stream/sync.h"

namespace Swage
{
    /*
        int close(int fd);

        int fclose(std::FILE* stream);

        BOOL CloseHandle(HANDLE hObject);
    */

    /*
        off_t lseek(int fd, off_t offset, int whence);
        off64_t lseek64(int fd, off64_t offset, int whence);

        int fseek(std::FILE* stream, long offset, int origin);

        DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
        BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);
    */
    i64 Stream::Seek(i64, SeekWhence)
    {
        return -1;
    }

    /*
        lseek(fd, 0, SEEK_CUR);

        long ftell(std::FILE* stream);

        SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
    */
    i64 Stream::Tell()
    {
        return Seek(0, SeekWhence::Cur);
    }

    /*
        int fstat(int fd, struct stat *buf);

        lseek(fd, 0, SEEK_END);

        BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);
    */
    i64 Stream::Size()
    {
        i64 size = -1;

        if (i64 here = Tell(); here >= 0)
        {
            size = Seek(0, SeekWhence::End);

            if (Seek(here, SeekWhence::Set) != here)
                size = -1;
        }

        return size;
    }

    /*
        ssize_t read(int fd, void* buf, size_t count);

        std::size_t fread(void* buffer, std::size_t size, std::size_t count, std::FILE* stream);

        BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
    */
    usize Stream::Read(void*, usize)
    {
        return 0;
    }

    /*
        ssize_t write(int fd, const void* buf, size_t count);

        std::size_t fwrite(const void* buffer, std::size_t size, std::size_t count, std::FILE* stream);

        BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
    */
    usize Stream::Write(const void*, usize)
    {
        return 0;
    }

    /*
        ssize_t pread(int fd, void* buf, size_t count, off_t offset);

        BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
    */
    usize Stream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        return TrySeek(offset) ? Read(ptr, len) : 0;
    }

    /*
        ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);

        BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
    */
    usize Stream::WriteBulk(const void* ptr, usize len, i64 offset)
    {
        return TrySeek(offset) ? Write(ptr, len) : 0;
    }

    /*
        int fsync(int fd);
        int fdatasync(int fd);

        int fflush(std::FILE* stream);

        BOOL FlushFileBuffers(HANDLE hFile);
    */
    bool Stream::Flush()
    {
        return true;
    }

    /*
        int ftruncate(int fd, off_t length);

        BOOL SetEndOfFile(HANDLE hFile);
    */
    i64 Stream::SetSize(i64)
    {
        return -1;
    }

    i64 Stream::CopyTo(Stream& output)
    {
        i64 total = 0;

        usize buffer_len = static_cast<usize>(std::clamp<i64>(DistToEnd(), 4 * 1024, 64 * 1024));
        Ptr<u8[]> buffer = MakeUniqueUninit<u8[]>(buffer_len);

        while (true)
        {
            usize len = Read(buffer.get(), buffer_len);

            if (len == 0)
                break;

            len = output.Write(buffer.get(), len);

            if (len == 0)
                break;

            total += len;
        }

        return total;
    }

    Rc<Stream> Stream::GetBulkStream(i64&, i64)
    {
        return nullptr;
    }

    bool Stream::IsSynchronized()
    {
        return false;
    }

    Rc<Stream> Stream::Synchronized(Rc<Stream> stream)
    {
        if (stream->IsSynchronized())
            return stream;

        return MakeRc<SyncStream>(stream);
    }

    static const i64 MaxBlobSize = 1 << 30;
    static_assert(MaxBlobSize <= SIZE_MAX);

    String Stream::ReadText()
    {
        i64 length = DistToEnd();

        if (length < 0 || length > MaxBlobSize)
            throw std::runtime_error("Invalid stream size");

        String result(static_cast<usize>(length), '\0');
        result.resize(Read(result.data(), result.size()));
        return result;
    }

    Vec<u8> Stream::ReadBytes()
    {
        i64 length = DistToEnd();

        if (length < 0 || length > MaxBlobSize)
            throw std::runtime_error("Invalid stream size");

        Vec<u8> result(static_cast<usize>(length));
        result.resize(Read(result.data(), result.size()));
        return result;
    }
} // namespace Swage
