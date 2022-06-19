#include "lzss.h"

namespace Swage
{
    LzssDecompressor::LzssDecompressor()
    {
        Reset();
    }

    bool LzssDecompressor::Reset()
    {
        format_ = 0;
        buffered_ = 0;
        current_ = 0;
        pending_ = 0;

        std::memset(window_, 0x20, sizeof(window_));

        return true;
    }

    bool LzssDecompressor::Update()
    {
        if (pending_)
        {
            FlushPending();

            if (pending_)
                return true;
        }

        if (AvailIn == 0)
            return false;

        while (AvailIn && AvailOut)
        {
            if ((format_ & 0x100) == 0)
            {
                format_ = 0xFF00 | u16(*NextIn++);

                if (--AvailIn == 0)
                    break;
            }

            if (format_ & 0x1)
            {
                if (pending_ > 0xFFF)
                    FlushPending();

                window_[current_++ & 0xFFF] = *NextIn++;
                ++pending_;
                --AvailIn;

                format_ >>= 1;
            }
            else
            {
                for (; AvailIn && buffered_ < 2; --AvailIn)
                    buffer_[buffered_++] = *NextIn++;

                if (buffered_ != 2)
                    break;

                u8 len = (buffer_[1] & 0xF) + 3;
                u16 const off = (static_cast<u16>(buffer_[1] & 0xF0) << 4) | buffer_[0];

                if (pending_ + len > 0x1000)
                {
                    FlushPending();

                    if (pending_ + len > 0x1000)
                        break;
                }

                u16 dst = current_;

                pending_ += len;
                current_ += len;

                if (off)
                {
                    u16 src = dst - off;

                    for (; len; --len, ++src, ++dst)
                        window_[dst & 0xFFF] = window_[src & 0xFFF];
                }

                buffered_ = 0;
                format_ >>= 1;
            }
        }

        return true;
    }

    void LzssDecompressor::FlushPending()
    {
        while (pending_ && AvailOut)
        {
            u16 len = pending_;

            if (len > AvailOut)
                len = static_cast<u16>(AvailOut);

            u16 src = (current_ - pending_) & 0xFFF;

            if (len > (0x1000 - src))
                len = (0x1000 - src);

            std::memcpy(NextOut, &window_[src], len);

            NextOut += len;
            AvailOut -= len;
            pending_ -= len;
        }
    }
} // namespace Swage
