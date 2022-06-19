import sys
import os
import ctypes
from enum import Enum

from rsc import read_rsc7, read_rsc8

buffer_format_names = [
    "UNKNOWN","R32G32B32A32_TYPELESS","R32G32B32A32_FLOAT","R32G32B32A32_UINT","R32G32B32A32_SINT","R32G32B32_TYPELESS","R32G32B32_FLOAT","R32G32B32_UINT","R32G32B32_SINT","R16G16B16A16_TYPELESS","R16G16B16A16_FLOAT","R16G16B16A16_UNORM","R16G16B16A16_UINT","R16G16B16A16_SNORM","R16G16B16A16_SINT","R32G32_TYPELESS","R32G32_FLOAT","R32G32_UINT","R32G32_SINT",None,"D32_FLOAT_S8X24_UINT","B10G10R10A2_UNORM","R10G10B10A2_SNORM","R10G10B10A2_TYPELESS","R10G10B10A2_UNORM","R10G10B10A2_UINT","R11G11B10_FLOAT","R8G8B8A8_TYPELESS","R8G8B8A8_UNORM","R8G8B8A8_UNORM_SRGB","R8G8B8A8_UINT","R8G8B8A8_SNORM","R8G8B8A8_SINT","R16G16_TYPELESS","R16G16_FLOAT","R16G16_UNORM","R16G16_UINT","R16G16_SNORM","R16G16_SINT","R32_TYPELESS","D32_FLOAT","R32_FLOAT","R32_UINT","R32_SINT",None,None,None,None,"R8G8_TYPELESS","R8G8_UNORM","R8G8_UINT","R8G8_SNORM","R8G8_SINT","R16_TYPELESS","R16_FLOAT","D16_UNORM","R16_UNORM","R16_UINT","R16_SNORM","R16_SINT","R8_TYPELESS","R8_UNORM","R8_UINT","R8_SNORM","R8_SINT","A8_UNORM",None,"R9G9B9E5_SHAREDEXP",None,None,"BC1_TYPELESS","BC1_UNORM","BC1_UNORM_SRGB","BC2_TYPELESS","BC2_UNORM","BC2_UNORM_SRGB","BC3_TYPELESS","BC3_UNORM","BC3_UNORM_SRGB","BC4_TYPELESS","BC4_UNORM","BC4_SNORM","BC5_TYPELESS","BC5_UNORM","BC5_SNORM","B5G6R5_UNORM","B5G5R5A1_UNORM","B8G8R8A8_UNORM",None,None,"B8G8R8A8_TYPELESS","B8G8R8A8_UNORM_SRGB",None,None,"BC6H_TYPELESS","BC6H_UF16","BC6H_SF16","BC7_TYPELESS","BC7_UNORM","BC7_UNORM_SRGB",None,None,None,"NV12",None,None,None,None,None,None,None,None,None,None,None,"B4G4R4A4_UNORM",None,None,"D16_UNORM_S8_UINT","R16_UNORM_X8_TYPELESS","X16_TYPELESS_G8_UINT","ETC1","ETC1_SRGB","ETC1A","ETC1A_SRGB",None,None,"R4G4_UNORM"
]

buffer_format_bpp = [
    0,128,128,128,128,96,96,96,96,64,64,64,64,64,64,64,64,64,64,0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,16,16,16,16,16,16,16,16,16,16,16,16,8,8,8,8,8,8,0,32,0,0,4,4,4,8,8,8,8,8,8,4,4,4,8,8,8,16,16,32,0,0,32,32,0,0,8,8,8,8,8,8,0,0,0,12,0,0,0,0,0,0,0,0,0,0,0,16,0,0,16,16,16,4,4,4,4,0,0,8,0
]

buffer_format_components = [
    0,4,4,4,4,3,3,3,3,4,4,4,4,4,4,2,2,2,2,0,2,4,4,4,4,4,3,4,4,4,4,4,4,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,0,3,0,0,4,4,4,4,4,4,4,4,4,1,1,1,2,2,2,3,4,4,0,0,4,4,0,0,3,3,3,4,4,4,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,4,0,0,2,2,2,4,4,4,4,0,0,2,0
]

image_dimension_names = [
    "1D",
    "2D",
    "Cube",
    "3D",
    "Buffer",
    "Unknown",
]

class rage:
    class BufferFormat(Enum):
        pass

    for i, v in enumerate(buffer_format_names):
        if v is None:
            continue
        setattr(BufferFormat, v, i)

    class ImageDimension(Enum):
        Texture1D = 0
        Texture2D = 1
        TextureCUBE = 2
        Texture3D = 3
        Buffer = 4
        Unknown = 5

