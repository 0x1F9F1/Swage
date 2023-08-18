#include "zstd.h"

#define ZSTD_DLL_IMPORT 1
#include <zstd.h>

namespace Swage
{
    class ZstdDecompressor : public BinaryTransform
    {
    public:
        ZstdDecompressor();
        ~ZstdDecompressor() override;

        bool Reset() override;
        bool Update() override;

    private:
        void* stream_ {};
    };

    ZstdDecompressor::ZstdDecompressor()
    {
        stream_ = ZSTD_createDStream();
    }

    ZstdDecompressor::~ZstdDecompressor()
    {
        ZSTD_freeDStream(static_cast<ZSTD_DStream*>(stream_));
    }

    bool ZstdDecompressor::Reset()
    {
        FinishedOut = false;

        return ZSTD_initDStream(static_cast<ZSTD_DStream*>(stream_)) == 0;
    }

    bool ZstdDecompressor::Update()
    {
        size_t error = 0;

        while ((error == 0) && (AvailIn || AvailOut))
        {
            ZSTD_inBuffer input {};
            input.src = NextIn;
            input.size = AvailIn;
            input.pos = 0;

            ZSTD_outBuffer output {};
            output.dst = NextOut;
            output.size = AvailOut;
            output.pos = 0;

            error = ZSTD_decompressStream(static_cast<ZSTD_DStream*>(stream_), &output, &input);

            AvailIn -= input.pos;
            AvailOut -= output.pos;

            NextIn += input.pos;
            NextOut += output.pos;

            if (ZSTD_isError(error))
            {
                return false;
            }

            // No more room for output
            if (output.pos == output.size)
            {
                break;
            }

            // Need more input
            if (input.pos < input.size)
            {
                break;
            }

            //if (output.pos < output.size)
            //{
            //    break;
            //}
        }

        return true;
    }

    Ptr<BinaryTransform> CreateZstdDecompressor()
    {
        return swnew ZstdDecompressor();
    }
} // namespace Swage
