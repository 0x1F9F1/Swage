#pragma once

#include "findhandle.h"

#include <queue>

namespace Swage
{
    class FileGlobber : public FileEnumerator
    {
    public:
        FileGlobber(Rc<FileDevice> device, String path, String pattern);

        bool Next(FolderEntry& entry) override;

    private:
        Rc<FileDevice> device_;
        Ptr<FileEnumerator> current_;
        std::queue<String> pending_;
        String base_;
        String here_;

        String pattern_;
        usize start_;
    };
} // namespace Swage
