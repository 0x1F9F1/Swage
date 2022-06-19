#include "bulk.h"

namespace Swage
{
    BulkStream::BulkStream(Rc<Stream> handle)
        : input_(handle)
    {}

    i64 BulkStream::Seek(i64 offset, SeekWhence whence)
    {
        switch (whence)
        {
            case SeekWhence::Set: break;
            case SeekWhence::Cur: offset += here_; break;
            case SeekWhence::End: offset += input_->Size(); break;
        }

        if (here_ < 0)
            here_ = -1;

        return here_;
    }

    i64 BulkStream::Tell()
    {
        return here_;
    }

    i64 BulkStream::Size()
    {
        return input_->Size();
    }

    usize BulkStream::Read(void* ptr, usize len)
    {
        usize result = ReadInternal(ptr, len, here_);

        here_ += result;

        return result;
    }

    usize BulkStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        return ReadInternal(ptr, len, offset);
    }

    bool BulkStream::IsSynchronized()
    {
        return input_->IsSynchronized();
    }

    Rc<Stream> BulkStream::GetBulkStream(i64&, i64)
    {
        return input_;
    }

    usize BulkStream::ReadInternal(void* ptr, usize len, i64 offset)
    {
        if (offset < 0)
            return 0;

        return input_->ReadBulk(ptr, len, offset);
    }
} // namespace Swage
