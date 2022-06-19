#include "lzxd.h"

#include "core/platform/procs.h"

namespace Swage
{
#define XCOMP_API __stdcall

    typedef enum _XMEMCODEC_TYPE
    {
        XMEMCODEC_DEFAULT = 0,
        XMEMCODEC_LZX = 1
    } XMEMCODEC_TYPE;

    typedef struct _XMEMCODEC_PARAMETERS_LZX
    {
        u32 Flags;
        u32 WindowSize;
        u32 CompressionPartitionSize;
    } XMEMCODEC_PARAMETERS_LZX;

#define XMEMCOMPRESS_STREAM 0x00000001

    static union
    {
        struct
        {
            usize(XCOMP_API* GetDecompressionContextSize)(
                XMEMCODEC_TYPE CodecType, const void* pCodecParams, u32 Flags);
            void*(XCOMP_API* InitializeDecompressionContext)(
                XMEMCODEC_TYPE CodecType, const void* pCodecParams, u32 Flags, void* pContextData, usize ContextSize);
            void(XCOMP_API* DestroyDecompressionContext)(void* Context);
            long(XCOMP_API* ResetDecompressionContext)(void* Context);
            long(XCOMP_API* DecompressStream)(
                void* Context, void* pDestination, usize* pDestSize, const void* pSource, usize* pSrcSize);
        } XCompress;

        ProcAddr XCompressProcs[5];

        static_assert(sizeof(XCompress) == sizeof(XCompressProcs));
    };

    static const char* const XCompressProcNames[] {"XMemGetDecompressionContextSize",
        "XMemInitializeDecompressionContext", "XMemDestroyDecompressionContext", "XMemResetDecompressionContext",
        "XMemDecompressStream", nullptr};

#if defined(_WIN64)
#    define XCOMPRESS_LIB_NAME "xcompress64"
#elif defined(_WIN32)
#    define XCOMPRESS_LIB_NAME "xcompress32"
#else
#    define XCOMPRESS_LIB_NAME "xcompress"
#endif

    static bool LoadXCompress()
    {
        static std::once_flag load_once;
        static bool is_loaded = false;

        std::call_once(
            load_once, [] { is_loaded = LoadProcs(XCOMPRESS_LIB_NAME, XCompressProcNames, XCompressProcs); });

        return is_loaded;
    }

    LzxdDecompressor::LzxdDecompressor()
    {
        if (!LoadXCompress())
            throw std::runtime_error("Failed to load XCompress");

        XMEMCODEC_PARAMETERS_LZX codec_params {};

        codec_params.Flags = 0;
        codec_params.WindowSize = 64 * 1024;
        codec_params.CompressionPartitionSize = 256 * 1024;

        usize length = XCompress.GetDecompressionContextSize(XMEMCODEC_LZX, &codec_params, XMEMCOMPRESS_STREAM);

        state_ = operator new(length);

        // context should always equal state_ (or null)
        context_ =
            XCompress.InitializeDecompressionContext(XMEMCODEC_LZX, &codec_params, XMEMCOMPRESS_STREAM, state_, length);
    }

    LzxdDecompressor::~LzxdDecompressor()
    {
        // Should be a no-op since we allocated our own memory
        XCompress.DestroyDecompressionContext(context_);

        operator delete(state_);
    }

    bool LzxdDecompressor::Reset()
    {
        return XCompress.ResetDecompressionContext(context_) >= 0;
    }

    bool LzxdDecompressor::Update()
    {
        usize input_len = AvailIn;
        usize output_len = AvailOut;

        long error = XCompress.DecompressStream(context_, NextOut, &output_len, NextIn, &input_len);

        NextIn += input_len;
        AvailIn -= input_len;

        NextOut += output_len;
        AvailOut -= output_len;

        return error >= 0;
    }
} // namespace Swage
