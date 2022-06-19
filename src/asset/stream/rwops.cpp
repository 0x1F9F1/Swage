#include "rwops.h"

#include <SDL_rwops.h>

namespace Swage
{
    RWopsStream::RWopsStream(SDL_RWops* ops)
        : ops_(ops)
    {}

    RWopsStream::~RWopsStream()
    {
        if (ops_ != nullptr)
            SDL_RWclose(ops_);
    }

    Rc<RWopsStream> RWopsStream::Open(const char* path, bool read_only)
    {
        SDL_RWops* ops = SDL_RWFromFile(path, read_only ? "rb" : "r+b");

        if (ops == nullptr)
            return nullptr;

        return MakeRc<RWopsStream>(ops);
    }

    Rc<RWopsStream> RWopsStream::Create(const char* path, bool write_only, bool truncate)
    {
        SDL_RWops* ops = SDL_RWFromFile(path, truncate ? (write_only ? "wb" : "w+b") : "r+b");

        if (ops == nullptr && !truncate)
            ops = SDL_RWFromFile(path, write_only ? "wb" : "w+b");

        if (ops == nullptr)
            return nullptr;

        return MakeRc<RWopsStream>(ops);
    }

    i64 RWopsStream::Seek(i64 offset, SeekWhence whence)
    {
        int iwhence = -1;

        switch (whence)
        {
            case SeekWhence::Set: iwhence = RW_SEEK_SET; break;
            case SeekWhence::Cur: iwhence = RW_SEEK_CUR; break;
            case SeekWhence::End: iwhence = RW_SEEK_END; break;
        }

        return SDL_RWseek(ops_, offset, iwhence);
    }

    i64 RWopsStream::Tell()
    {
        return SDL_RWtell(ops_);
    }

    i64 RWopsStream::Size()
    {
        return SDL_RWsize(ops_);
    }

    usize RWopsStream::Read(void* ptr, usize len)
    {
        return SDL_RWread(ops_, ptr, 1, len);
    }

    usize RWopsStream::Write(const void* ptr, usize len)
    {
        return SDL_RWwrite(ops_, ptr, 1, len);
    }
} // namespace Swage