class DDS_PIXELFORMAT(ctypes.LittleEndianStructure):
    _fields_ = [
        ('size', ctypes.c_uint32),
        ('flags', ctypes.c_uint32),
        ('fourCC', ctypes.c_uint32),
        ('RGBBitCount', ctypes.c_uint32),
        ('RBitMask', ctypes.c_uint32),
        ('GBitMask', ctypes.c_uint32),
        ('BBitMask', ctypes.c_uint32),
        ('ABitMask', ctypes.c_uint32),
    ]

class DDS_HEADER(ctypes.LittleEndianStructure):
    _fields_ = [
        ('size', ctypes.c_uint32),
        ('flags', ctypes.c_uint32),
        ('height', ctypes.c_uint32),
        ('width', ctypes.c_uint32),
        ('pitchOrLinearSize', ctypes.c_uint32),
        ('depth', ctypes.c_uint32),
        ('mipMapCount', ctypes.c_uint32),
        ('reserved1', ctypes.c_uint32 * 11),
        ('ddspf', DDS_PIXELFORMAT),
        ('caps', ctypes.c_uint32),
        ('caps2', ctypes.c_uint32),
        ('caps3', ctypes.c_uint32),
        ('caps4', ctypes.c_uint32),
        ('reserved2', ctypes.c_uint32),
    ]

class DDS_HEADER_DXT10(ctypes.LittleEndianStructure):
    _fields_ = [
        ('dxgiFormat', ctypes.c_uint32),
        ('resourceDimension', ctypes.c_uint32),
        ('miscFlag', ctypes.c_uint32),
        ('arraySize', ctypes.c_uint32),
        ('miscFlags2', ctypes.c_uint32),
    ]

DDSD_CAPS = 0x1
DDSD_HEIGHT = 0x2
DDSD_WIDTH = 0x4
DDSD_PITCH = 0x8
DDSD_PIXELFORMAT = 0x1000
DDSD_MIPMAPCOUNT = 0x20000
DDSD_LINEARSIZE = 0x80000
DDSD_DEPTH = 0x800000

DDSCAPS_COMPLEX = 0x8
DDSCAPS_MIPMAP = 0x400000
DDSCAPS_TEXTURE = 0x1000

DDSCAPS2_CUBEMAP = 0x200
DDSCAPS2_CUBEMAP_POSITIVEX = 0x400
DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800
DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000
DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000
DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000
DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000
DDSCAPS2_VOLUME = 0x200000

DDPF_ALPHAPIXELS     = 0x00000001
DDPF_ALPHA           = 0x00000002
DDPF_FOURCC          = 0x00000004
DDPF_RGB             = 0x00000040
DDPF_PALETTEINDEXED1 = 0x00000800
DDPF_PALETTEINDEXED2 = 0x00001000
DDPF_PALETTEINDEXED4 = 0x00000008
DDPF_PALETTEINDEXED8 = 0x00000020
DDPF_LUMINANCE       = 0x00020000
DDPF_ALPHAPREMULT    = 0x00008000
DDPF_BUMPDUDV        = 0x00080000

FOURCC_DX10 = 0x30315844

FOURCC_DXT1 = 0x31545844
FOURCC_DXT2 = 0x32545844
FOURCC_DXT3 = 0x33545844
FOURCC_DXT4 = 0x34545844
FOURCC_DXT5 = 0x35545844
FOURCC_BC4_UNORM = 0x55344342
FOURCC_BC4_SNORM = 0x53344342
FOURCC_BC5_UNORM = 0x55354342
FOURCC_BC5_SNORM = 0x53354342
FOURCC_R8G8_B8G8 = 0x47424752
FOURCC_G8R8_G8B8 = 0x42475247
FOURCC_YUY2 = 0x32595559

D3DFMT_A32B32G32R32F = 116
D3DFMT_A16B16G16R16F = 113
D3DFMT_A16B16G16R16 = 36
D3DFMT_Q16W16V16U16 = 110
D3DFMT_G32R32F = 115
D3DFMT_G16R16F = 112
D3DFMT_R32F = 114
D3DFMT_R16F = 111

DDS_FOURCC      = DDPF_FOURCC
DDS_RGB         = DDPF_RGB
DDS_RGBA        = DDPF_RGB | DDPF_ALPHAPIXELS
DDS_LUMINANCE   = DDPF_LUMINANCE
DDS_LUMINANCEA  = DDPF_LUMINANCE | DDPF_ALPHAPIXELS
DDS_ALPHAPIXELS = DDPF_ALPHAPIXELS
DDS_ALPHA       = DDPF_ALPHA
DDS_PAL8        = DDPF_PALETTEINDEXED8
DDS_PAL8A       = DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
DDS_BUMPDUDV    = DDPF_BUMPDUDV

