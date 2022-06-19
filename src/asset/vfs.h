#pragma once

#include "findhandle.h"

namespace Swage
{
    struct FileDeviceExtension;

    class VFS
    {
    public:
        class File;
        class FileOperations;

        VFS();
        ~VFS();

        VFS(const VFS&) = delete;
        VFS& operator=(const VFS&) = delete;

        void Reserve(usize capacity);
        void Clear();

        bool Exists(StringView path) const;
        File* GetFile(StringView name) const;

        // Add a file with the given name, if it does not already exist
        // The resulting File* must be initialized by the caller
        File** AddFile(StringView name);

        Ptr<FileEnumerator> Enumerate(StringView path);

    private:
        struct Node;
        struct Folder;

        class Enumerator;

        // Find a node with the given name and hash
        Node* FindNode(StringView name, u32 hash) const;

        // Find a node with the given name
        Node* FindNode(StringView name) const;

        // Allocate and construct a new node
        Node* AllocNode(u32 hash, StringView name);

        // Delete a node
        void FreeNode(Node* node);

        // Find a folder node with the given name
        Node* GetFolder(StringView name) const;

        // Add a folder node with the given name
        Node* AddFolder(StringView name);

        // Find or create a new node
        Pair<Node*, bool> AddNode(StringView name, u32 hash);

        // Get the next valid capacity
        usize GetNextCapacity(usize capacity);

        // Link a node to the hash table
        void LinkHash(Node* node);

        usize node_count_ {};
        usize bucket_count_ {};
        Ptr<Node* []> buckets_ {};
    };

    // TODO: Should this be AtomicRefCounted?
    class VFS::FileOperations : public RefCounted
    {
    public:
        using File = VFS::File;

        virtual void Delete(File* file) = 0;

        virtual Rc<Stream> Open(File& file) = 0;
        virtual void Stat(File& file, FolderEntry& entry) = 0;

        virtual bool Extension(File& file, FileDeviceExtension& data);
    };

    class VFS::File
    {
    public:
        File(FileOperations* ops);

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        FileOperations* Ops;
    };

    // Returns the (dirname, basename) pair of the path
    Pair<StringView, StringView> SplitPath(StringView path);

    inline VFS::File::File(FileOperations* ops)
        : Ops(ops)
    {
        Ops->AddRef();
    }
} // namespace Swage