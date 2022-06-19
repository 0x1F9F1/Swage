#include "partial.h"

namespace Swage
{
    PartialStream::PartialStream(i64 start, i64 size, const Rc<Stream>& handle)
        : start_(start)
        , size_(size)
        , here_(0)
        , input_(handle->GetBulkStream(start_, size_))
    {
        if (input_ == nullptr)
        {
            start_ = start;
            input_ = handle;
        }

        if (!input_->IsSynchronized())
            SwLogFatal("PartialStream: Input not synchronized");
    }

    i64 PartialStream::Seek(i64 offset, SeekWhence whence)
    {
        switch (whence)
        {
            case SeekWhence::Set: break;
            case SeekWhence::Cur: offset += here_; break;
            case SeekWhence::End: offset += size_; break;
        }

        here_ = std::clamp<i64>(offset, 0, size_);

        return here_;
    }

    i64 PartialStream::Tell()
    {
        return here_;
    }

    i64 PartialStream::Size()
    {
        return size_;
    }

    usize PartialStream::Read(void* ptr, usize len)
    {
        usize result = ReadInternal(ptr, len, here_);

        here_ += result;

        return result;
    }

    usize PartialStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        return ReadInternal(ptr, len, offset);
    }

    bool PartialStream::IsSynchronized()
    {
        return true;
    }

    Rc<Stream> PartialStream::GetBulkStream(i64& offset, i64 size)
    {
        if (size <= size_)
        {
            offset += start_;

            return input_;
        }

        return nullptr;
    }

    usize PartialStream::ReadInternal(void* ptr, usize len, i64 offset)
    {
        if (offset < 0 || offset >= size_)
            return 0;

        return input_->ReadBulk(ptr, static_cast<usize>(std::min<i64>(size_ - offset, len)), start_ + offset);
    }
} // namespace Swage
