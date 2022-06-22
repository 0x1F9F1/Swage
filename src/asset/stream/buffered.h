#pragma once

#include "../stream.h"

namespace Swage
{
    class BufferedStream final : public Stream
    {
    public:
        BufferedStream(Rc<Stream> handle);
        ~BufferedStream() override;

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize Write(const void* ptr, usize len) override;

        usize ReadBulk(void* ptr, usize len, i64 offset) override;
        usize WriteBulk(const void* ptr, usize len, i64 offset) override;

        bool Flush() override;

        bool FlushBuffer();

        i32 GetCh();
        i32 UnGetCh(i32 ch);

        i32 PutCh(i32 ch);

        bool GetLine(String& output, char delim = '\n');

        void PutString(StringView str);

    private:
        i64 RawTell() const;
        i64 RawSize() const;
        i64 RawSeek(i64 offset, SeekWhence whence);

        usize RawRead(void* ptr, usize len);
        usize RawWrite(const void* ptr, usize len);

        bool SeekBuffer(i64 offset, SeekWhence whence);

        bool RawFlush();

        bool FlushReads();
        bool FlushWrites();

        // Underlying file handle
        Rc<Stream> handle_;

        // Buffer for more efficient reads/writes
        Ptr<u8[]> buffer_ {};

        // Cached file position, (position_ + buffer_read_ == RawTell())
        i64 position_ {-1};

        // Maximum capacity of the buffer
        u32 buffer_capacity_ {0};

        // Number of bytes read from or written into the buffer
        u32 buffer_head_ {0};

        // Number of bytes read from the file for buffering
        u32 buffer_read_ {0};
    };

    inline i64 BufferedStream::Tell()
    {
        return position_ + buffer_head_;
    }

    inline i32 BufferedStream::GetCh()
    {
        if (buffer_head_ < buffer_read_)
        {
            return buffer_[buffer_head_++];
        }

        u8 result = 0;

        if (Read(&result, 1))
        {
            return result;
        }

        return -1;
    }

    inline i32 BufferedStream::UnGetCh(i32 ch)
    {
        if (buffer_read_ != 0 && buffer_head_ != 0)
        {
            buffer_[--buffer_head_] = static_cast<u8>(ch);

            return ch;
        }

        return -1;
    }

    inline i32 BufferedStream::PutCh(i32 ch)
    {
        if (buffer_read_ == 0 && buffer_head_ < buffer_capacity_)
        {
            buffer_[buffer_head_++] = static_cast<u8>(ch);

            return ch;
        }

        u8 result = static_cast<u8>(ch);

        if (Write(&result, 1))
        {
            return ch;
        }

        return -1;
    }

    inline void BufferedStream::PutString(StringView str)
    {
        Write(str.data(), str.size());
    }
} // namespace Swage
