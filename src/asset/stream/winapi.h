#pragma once

#include "../stream.h"

namespace Swage
{
    Rc<Stream> Win32FileOpen(StringView path, bool read_only);
    Rc<Stream> Win32FileCreate(StringView path, bool write_only, bool truncate);
    Rc<Stream> Win32FileTemporary();

    bool Win32FileExists(StringView path);

    Ptr<FileEnumerator> Win32FileFind(StringView path);

    String Win32GetPrefPath(StringView app);
    String Win32GetBasePath();

    String Win32PathExpandEnvStrings(StringView path);

    u32 Win32ProcIdFromWindow(const char* wnd_title, const char* wnd_class);
    Rc<Stream> Win32ProcOpenStream(u32 proc_id);
} // namespace Swage
