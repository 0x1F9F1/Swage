#pragma once

namespace Swage
{
    using ProcAddr = void (*)();

    void* LoadProcs(const char* lib_name, const char* const* proc_names, ProcAddr* procs);
} // namespace Swage