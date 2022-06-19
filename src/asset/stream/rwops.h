#pragma once

#include "../stream.h"

struct SDL_RWops;

namespace Swage
{
    class RWopsStream final : public Stream
    {
    public:
        RWopsStream(SDL_RWops* ops);
        ~RWopsStream() override;

        static Rc<RWopsStream> Open(const char* path, bool read_only);
        static Rc<RWopsStream> Create(const char* path, bool write_only, bool truncate);

        i64 Seek(i64 offset, SeekWhence whence) override;

        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize Write(const void* ptr, usize len) override;

    private:
        SDL_RWops* ops_ {};
    };
} // namespace Swage
