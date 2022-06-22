#pragma once

#include "fileapi.h"

namespace Swage
{
    struct ExtensionData;

    class FileDevice : public AtomicRefCounted
    {
    public:
        // Opens a file
        // Returns a handle to the opened file, or null on error
        virtual Rc<Stream> Open(StringView path, bool read_only);

        // Creates a file
        // Returns a handle to the new file, or null on error
        virtual Rc<Stream> Create(StringView path, bool write_only, bool truncate);

        // Checks whether a file exists
        // Returns whether or not the specified file exists
        virtual bool Exists(StringView path);

        // Enumerates files in the specified directory
        // Returns a handle for file enumeration, or null on error
        virtual Ptr<FileEnumerator> Enumerate(StringView path);

        // DeleteFile
        // CreateFolder
        // DeleteFolder
        // Stat
        // Rename
        // Move
        // GetPackProvider

        virtual bool Extension(StringView path, const ExtensionData& data);
    };
} // namespace Swage
