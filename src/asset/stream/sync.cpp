#include "sync.h"

namespace Swage
{
    SyncStream::SyncStream(Rc<Stream> input)
        : input_(std::move(input))
    {}

    i64 SyncStream::Seek(i64 offset, SeekWhence whence)
    {
        LockGuard<Mutex> lock(lock_);

        return input_->Seek(offset, whence);
    }

    i64 SyncStream::Tell()
    {
        LockGuard<Mutex> lock(lock_);

        return input_->Tell();
    }

    i64 SyncStream::Size()
    {
        LockGuard<Mutex> lock(lock_);

        return input_->Size();
    }

    usize SyncStream::Read(void* ptr, usize len)
    {
        LockGuard<Mutex> lock(lock_);

        return input_->Read(ptr, len);
    }

    usize SyncStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        LockGuard<Mutex> lock(lock_);

        return input_->ReadBulk(ptr, len, offset);
    }
} // namespace Swage