DDS_DIMENSION_TEXTURE1D = 2
DDS_DIMENSION_TEXTURE2D = 3
DDS_DIMENSION_TEXTURE3D = 4

DDS_RESOURCE_MISC_TEXTURECUBE = 0x4

DDS_ALPHA_MODE_UNKNOWN = 0x0
DDS_ALPHA_MODE_STRAIGHT = 0x1
DDS_ALPHA_MODE_PREMULTIPLIED = 0x2
DDS_ALPHA_MODE_OPAQUE = 0x3
DDS_ALPHA_MODE_CUSTOM = 0x4

fourcc_buffer_formats = {
    rage.BufferFormat.BC1_UNORM: FOURCC_DXT1,
    rage.BufferFormat.BC2_UNORM: FOURCC_DXT3,
    rage.BufferFormat.BC3_UNORM: FOURCC_DXT5,

    rage.BufferFormat.BC4_UNORM: FOURCC_BC4_UNORM,
    rage.BufferFormat.BC4_SNORM: FOURCC_BC4_SNORM,
    rage.BufferFormat.BC5_UNORM: FOURCC_BC5_UNORM,
    rage.BufferFormat.BC5_SNORM: FOURCC_BC5_SNORM,

    rage.BufferFormat.R32G32B32A32_FLOAT: D3DFMT_A32B32G32R32F,
    rage.BufferFormat.R16G16B16A16_FLOAT: D3DFMT_A16B16G16R16F,
    rage.BufferFormat.R16G16B16A16_UNORM: D3DFMT_A16B16G16R16,
    rage.BufferFormat.R16G16B16A16_SNORM: D3DFMT_Q16W16V16U16,
    rage.BufferFormat.R32G32_FLOAT: D3DFMT_G32R32F,
    rage.BufferFormat.R16G16_FLOAT: D3DFMT_G16R16F,
    rage.BufferFormat.R32_FLOAT: D3DFMT_R32F,
    rage.BufferFormat.R16_FLOAT: D3DFMT_R16F,
}

simple_buffer_formats = {
    rage.BufferFormat.R8G8B8A8_UNORM: DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGBA,       0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000),
    rage.BufferFormat.R16G16_UNORM:   DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGB,        0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000),
    rage.BufferFormat.R8G8_UNORM:     DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 16, 0x000000ff, 0x00000000, 0x00000000, 0x0000ff00),
    rage.BufferFormat.R16_UNORM:      DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE,  0, 16, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000),
    rage.BufferFormat.R8_UNORM:       DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE,  0,  8, 0x000000ff, 0x00000000, 0x00000000, 0x00000000),
    rage.BufferFormat.A8_UNORM:       DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_ALPHA,      0,  8, 0x00000000, 0x00000000, 0x00000000, 0x000000ff),
    rage.BufferFormat.B5G6R5_UNORM:   DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGB,        0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000),
    rage.BufferFormat.B5G5R5A1_UNORM: DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGBA,       0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000),
    rage.BufferFormat.R8G8_SNORM:     DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV,   0, 16, 0x000000ff, 0x0000ff00, 0x00000000, 0x00000000),
    rage.BufferFormat.R8G8B8A8_SNORM: DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV,   0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000),
    rage.BufferFormat.R16G16_SNORM:   DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV,   0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000),
    rage.BufferFormat.B8G8R8A8_UNORM: DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGBA,       0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000),
    rage.BufferFormat.B4G4R4A4_UNORM: DDS_PIXELFORMAT(ctypes.sizeof(DDS_PIXELFORMAT), DDS_RGBA,       0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000),
}

