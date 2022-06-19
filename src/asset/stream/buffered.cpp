#include "buffered.h"

#include "../assetmanager.h"

namespace Swage
{
    BufferedStream::BufferedStream(Rc<Stream> handle)
        : handle_(std::move(handle))
    {
        buffer_capacity_ = 4096;
        buffer_.reset(new u8[buffer_capacity_]);
        position_ = RawTell();
    }

    BufferedStream::~BufferedStream()
    {
        FlushWrites();
    }

    i64 BufferedStream::Size()
    {
        if (!FlushWrites())
        {
            return -1;
        }

        return RawSize();
    }

    i64 BufferedStream::Seek(i64 offset, SeekWhence whence)
    {
        if (SeekBuffer(offset, whence))
        {
            return position_ + buffer_head_;
        }

        if (buffer_read_ != 0)
        {
            if (whence == SeekWhence::Cur)
            {
                offset -= buffer_read_ - buffer_head_;
            }
        }
        else if (!FlushWrites())
        {
            return -1;
        }

        position_ = RawSeek(offset, whence);

        buffer_head_ = 0;
        buffer_read_ = 0;

        return position_;
    }

    usize BufferedStream::Read(void* ptr, usize len)
    {
        SwDebugAssert(position_ != -1);

        if (!FlushWrites())
        {
            return 0;
        }

        usize total = 0;
        usize const buffered = buffer_read_ - buffer_head_;

        if (len > buffered)
        {
            if (buffered != 0)
            {
                std::memcpy(ptr, &buffer_[buffer_head_], buffered);
                buffer_head_ = buffer_read_;

                ptr = static_cast<u8*>(ptr) + buffered;
                len -= buffered;
                total += buffered;
            }

            position_ += buffer_head_;

            if (len >= buffer_capacity_)
            {
                usize const raw_read = RawRead(ptr, len);

                buffer_head_ = 0;
                buffer_read_ = 0;

                position_ += raw_read;
                total += raw_read;

                return total;
            }

            usize const raw_read = RawRead(&buffer_[0], buffer_capacity_);

            buffer_head_ = 0;
            buffer_read_ = static_cast<u32>(raw_read);

            if (len > raw_read)
                len = raw_read;
        }

        std::memcpy(ptr, &buffer_[buffer_head_], len);
        buffer_head_ += static_cast<u32>(len);
        total += len;

        return total;
    }

    usize BufferedStream::Write(const void* ptr, usize len)
    {
        SwDebugAssert(position_ != -1);

        if (!FlushReads())
        {
            return 0;
        }

        if (len < buffer_capacity_)
        {
            usize total = 0;

            usize const available = buffer_capacity_ - buffer_head_;

            if (len >= available)
            {
                std::memcpy(&buffer_[buffer_head_], ptr, available);

                ptr = static_cast<const u8*>(ptr) + available;
                len -= available;
                total += available;

                usize const written = RawWrite(&buffer_[0], buffer_capacity_);

                position_ += written;
                buffer_head_ = 0;

                if (written != buffer_capacity_)
                {
                    return available;
                }
            }

            std::memcpy(&buffer_[buffer_head_], ptr, len);

            buffer_head_ += static_cast<u32>(len);

            total += len;

            return total;
        }
        else if (FlushWrites())
        {
            usize const written = RawWrite(ptr, len);

            position_ += written;

            return written;
        }
        else
        {
            return 0;
        }
    }

    usize BufferedStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        if (!FlushWrites())
        {
            return 0;
        }

        position_ = -1;

        return handle_->ReadBulk(ptr, len, offset);
    }

    usize BufferedStream::WriteBulk(const void* ptr, usize len, i64 offset)
    {
        if (!FlushWrites())
        {
            return 0;
        }

        position_ = -1;

        return handle_->WriteBulk(ptr, len, offset);
    }

    bool BufferedStream::GetLine(String& output, char delim)
    {
        usize total = 0;

        while (true)
        {
            i32 const cur = GetCh();

            if (cur == -1)
            {
                if (total == 0)
                    return false;

                break;
            }

            if (cur == delim)
                break;

            output.push_back(static_cast<char>(cur));

            ++total;
        }

        if (total && delim == '\n' && output.back() == '\r')
        {
            output.pop_back();
        }

        return true;
    }

    Ptr<BufferedStream> BufferedStream::Open(StringView path, bool read_only)
    {
        Rc<Stream> handle = AssetManager::Open(path, read_only);

        if (!handle)
            return nullptr;

        return MakeUnique<BufferedStream>(std::move(handle));
    }

    Ptr<BufferedStream> BufferedStream::Create(StringView path, bool write_only, bool truncate)
    {
        Rc<Stream> handle = AssetManager::Create(path, write_only, truncate);

        if (!handle)
            return nullptr;

        return MakeUnique<BufferedStream>(std::move(handle));
    }

    bool BufferedStream::FlushBuffer()
    {
        return FlushReads() && FlushWrites();
    }

    bool BufferedStream::Flush()
    {
        return FlushBuffer() && RawFlush();
    }

    inline i64 BufferedStream::RawTell() const
    {
        return handle_->Tell();
    }

    inline i64 BufferedStream::RawSize() const
    {
        return handle_->Size();
    }

    inline i64 BufferedStream::RawSeek(i64 offset, SeekWhence whence)
    {
        return handle_->Seek(offset, whence);
    }

    inline usize BufferedStream::RawRead(void* ptr, usize len)
    {
        return handle_->Read(ptr, len);
    }

    inline usize BufferedStream::RawWrite(const void* ptr, usize len)
    {
        return handle_->Write(ptr, len);
    }

    inline bool BufferedStream::SeekBuffer(i64 offset, SeekWhence whence)
    {
        if (buffer_read_ == 0)
            return false;

        switch (whence)
        {
            case SeekWhence::Set: offset -= position_; break;
            case SeekWhence::Cur: offset += buffer_head_; break;
            case SeekWhence::End: return false;
        }

        if ((offset < 0) || (offset > buffer_read_))
            return false;

        buffer_head_ = static_cast<u32>(offset);

        return true;
    }

    inline bool BufferedStream::RawFlush()
    {
        return handle_->Flush();
    }

    inline bool BufferedStream::FlushReads()
    {
        if (buffer_read_ != 0 && buffer_read_ != buffer_head_)
        {
            position_ = RawSeek(position_ + buffer_head_, SeekWhence::Set);

            buffer_head_ = 0;
            buffer_read_ = 0;

            return position_ != -1;
        }
        else
        {
            return true;
        }
    }

    inline bool BufferedStream::FlushWrites()
    {
        if (buffer_read_ == 0 && buffer_head_ != 0)
        {
            // TODO: Better handle incomplete writes
            usize const written = RawWrite(&buffer_[0], buffer_head_);

            SwDebugAssert((written == 0) || (written == buffer_head_));

            position_ += written;
            buffer_head_ = 0;

            return written != 0;
        }
        else
        {
            return true;
        }
    }
} // namespace Swage
