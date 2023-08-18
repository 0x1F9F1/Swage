#include "deflate.h"

#include <zlib-ng.h>

namespace Swage
{
    static void* Z_alloc([[maybe_unused]] void* opaque, unsigned int items, unsigned int size)
    {
        return operator new(size* items);
    }

    static void Z_free([[maybe_unused]] void* opaque, void* address)
    {
        operator delete(address);
    }

    class DeflateDecompressor : public BinaryTransform
    {
    public:
        DeflateDecompressor(i32 window_bits);
        ~DeflateDecompressor() override;

        bool Reset() override;
        bool Update() override;

    private:
        zng_stream inflater_ {};
    };

    DeflateDecompressor::DeflateDecompressor(i32 window_bits)
    {
        inflater_.zalloc = Z_alloc;
        inflater_.zfree = Z_free;
        zng_inflateInit2(&inflater_, window_bits);
    }

    DeflateDecompressor::~DeflateDecompressor()
    {
        zng_inflateEnd(&inflater_);
    }

    bool DeflateDecompressor::Reset()
    {
        FinishedOut = false;

        return zng_inflateReset(&inflater_) == Z_OK;
    }

    bool DeflateDecompressor::Update()
    {
        int error = Z_OK;

        // Try and inflate as much data as possible
        while ((error == Z_OK) && (AvailIn || AvailOut))
        {
            uInt actual_in = (AvailIn <= UINT_MAX) ? uInt(AvailIn) : UINT_MAX;
            uInt actual_out = (AvailOut <= UINT_MAX) ? uInt(AvailOut) : UINT_MAX;

            inflater_.next_in = NextIn;
            inflater_.next_out = NextOut;

            inflater_.avail_in = actual_in;
            inflater_.avail_out = actual_out;

            // Always try and flush as much data as possible
            error = zng_inflate(&inflater_, Z_SYNC_FLUSH);

            AvailIn -= actual_in - inflater_.avail_in;
            AvailOut -= actual_out - inflater_.avail_out;

            NextIn = inflater_.next_in;
            NextOut = inflater_.next_out;
        }

        // No progress was possible (avail_in or avail_out was zero)
        if (error == Z_BUF_ERROR)
        {
            // We have sufficient output capacity, but there is no more input
            if (AvailOut && (AvailIn || FinishedIn))
                return false;

            // The stream is valid, but just needs more input/output
            return true;
        }

        // The end of the compressed data has been reached and all uncompressed output has been produced
        if (error == Z_STREAM_END)
        {
            // We've already reached the end once, so now report it as an error
            if (FinishedOut)
                return false;

            FinishedOut = true;
            return true;
        }

        return error == Z_OK;
    }

    Ptr<BinaryTransform> CreateDeflateDecompressor(i32 window_bits)
    {
        return swnew DeflateDecompressor(window_bits);
    }
} // namespace Swage
