#pragma once

namespace Swage
{
    class FileDevice;
    class Stream;
    class FileEnumerator;

    class BufferedStream;

    struct FolderEntry;

    enum class SeekWhence : u8
    {
        Set,
        Cur,
        End
    };
} // namespace Swage
