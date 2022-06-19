#include "memory.h"

namespace Swage
{
    MemoryStream::MemoryStream(void* data, usize size)
        : current_(0)
        , size_(static_cast<i64>(size))
        , data_(static_cast<u8*>(data))
    {}

    i64 MemoryStream::Seek(i64 offset, SeekWhence whence)
    {
        switch (whence)
        {
            case SeekWhence::Set: break;
            case SeekWhence::Cur: offset += current_; break;
            case SeekWhence::End: offset += size_; break;
        }

        offset = std::clamp<i64>(offset, 0, size_);

        current_ = offset;

        return offset;
    }

    i64 MemoryStream::Tell()
    {
        return current_;
    }

    i64 MemoryStream::Size()
    {
        return size_;
    }

    usize MemoryStream::Read(void* ptr, usize len)
    {
        usize result = ReadInternal(ptr, len, current_);

        current_ += result;

        return result;
    }

    usize MemoryStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        return ReadInternal(ptr, len, offset);
    }

    i64 MemoryStream::CopyTo(Stream& output)
    {
        if (current_ < 0 || current_ >= size_)
            return 0;

        return output.Write(data_ + current_, static_cast<usize>(size_ - current_));
    }

    bool MemoryStream::IsSynchronized()
    {
        return true;
    }

    usize MemoryStream::ReadInternal(void* ptr, usize len, i64 offset)
    {
        if (offset < 0 || offset >= size_)
            return 0;

        len = static_cast<usize>(std::min<i64>(size_ - offset, len));

        std::memcpy(ptr, data_ + offset, len);

        return len;
    }
} // namespace Swage
