#pragma once

#include "fileapi.h"

namespace Swage
{
    class Stream : public AtomicRefCounted
    {
    public:
        // Sets the current file position
        // Returns the new file position, or -1 on error
        virtual i64 Seek(i64 position, SeekWhence whence);

        // Retrieves the current file position
        // Returns the current file position, or -1 on error
        virtual i64 Tell();

        // Retrieves the file size
        // Returns the file size, or -1 on error
        virtual i64 Size();

        // Reads up to len bytes from the current file position
        // Increments the file position by the number of bytes read
        // Returns the number of bytes read
        virtual usize Read(void* ptr, usize len);

        // Writes up to len bytes to the current file position
        // Increments the file position by the number of bytes written
        // Returns the number of bytes written
        virtual usize Write(const void* ptr, usize len);

        // Reads up to len bytes from the specified file position
        // May modify the current file position
        // Returns the number of bytes read
        virtual usize ReadBulk(void* ptr, usize len, i64 offset);

        // Writes up to len bytes to the specified file position
        // May modify the current file position
        // Returns the number of bytes written
        virtual usize WriteBulk(const void* ptr, usize len, i64 offset);

        // Flushes any buffers, causing buffered data to be written to the underlying file
        // May modify the current file position
        // Returns true if successful
        virtual bool Flush();

        // Sets the file size
        // May modify the current file position
        // Returns the new file size, or -1 on error
        virtual i64 SetSize(i64 length);

        // Copies current stream contents to output
        // Returns number of bytes copied
        virtual i64 CopyTo(Stream& output);

        // Retreives the underlying file handle usable for bulk operations
        // Returns the bulk file handle, and adjusts offset as required
        virtual Rc<Stream> GetBulkStream(i64& offset, i64 size);

        virtual bool IsSynchronized();

        // virtual bool IsCompressed()
        // virtual bool IsEncrypted()
        // virtual bool IsRemote()
        // virtual bool IsBuffered();

        // static Rc<Stream> Buffered(Rc<Stream> stream);

        static Rc<Stream> Synchronized(Rc<Stream> stream);

        bool Rewind();
        i64 DistToEnd();

        [[nodiscard]] bool TrySeek(u64 offset);

        [[nodiscard]] bool TryRead(void* ptr, usize len);
        [[nodiscard]] bool TryReadBulk(void* ptr, usize len, i64 offset);

        [[nodiscard]] bool TryWrite(const void* ptr, usize len);

        String ReadText();
        Vec<u8> ReadBytes();
    };

    template <typename T>
    inline usize ByteSize(T& values)
    {
        return values.size() * sizeof(*values.data());
    }

    template <typename T, usize N>
    constexpr bool is_c_struct_v = (sizeof(T) == N && std::is_standard_layout_v<T> && std::is_trivial_v<T>);

    inline bool Stream::Rewind()
    {
        return TrySeek(0);
    }

    inline i64 Stream::DistToEnd()
    {
        i64 here = Tell();
        i64 size = Size();
        return (here >= 0 && here <= size) ? (size - here) : -1;
    }

    inline bool Stream::TrySeek(u64 offset)
    {
        return Seek(static_cast<i64>(offset), SeekWhence::Set) == static_cast<i64>(offset);
    }

    inline bool Stream::TryRead(void* ptr, usize len)
    {
        return Read(ptr, len) == len;
    }

    inline bool Stream::TryReadBulk(void* ptr, usize len, i64 offset)
    {
        return ReadBulk(ptr, len, offset) == len;
    }

    inline bool Stream::TryWrite(const void* ptr, usize len)
    {
        return Write(ptr, len) == len;
    }
} // namespace Swage