def make_dds_header(dimension, buffer_format, width, height, depth, mipmaps, elements):
    result = b'DDS '

    hdr = DDS_HEADER()
    hdr.size = ctypes.sizeof(hdr)
    hdr.flags |= DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT

    hdr.width = width
    hdr.height = height

    hdr.caps |= DDSCAPS_TEXTURE

    if mipmaps > 1:
        hdr.flags |= DDSD_MIPMAPCOUNT
        hdr.mipMapCount = mipmaps

    if dimension == rage.ImageDimension.TextureCUBE:
        hdr.caps2 |= DDSCAPS2_CUBEMAP \
        | DDSCAPS2_CUBEMAP_POSITIVEX \
        | DDSCAPS2_CUBEMAP_NEGATIVEX \
        | DDSCAPS2_CUBEMAP_POSITIVEY \
        | DDSCAPS2_CUBEMAP_NEGATIVEY \
        | DDSCAPS2_CUBEMAP_POSITIVEZ \
        | DDSCAPS2_CUBEMAP_NEGATIVEZ
    elif dimension == rage.ImageDimension.Texture3D:
        hdr.flags |= DDSD_DEPTH
        hdr.depth = depth
        hdr.caps2 |= DDSCAPS2_VOLUME

    hdr.ddspf.size = ctypes.sizeof(hdr.ddspf)
    hdr.ddspf.flags |= DDPF_FOURCC
    hdr.ddspf.fourCC = FOURCC_DX10

    if (elements == 1) or (elements == 6 and dimension == rage.ImageDimension.TextureCUBE):
        if buffer_format in simple_buffer_formats:
            hdr.ddspf = simple_buffer_formats[buffer_format]
        elif buffer_format in fourcc_buffer_formats:
            hdr.ddspf.fourCC = fourcc_buffer_formats[buffer_format]

    result += ctypes.string_at(ctypes.addressof(hdr), ctypes.sizeof(hdr))

    if hdr.ddspf.fourCC == FOURCC_DX10:
        exhdr = DDS_HEADER_DXT10()
        exhdr.dxgiFormat = buffer_format

        if dimension == rage.ImageDimension.Texture1D:
            exhdr.resourceDimension = DDS_DIMENSION_TEXTURE1D
        elif dimension != rage.ImageDimension.Texture3D:
            exhdr.resourceDimension = DDS_DIMENSION_TEXTURE2D
        else:
            exhdr.resourceDimension = DDS_DIMENSION_TEXTURE3D

        if dimension == rage.ImageDimension.TextureCUBE:
            exhdr.miscFlag |= DDS_RESOURCE_MISC_TEXTURECUBE
            exhdr.arraySize = elements // 6
        else:
            exhdr.arraySize = elements

        result += ctypes.string_at(ctypes.addressof(exhdr), ctypes.sizeof(exhdr))

    return result

def calc_image_size(dimension, buffer_format, width, height, depth, mip_level):
    width  = (width  >> mip_level) or 1
    height = (height >> mip_level) or 1

    if dimension == rage.ImageDimension.Texture3D:
        depth = (depth >> mip_level) or 1
    else:
        depth = 1

    bpp = buffer_format_bpp[buffer_format]

    if (buffer_format >=  70 and buffer_format <=  84) \
    or (buffer_format >=  94 and buffer_format <=  99) \
    or (buffer_format >= 121 and buffer_format <= 124):
        width  = (width  + 0x3) & ~0x3
        height = (height + 0x3) & ~0x3

    return (width * height * depth * bpp) >> 3

def convert_ytd(input_file):
    with open(input_file, 'rb') as f:
        resource_id, reader = read_rsc7(f)

    assert resource_id == 5

    txd_dict = reader.read_pgdict(0x50000000)

    output_dir = os.path.splitext(input_file)[0] + '/'

    for h, v in txd_dict:
        _vtbl, _dword8, _wordC, _wordE, flags,\
        width, height, depth, dimension, buffer_format,\
        tile_mode, aa_mode, mipmaps, _word24, _word26,\
        p_name, p_void30, p_image_data = reader.unpack(v, '<QIHHQHHHBBBBHHHQQQ')

        name = reader.read_cstr(p_name)

        print('0x{:08X} {:<50} | {:6} {:20} | {:4} x {:4} | {:4} {:2} @ 0x{:08X}'.format(
            h, name, image_dimension_names[dimension], buffer_format_names[buffer_format], width, height, depth, mipmaps,
            reader.get_offset(p_image_data, 1)
        ))

        output_dds = output_dir + name + '.dds'

        try:
            os.mkdir(os.path.dirname(output_dds))
        except:
            pass

        with open(output_dds, 'wb') as f:
            f.write(
                make_dds_header(dimension, buffer_format, width, height,
                    depth if dimension == rage.ImageDimension.Texture3D else 1, mipmaps,
                    depth if dimension != rage.ImageDimension.Texture3D else 1
                )
            )

            for _ in range(depth if dimension != rage.ImageDimension.Texture3D else 1):
                for mip_level in range(mipmaps):
                    size = calc_image_size(dimension, buffer_format, width, height, depth, mip_level)
                    f.write(reader.read(p_image_data, size))
                    p_image_data += size

if len(sys.argv) < 2:
    print('Usage: python3 ytd_to_dds.py <ytd files>')
    sys.exit(1)

for input_file in sys.argv[1:]:
    convert_ytd(input_file)
