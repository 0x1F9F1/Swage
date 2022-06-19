#include "resource.h"

#include "core/platform/minwin.h"

namespace Swage
{
    ResourceStream::ResourceStream(void* module, int id, const char* type)
        : ResourceStream(module, MAKEINTRESOURCEA(id), type)
    {}

    ResourceStream::ResourceStream(void* module, const char* name, const char* type)
    {
        HMODULE hModule = static_cast<HMODULE>(module);
        HRSRC hResInfo = FindResourceA(hModule, name, type);
        res_data_ = LoadResource(hModule, hResInfo);

        size_ = SizeofResource(hModule, hResInfo);
        data_ = static_cast<u8*>(LockResource(res_data_));
    }

    ResourceStream::~ResourceStream()
    {
        FreeResource(res_data_);
    }
} // namespace Swage
