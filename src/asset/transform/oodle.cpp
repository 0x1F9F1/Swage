#include "oodle.h"

#include "core/mutex.h"
#include "core/platform/procs.h"

namespace Swage
{
#define OODLELZ_FAILED 0

#if defined(_WIN32)
#    define OODLEAPI __stdcall
#else
#    define OODLEAPI
#endif

    enum OodleLZ_Verbosity : i32
    {
        OodleLZ_Verbosity_None,
        OodleLZ_Verbosity_Max = 3, // Unknown Name
    };

    enum OodleLZ_FuzzSafe : i32
    {
        OodleLZ_FuzzSafe_No,
        OodleLZ_FuzzSafe_Yes,
    };

    enum OodleLZ_CheckCRC : i32
    {
        OodleLZ_CheckCRC_No,
        OodleLZ_CheckCRC_Yes,
    };

    enum OodleLZ_Decode_ThreadPhase : i32
    {
        OodleLZ_Decode_ThreadPhase1 = 0x1,
        OodleLZ_Decode_ThreadPhase2 = 0x2,

        OodleLZ_Decode_Unthreaded = OodleLZ_Decode_ThreadPhase1 | OodleLZ_Decode_ThreadPhase2,
    };

    // Used to indicate the progress of OodleLZDecoder_DecodeSome
    // If OodleLZDecoder_DecodeSome returns true, it should always contain valid/zeroed data
    struct OodleLZ_DecodeSome_Out
    {
        // Number of uncompressed bytes decoded
        // This will only be non-zero if the whole block/quantum was decoded
        u32 decodedCount;

        // Number of compressed bytes consumed
        // If only the block header is consumed, this will be zero
        // If the quantum header is consumed
        // This may be non-zero while decodedCount is zero, if only the quantum head
        u32 compBufUsed;

        // You must have at least this much room available in the output buffer
        // Should always be <= OODLELZ_BLOCK_LEN
        // Ignore if decodedCount != 0
        u32 curQuantumRawLen;

        // Amount of input data needed to decode the current quantum.
        // Should always be <= OODLELZ_BLOCK_MAX_COMPLEN
        // Ignore if decodedCount != 0
        u32 curQuantumCompLen;
    };

    static union
    {
        struct
        {
            i32(OODLEAPI* LZDecoder_MemorySizeNeeded)(OodleLZ_Compressor compressor, isize rawLen);

            OodleLZDecoder*(OODLEAPI* LZDecoder_Create)(
                OodleLZ_Compressor compressor, i64 rawLen, void* memory, usize memorySize);

            i32(OODLEAPI* LZDecoder_Reset)(OodleLZDecoder* decoder, isize decPos, isize decLen);

            void(OODLEAPI* LZDecoder_Destroy)(OodleLZDecoder* decoder);

            i32(OODLEAPI* LZDecoder_DecodeSome)(OodleLZDecoder* decoder, OodleLZ_DecodeSome_Out* out, void* decBuf,
                isize decBufPos, isize decBufferSize, isize decBufAvail, const void* compPtr, isize compAvail,
                OodleLZ_FuzzSafe fuzzSafe, OodleLZ_CheckCRC checkCRC, OodleLZ_Verbosity verbosity,
                OodleLZ_Decode_ThreadPhase threadPhase);
        } Oodle;

        ProcAddr OodleProcs[5];

        static_assert(sizeof(Oodle) == sizeof(OodleProcs));
    };

#if defined(_WIN32) && !defined(_WIN64)
    static const char* const OodleProcNames[] {
        "_OodleLZDecoder_MemorySizeNeeded@8",
        "_OodleLZDecoder_Create@20",
        "_OodleLZDecoder_Reset@12",
        "_OodleLZDecoder_Destroy@4",
        "_OodleLZDecoder_DecodeSome@48",
        nullptr,
    };
#else
    static const char* const OodleProcNames[] {
        "OodleLZDecoder_MemorySizeNeeded",
        "OodleLZDecoder_Create",
        "OodleLZDecoder_Reset",
        "OodleLZDecoder_Destroy",
        "OodleLZDecoder_DecodeSome",
        nullptr,
    };
#endif

#if defined(_WIN64)
#    define OODLE_LIB_SUFFIX "win64"
#elif defined(_WIN32)
#    define OODLE_LIB_SUFFIX "win32"
#else
#    define OODLE_LIB_SUFFIX "unknown"
#endif

    static bool LoadOodle()
    {
        static std::once_flag load_once;
        static bool is_loaded = false;

        std::call_once(load_once, [] {
            is_loaded = false;

            for (i32 i = 9; i >= 5; --i)
            {
                String dll_name = fmt::format("oo2core_{}_{}", i, OODLE_LIB_SUFFIX);

                if (LoadProcs(dll_name.c_str(), OodleProcNames, OodleProcs))
                {
                    SwLogInfo("Loaded {}", dll_name);
                    is_loaded = true;
                    break;
                }
            }
        });

        return is_loaded;
    }

