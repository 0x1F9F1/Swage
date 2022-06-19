#pragma once

namespace Swage
{
    class BinaryTransform
    {
    public:
        // Pointer and length of data waiting to be transformed
        const u8* NextIn {};
        usize AvailIn {};

        // Pointer and length of buffer to store transformed data
        u8* NextOut {};
        usize AvailOut {};

        // Indicates whether to expect any more input
        bool FinishedIn {};

        // Indicates whether to expect any more output
        bool FinishedOut {};

        virtual ~BinaryTransform() = default;

        virtual bool Reset() = 0;
        virtual bool Update() = 0;

        virtual usize GetOptimalBufferSize();
    };
} // namespace Swage
