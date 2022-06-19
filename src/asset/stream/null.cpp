#include "null.h"

namespace Swage
{
    NullStream::NullStream(i64 size)
        : size_(size)
    {}

    i64 NullStream::Seek(i64 offset, SeekWhence whence)
    {
        switch (whence)
        {
            case SeekWhence::Set: break;
            case SeekWhence::Cur: offset += current_; break;
            case SeekWhence::End: offset += size_; break;
        }

        current_ = std::clamp<i64>(offset, 0, size_);

        return current_;
    }

    i64 NullStream::Tell()
    {
        return current_;
    }

    i64 NullStream::Size()
    {
        return size_;
    }

    usize NullStream::Read(void* ptr, usize len)
    {
        std::memset(ptr, 0, len);

        return len;
    }

    usize NullStream::Write([[maybe_unused]] const void* ptr, usize len)
    {
        current_ += len;
        size_ = std::max<i64>(size_, current_);
        return len;
    }
} // namespace Swage