    OodleDecompressor::OodleDecompressor(i64 size, OodleLZ_Compressor compressor)
        : real_size_(static_cast<isize>(size))
    {
        if (size != real_size_)
            throw std::runtime_error("Oodle cannot decompress files larger than SSIZE_MAX");

        if (!LoadOodle())
            throw std::runtime_error("Failed to load Oodle");

        usize length = Oodle.LZDecoder_MemorySizeNeeded(compressor, -1);
        state_ = operator new(length);
        decoder_ = Oodle.LZDecoder_Create(compressor, real_size_, state_, length);
    }

    OodleDecompressor::~OodleDecompressor()
    {
        Oodle.LZDecoder_Destroy(decoder_);

        operator delete(state_);
    }

    bool OodleDecompressor::Reset()
    {
        total_out_ = 0;

        needed_in_ = 0;
        buffered_in_ = 0;

        output_start_ = 0;
        output_end_ = 0;

        FinishedOut = false;

        return Oodle.LZDecoder_Reset(decoder_, 0, 0);
    }

    bool OodleDecompressor::Update()
    {
        while (true)
        {
            // Try and flush any pending output
            if (i32 error = FlushOutput())
                return error > 0;

            // Ensure we have enough input
            if (i32 error = ConsumeInput())
                return error > 0;

            // Try and decode the input
            if (i32 error = DecodeSome())
                return error > 0;
        }
    }

    i32 OodleDecompressor::FlushOutput()
    {
        SwAssert(output_start_ <= output_end_);

        // Check if we have any buffered data to flush
        if (usize buffered = output_end_ - output_start_)
        {
            // See how much of the buffered data we can write
            usize avail = std::min<usize>(buffered, AvailOut);

            // If we can write any output, do it
            if (avail)
            {
                std::memcpy(NextOut, &buffer_out_[output_start_], avail);
                NextOut += avail;
                AvailOut -= avail;

                output_start_ += avail;
                total_out_ += avail;
                buffered -= avail;
            }

            // There is still data to write, so return
            if (buffered)
                return 1;

            SwAssert(output_start_ == output_end_);

            // Time to start a new block
            if (output_end_ == OODLELZ_BLOCK_LEN)
            {
                output_start_ = 0;
                output_end_ = 0;
            }
        }

        // Check if we've decompressed the whole thing
        if (total_out_ >= real_size_)
        {
            // We've already reached the end once, so now report it as an error
            if (FinishedOut)
                return -1;

            FinishedOut = true;
            return 1;
        }

        return 0;
    }

    i32 OodleDecompressor::ConsumeInput()
    {
        SwAssert(buffered_in_ <= needed_in_);

        // Check if we need more input
        if (usize needed = needed_in_ - buffered_in_)
        {
            // See how much of that input we can read
            usize avail = std::min<usize>(needed, AvailIn);

            // If we can read any, do it
            if (avail)
            {
                std::memcpy(&buffer_in_[buffered_in_], NextIn, avail);
                NextIn += avail;
                AvailIn -= avail;

                buffered_in_ += avail;
                needed -= avail;
            }

            if (FinishedIn)
            {
                // If there is no more input, try and process what we have
                if (buffered_in_)
                    needed_in_ = buffered_in_;
                else if (avail == 0)
                    return -1;
            }
            else if (needed)
            {
                return 1;
            }
        }

        return 0;
    }

    i32 OodleDecompressor::DecodeSome()
    {
        // TODO: When possible, decode directly to NextOut

        // Older versions of Oodle would report an error when given 0 bytes of input
        if (buffered_in_ == 0)
        {
            needed_in_ = 16;

            return 0;
        }

        OodleLZ_DecodeSome_Out out {};

        if (Oodle.LZDecoder_DecodeSome(decoder_, &out, buffer_out_, output_end_, real_size_,
                sizeof(buffer_out_) - output_end_, buffer_in_, buffered_in_, OodleLZ_FuzzSafe_No, OodleLZ_CheckCRC_Yes,
                OodleLZ_Verbosity_None, OodleLZ_Decode_Unthreaded) <= 0)
            return -1;

        // Check if the any input was consumed
        if (out.compBufUsed)
        {
            SwAssert(out.compBufUsed <= buffered_in_);

            // Check if there is any extra data in the input buffer
            usize extra = buffered_in_ - out.compBufUsed;

            if (extra)
            {
                // This should never be more than a few bytes
                std::memmove(buffer_in_, &buffer_in_[out.compBufUsed], extra);
            }

            buffered_in_ = extra;
        }

        needed_in_ = buffered_in_;

        if (out.decodedCount)
        {
            output_end_ += out.decodedCount;

            SwAssert(output_end_ <= sizeof(buffer_out_));
        }
        else
        {
            if (out.curQuantumRawLen)
            {
                if (output_end_ + out.curQuantumRawLen > sizeof(buffer_out_))
                    return -1;
            }

            if (out.curQuantumCompLen)
            {
                if (out.curQuantumCompLen > sizeof(buffer_in_))
                    return -1;

                needed_in_ = std::max<usize>(needed_in_, out.curQuantumCompLen);
            }
        }

        // Always try and request slightly more than necessary, to avoid wasting DecomeSome calls
        needed_in_ = std::min<usize>(needed_in_ + 16, sizeof(buffer_in_));

        return 0;
    }

    usize OodleDecompressor::GetOptimalBufferSize()
    {
        return OODLELZ_BLOCK_LEN;
    }
} // namespace Swage
