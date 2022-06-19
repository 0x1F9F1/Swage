#pragma once

namespace Swage
{
    class Cipher
    {
    public:
        virtual ~Cipher() = default;

        // Processing length bytes from input and writes them to output
        // input may equal output, otherwise input must not overlap with output
        virtual usize Update(const u8* input, u8* output, usize length) = 0;

        virtual usize GetBlockSize() = 0;

        inline usize Update(void* data, usize length)
        {
            return Update(static_cast<const u8*>(data), static_cast<u8*>(data), length);
        }
    };
} // namespace Swage
