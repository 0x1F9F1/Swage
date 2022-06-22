#pragma once

#include "asset/filedevice.h"
#include "asset/vfs.h"

namespace Swage
{
    class VirtualFileDevice : public FileDevice
    {
    public:
        VirtualFileDevice() = default;

        Rc<Stream> Open(StringView path, bool read_only) override;
        bool Exists(StringView path) override;

        Ptr<FileEnumerator> Enumerate(StringView path) override;
        bool Extension(StringView path, const ExtensionData& data) override;

        VFS Files {};
    };

    template <typename T>
    class FileOperationsT : public VFS::FileOperations
    {
    public:
        template <typename... Args>
        T* AddFile(VFS& vfs, StringView path, Args&&... args);
        T& GetData(File& file);

    private:
        class FileData;

        void Delete(File* file) final override;
    };

    template <typename T>
    class FileOperationsT<T>::FileData final : public VFS::File
    {
    public:
        template <typename... Args>
        FileData(VFS::FileOperations* ops, Args&&... args)
            : File(ops)
            , Data {std::forward<Args>(args)...}
        {}

        T Data;
    };

    template <typename T>
    template <typename... Args>
    inline T* FileOperationsT<T>::AddFile(VFS& vfs, StringView path, Args&&... args)
    {
        if (File** file = vfs.AddFile(path))
        {
            FileData* file_data = new FileData(this, std::forward<Args>(args)...);
            *file = file_data;
            return &file_data->Data;
        }

        return nullptr;
    }

    template <typename T>
    inline T& FileOperationsT<T>::GetData(File& file)
    {
        return static_cast<FileData&>(file).Data;
    }

    template <typename T>
    inline void FileOperationsT<T>::Delete(File* file)
    {
        delete static_cast<FileData*>(file);
    }
} // namespace Swage