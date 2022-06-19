#pragma once

#include "../stream/memory.h"

namespace Swage
{
    class ResourceStream final : public MemoryStream
    {
    public:
        ResourceStream(void* module, int id, const char* type);
        ResourceStream(void* module, const char* name, const char* type);
        ~ResourceStream() override;

    private:
        void* res_data_ {};
    };
} // namespace Swage
