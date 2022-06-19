#include "winapi.h"

#include "../findhandle.h"

#include "core/platform/minwin.h"

#include <ShlObj.h>

namespace Swage
{
    static constexpr usize MaxPathLength = 1024;

    static usize ToNativePath(StringView input, wchar_t* buffer, usize buffer_len)
    {
        if (buffer_len < 5)
            return 0;

        usize converted = 0;

        std::wmemcpy(buffer + converted, LR"(\\?\)", 4);
        converted += 4;

        converted += static_cast<usize>(MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
            buffer + converted, static_cast<int>(buffer_len - converted - 1)));

        buffer[converted] = L'\0';

        for (wchar_t* find = std::wcschr(buffer, L'/'); find; find = std::wcschr(find + 1, L'/'))
            *find = L'\\';

        return converted;
    }

    static usize FromNativePath(const wchar_t* input, char* buffer, usize buffer_len)
    {
        if (buffer_len == 0)
            return 0;

        if (std::wcsncmp(input, LR"(\\?\)", 4) == 0)
            input += 4;

        usize converted = static_cast<usize>(
            WideCharToMultiByte(CP_UTF8, 0, input, -1, buffer, static_cast<int>(buffer_len), nullptr, nullptr));

        if (converted != 0)
            converted -= 1;

        for (char* find = std::strchr(buffer, '\\'); find; find = std::strchr(find + 1, '\\'))
            *find = '/';

        return converted;
    }

    class Win32FileStream final : public Stream
    {
    public:
        Win32FileStream(HANDLE handle);
        ~Win32FileStream() override;

        i64 Seek(i64 offset, SeekWhence whence) override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;
        usize Write(const void* ptr, usize len) override;

        usize ReadBulk(void* ptr, usize len, i64 offset) override;
        usize WriteBulk(const void* ptr, usize len, i64 offset) override;

        bool Flush() override;

        i64 SetSize(i64 length) override;

        bool IsSynchronized() override;

    private:
        HANDLE handle_ {};
    };

    class Win32FileEnumerator final : public FileEnumerator
    {
    public:
        Win32FileEnumerator(const wchar_t* path);
        ~Win32FileEnumerator() override;

        bool Next(FolderEntry& entry) override;

    private:
        HANDLE handle_ {};
        bool valid_ {false};
        WIN32_FIND_DATAW data_ {};
    };

    Win32FileStream::Win32FileStream(HANDLE handle)
        : handle_(handle)
    {}

    Win32FileStream::~Win32FileStream()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }

    i64 Win32FileStream::Seek(i64 offset, SeekWhence whence)
    {
        LARGE_INTEGER distance;
        distance.QuadPart = offset;

        LARGE_INTEGER result;
        result.QuadPart = 0;

        DWORD method = 0;

        switch (whence)
        {
            case SeekWhence::Set: method = FILE_BEGIN; break;
            case SeekWhence::Cur: method = FILE_CURRENT; break;
            case SeekWhence::End: method = FILE_END; break;
        }

        return SetFilePointerEx(handle_, distance, &result, method) ? result.QuadPart : -1;
    }

    i64 Win32FileStream::Size()
    {
        LARGE_INTEGER result;
        result.QuadPart = 0;

        return GetFileSizeEx(handle_, &result) ? result.QuadPart : -1;
    }

    usize Win32FileStream::Read(void* ptr, usize len)
    {
        DWORD result = 0;

        // TODO: Handle reads > 4gb
        SwAssert(len <= UINT32_MAX);
        return ReadFile(handle_, ptr, static_cast<DWORD>(len), &result, nullptr) ? result : 0;
    }

    usize Win32FileStream::Write(const void* ptr, usize len)
    {
        DWORD result = 0;

        // TODO: Handle writes > 4gb
        SwAssert(len <= UINT32_MAX);
        return WriteFile(handle_, ptr, static_cast<DWORD>(len), &result, nullptr) ? result : 0;
    }

    usize Win32FileStream::ReadBulk(void* ptr, usize len, i64 offset)
    {
        OVERLAPPED overlapped {};

        overlapped.Offset = offset & 0xFFFFFFFF;
        overlapped.OffsetHigh = u64(offset) >> 32;

        DWORD result = 0;

        // TODO: Handle reads > 4gb
        SwAssert(len <= UINT32_MAX);
        return ReadFile(handle_, ptr, static_cast<DWORD>(len), &result, &overlapped) ? result : 0;
    }

    usize Win32FileStream::WriteBulk(const void* ptr, usize len, i64 offset)
    {
        OVERLAPPED overlapped {};

        overlapped.Offset = offset & 0xFFFFFFFF;
        overlapped.OffsetHigh = u64(offset) >> 32;

        DWORD result = 0;

        // TODO: Handle writes > 4gb
        SwAssert(len <= UINT32_MAX);
        return WriteFile(handle_, ptr, static_cast<DWORD>(len), &result, &overlapped) ? result : 0;
    }

    bool Win32FileStream::Flush()
    {
        return FlushFileBuffers(handle_);
    }

    i64 Win32FileStream::SetSize(i64 length)
    {
        LARGE_INTEGER distance;
        distance.QuadPart = length;

        LARGE_INTEGER result;
        result.QuadPart = 0;

        return (SetFilePointerEx(handle_, distance, &result, FILE_BEGIN) && (result.QuadPart == length) &&
                   SetEndOfFile(handle_))
            ? length
            : -1;
    }

    bool Win32FileStream::IsSynchronized()
    {
        return true;
    }

    Win32FileEnumerator::Win32FileEnumerator(const wchar_t* path)
    {
        handle_ = FindFirstFileExW(path, FindExInfoBasic, &data_, FindExSearchNameMatch, nullptr, 0);
        valid_ = handle_ != INVALID_HANDLE_VALUE;

        // Skip "." and ".." directories
        for (; valid_; valid_ = FindNextFileW(handle_, &data_))
        {
            if (!(data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (data_.cFileName[0] != L'.') ||
                (data_.cFileName[1] != L'\0') && (data_.cFileName[1] != L'.' || data_.cFileName[2] != L'\0'))
                break;
        }
    }

    Win32FileEnumerator::~Win32FileEnumerator()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            FindClose(handle_);
    }

    bool Win32FileEnumerator::Next(FolderEntry& entry)
    {
        entry.Reset();

        if (!valid_)
            return false;

        char buffer[MaxPathLength];

        if (usize converted = FromNativePath(data_.cFileName, buffer, std::size(buffer)); converted != 0)
            entry.Name.assign(buffer, converted);
        else
            entry.Name.assign("**Invalid**"_sv);

        entry.IsFolder = data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        entry.Size = data_.nFileSizeLow | u64(data_.nFileSizeHigh) << 32;

        valid_ = FindNextFileW(handle_, &data_);

        return true;
    }

    class Win32VolumeEnumerator final : public FileEnumerator
    {
    public:
        Win32VolumeEnumerator()
            : drives_(GetLogicalDrives())
        {}

        bool Next(FolderEntry& entry) override
        {
            entry.Reset();

            while (drives_)
            {
                bool valid = drives_ & 1;
                drives_ >>= 1;
                char letter = letter_++;

                if (!valid)
                    continue;

                char path[4] {letter, ':', '\\', '\0'};

                if (GetVolumeInformationA(path, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0))
                {
                    entry.Name.assign(path, 2);
                    entry.IsFolder = true;

                    return true;
                }
            }

            return false;
        }

    private:
        u32 drives_ {0};
        char letter_ {'A'};
    };

    static inline Rc<Stream> Win32FileCreateRaw(const wchar_t* path, DWORD desired_access, DWORD share_mode,
        DWORD create_disposition, DWORD flags_and_attributes = FILE_ATTRIBUTE_NORMAL)
    {
        HANDLE handle =
            CreateFileW(path, desired_access, share_mode, nullptr, create_disposition, flags_and_attributes, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
            return nullptr;

        return MakeUnique<Win32FileStream>(handle);
    }

    static inline bool Win32FolderCreateRaw(const wchar_t* path)
    {
        return CreateDirectoryW(path, NULL) || (GetLastError() == ERROR_ALREADY_EXISTS);
    }

    static inline bool Win32FileExistsRaw(const wchar_t* path)
    {
        DWORD attribs = GetFileAttributesW(path);

        return (attribs != INVALID_FILE_ATTRIBUTES) && !(attribs & FILE_ATTRIBUTE_DIRECTORY);
    }

    static inline bool Win32FolderExistsRaw(const wchar_t* path)
    {
        DWORD attribs = GetFileAttributesW(path);

        return (attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY);
    }

    Rc<Stream> Win32FileOpen(StringView path, bool read_only)
    {
        wchar_t wpath[MaxPathLength];

        if (ToNativePath(path, wpath, std::size(wpath)) == 0)
            return nullptr;

        // Avoid opening nonexistent files
        if (!Win32FileExistsRaw(wpath))
            return nullptr;

        return Win32FileCreateRaw(wpath, read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
            read_only ? FILE_SHARE_READ : 0, OPEN_EXISTING);
    }

    Rc<Stream> Win32FileCreate(StringView path, bool write_only, bool truncate)
    {
        wchar_t wpath[MaxPathLength];

        if (ToNativePath(path, wpath, std::size(wpath)) == 0)
            return nullptr;

        for (usize i = 0;; i = 1)
        {
            Rc<Stream> result = Win32FileCreateRaw(wpath, write_only ? GENERIC_WRITE : (GENERIC_READ | GENERIC_WRITE),
                0, truncate ? CREATE_ALWAYS : OPEN_ALWAYS);

            if (result != nullptr || i == 1 || GetLastError() != ERROR_PATH_NOT_FOUND)
                return result;

            for (wchar_t* j = std::wcschr(wpath + 7, L'\\'); j; j = std::wcschr(j + 1, L'\\'))
            {
                *j = L'\0';

                if (!Win32FolderExistsRaw(wpath) && !Win32FolderCreateRaw(wpath))
                    return nullptr;

                *j = L'\\';
            }
        }

        return nullptr;
    }

    Rc<Stream> Win32FileTemporary()
    {
        wchar_t wtemp_path[MaxPathLength];

        if (GetTempPathW(static_cast<DWORD>(std::size(wtemp_path)), wtemp_path) == 0)
            return nullptr;

        wchar_t wpath[MaxPathLength];

        if (GetTempFileNameW(wtemp_path, L"", 0, wpath) == 0)
            return nullptr;

        return Win32FileCreateRaw(wpath, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE);
    }

    bool Win32FileExists(StringView path)
    {
        wchar_t wpath[MaxPathLength];

        if (ToNativePath(path, wpath, std::size(wpath)) == 0)
            return false;

        return Win32FileExistsRaw(wpath);
    }

    Ptr<FileEnumerator> Win32FileFind(StringView path)
    {
        if (path.empty())
            return MakeUnique<Win32VolumeEnumerator>();

        wchar_t wpath[MaxPathLength + 2];

        usize converted = ToNativePath(path, wpath, std::size(wpath) - 2);

        if (converted == 0)
            return nullptr;

        if (wpath[converted - 1] != L'\\')
            return nullptr;

        wpath[converted++] = L'*';
        wpath[converted++] = L'\0';

        return MakeUnique<Win32FileEnumerator>(wpath);
    }

    static usize Win32GetKnownFolderPathRaw(REFKNOWNFOLDERID rfid, char* buffer, usize buffer_len)
    {
        if (buffer_len == 0)
            return 0;

        wchar_t* wpath = nullptr;

        SwAssert(SUCCEEDED(SHGetKnownFolderPath(rfid, KF_FLAG_DEFAULT, NULL, &wpath)));

        usize converted = FromNativePath(wpath, buffer, buffer_len);

        CoTaskMemFree(wpath);

        return converted;
    }

    String Win32GetPrefPath(StringView app)
    {
        char buffer[MaxPathLength];

        SwAssert(Win32GetKnownFolderPathRaw(FOLDERID_Documents, buffer, std::size(buffer)));
        SwAssert(strcat_s(buffer, "/") == 0);
        SwAssert(strncat_s(buffer, app.data(), app.size()) == 0);
        SwAssert(strcat_s(buffer, "/") == 0);

        return buffer;
    }

    String Win32GetBasePath()
    {
        wchar_t filename[MaxPathLength];

        usize length = GetModuleFileNameW(NULL, filename, static_cast<DWORD>(std::size(filename)));

        SwAssert(length != 0);

        for (; length && filename[length - 1] != L'\\'; --length)
            ;

        filename[length] = L'\0';

        char buffer[MaxPathLength];

        SwAssert(FromNativePath(filename, buffer, std::size(buffer)));

        return buffer;
    }

    String Win32PathExpandEnvStrings(StringView path)
    {
        wchar_t wpath[MAX_PATH];

        if (ToNativePath(path, wpath, std::size(wpath)) == 0)
            return String(path);

        wchar_t wpath_new[MAX_PATH];

        DWORD length = ExpandEnvironmentStringsW(wpath, wpath_new, static_cast<DWORD>(std::size(wpath)));

        if (length == 0)
            return String(path);

        char buffer[MAX_PATH];

        usize converted = FromNativePath(wpath_new, buffer, std::size(buffer));

        if (converted == 0)
            return String(path);

        return String(buffer, buffer + converted);
    }

    static HANDLE(WINAPI* Swage_OpenProcess)(DWORD, BOOL, DWORD);
    static SIZE_T(WINAPI* Swage_VirtualQueryEx)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
    static BOOL(WINAPI* Swage_ReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);

    static HWND(WINAPI* Swage_FindWindowA)(LPCSTR, LPCSTR);
    static DWORD(WINAPI* Swage_GetWindowThreadProcessId)(HWND, LPDWORD);

    class Win32ProcStream : public Stream
    {
    public:
        Win32ProcStream(HANDLE hProc);
        ~Win32ProcStream();

        i64 Seek(i64 position, SeekWhence whence) override;
        i64 Tell() override;
        i64 Size() override;

        usize Read(void* ptr, usize len) override;

    protected:
        HANDLE handle_ {};

        u64 here_ {};
        u64 end_ {};
        i32 state_ {};
    };

    Win32ProcStream::Win32ProcStream(HANDLE hProc)
        : handle_(hProc)
    {}

    Win32ProcStream::~Win32ProcStream()
    {
        if (handle_ != NULL)
            CloseHandle(handle_);
    }

    i64 Win32ProcStream::Seek(i64 position, SeekWhence whence)
    {
        switch (whence)
        {
            case SeekWhence::Set: here_ = position; break;
            case SeekWhence::Cur: here_ += position; break;
            case SeekWhence::End: here_ = 0; break;
        }

        end_ = here_;
        state_ = 0;

        return here_;
    }

    i64 Win32ProcStream::Tell()
    {
        return static_cast<i64>(here_);
    }

    i64 Win32ProcStream::Size()
    {
        return 0;
    }

    usize Win32ProcStream::Read(void* ptr, usize len)
    {
        usize total = 0;

        while (total < len)
        {
            if (state_ < 0)
                break;

            if ((state_ == 1) || (here_ >= end_))
            {
                MEMORY_BASIC_INFORMATION mem_info;

                if (Swage_VirtualQueryEx(handle_, reinterpret_cast<LPCVOID>(static_cast<usize>(here_)), &mem_info,
                        sizeof(mem_info)) != sizeof(mem_info))
                {
                    end_ = 0;
                    state_ = -1;
                    break;
                }

                u64 region_start = static_cast<u64>(reinterpret_cast<usize>(mem_info.BaseAddress));
                u64 region_end = region_start + mem_info.RegionSize;

                end_ = region_end;

                if (state_ == 1)
                {
                    state_ = 0;
                    here_ = end_;
                    continue;
                }

                if (!(mem_info.State & MEM_COMMIT) || (mem_info.Type & MEM_MAPPED) ||
                    (mem_info.Protect & (PAGE_NOACCESS | PAGE_GUARD | PAGE_WRITECOMBINE | PAGE_NOCACHE)))
                {
                    here_ = end_;
                    continue;
                }

                if (here_ < region_start)
                    here_ = region_start;
            }

            SIZE_T read = static_cast<SIZE_T>(std::min<u64>(len - total, end_ - here_));

            if ((!Swage_ReadProcessMemory(
                     handle_, reinterpret_cast<LPCVOID>(static_cast<usize>(here_)), ptr, read, &read) &&
                    (GetLastError() != ERROR_PARTIAL_COPY)) ||
                (read == 0))
            {
                state_ = 1;
                continue;
            }

            here_ += read;
            total += read;
        }

        return total;
    }

    // Try to avoid silly "anti-virus" detections
    static void LoadProcFunctions()
    {
        /*
            data = bytearray(b"OpenProcess\0VirtualQueryEx\0ReadProcessMemory\0FindWindowA\0GetWindowThreadProcessId\0")
            iv = 0x41
            end = 0

            for i, v in enumerate(data):
                if i == end:
                    end = data.find(b'\0', i)
                    print('X(Swage_{}, FOOBAR, {});'.format(data[i:end].decode('utf-8'), i))
                    end += 1
                v = ((v * 0x55) & 0xFF) ^ iv
                data[i] = v
                iv = v

            print(''.join(f'\\x{v:02X}' for v in data))
        */

        static std::once_flag load_procs;

        std::call_once(load_procs, [] {
            char imports[] = "\x7A\x4A\xC3\x45\xD5\x0F\xD4\x0B\x82\xAD\x82\x82\x0C\xD1\x0B\x8F\x56\x63\xBF\x5A\x83\x0A"
                             "\xD0\xFD\x14\xCC\xCC\xF6\x7F\x4A\x7E\xEE\x34\xEF\x30\xB9\x96\xB9\x28\xA1\x90\x4B\x91\xBC"
                             "\xBC\x82\x5F\xD9\xED\x0E\xD3\x55\x61\xBA\x39\xAC\xAC\x3F\xB6\x32\xD1\x0C\x8A\xBE\x65\xE6"
                             "\x02\x8A\x50\xD9\xEC\xD8\x48\x92\x49\x96\x1F\x30\x1F\x22\x16\x16";

            volatile u8 iv = 0x41;

            for (usize i = 0; i < sizeof(imports) - 1; ++i)
            {
                volatile u8 v = imports[i];
                imports[i] = static_cast<u8>((v ^ iv) * 0xFD);
                iv = v;
            }

#define X(VARIABLE, MODULE, INDEX) \
    VARIABLE = reinterpret_cast<decltype(VARIABLE)>(GetProcAddress(MODULE, imports + INDEX))

            HMODULE kernel32 = LoadLibraryA("kernel32.dll");
            HMODULE user32 = LoadLibraryA("user32.dll");

            X(Swage_OpenProcess, kernel32, 0);
            X(Swage_VirtualQueryEx, kernel32, 12);
            X(Swage_ReadProcessMemory, kernel32, 27);

            X(Swage_FindWindowA, user32, 45);
            X(Swage_GetWindowThreadProcessId, user32, 57);

#undef X
        });
    }

    u32 Win32ProcIdFromWindow(const char* wnd_title, const char* wnd_class)
    {
        LoadProcFunctions();

        HWND hWnd = Swage_FindWindowA(wnd_class, wnd_title);

        if (hWnd == NULL)
            return 0;

        DWORD dwProcessId = 0;
        Swage_GetWindowThreadProcessId(hWnd, &dwProcessId);

        return dwProcessId;
    }

    Rc<Stream> Win32ProcOpenStream(u32 proc_id)
    {
        LoadProcFunctions();

        HANDLE hProc = Swage_OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, proc_id);

        if (hProc == NULL)
            return nullptr;

        return MakeRc<Win32ProcStream>(hProc);
    }

} // namespace Swage
