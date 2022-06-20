#include "vfs.h"

enum class vla_size_t : std::size_t
{
};

#define vla_size(TYPE, MEMBER, LENGTH) \
    static_cast<vla_size_t>(offsetof(TYPE, MEMBER) + (sizeof(((TYPE*) 0)->MEMBER[0]) * (LENGTH)))

void* operator new([[maybe_unused]] std::size_t size, vla_size_t new_size)
{
    return ::operator new(static_cast<std::size_t>(new_size));
}

namespace Swage
{
    static const u32 NODE_HASH_FOLDER_BIT = 0x80000000;

    static inline u32 NodeHashPathPartial(StringView name, u32 hash)
    {
        for (const char v : name)
        {
            hash += static_cast<unsigned char>(v);
            hash += hash << 10;
            hash ^= hash >> 6;
        }

        return hash;
    }

    static inline u32 NodeHashPath(StringView path, u32 seed = 0)
    {
        u32 hash = NodeHashPathPartial(path, seed);

        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;

        hash &= ~NODE_HASH_FOLDER_BIT;

        if (path.empty() || path.back() == '/')
            hash |= NODE_HASH_FOLDER_BIT;

        return hash;
    }

    static inline bool NodeHashIsFolder(u32 hash)
    {
        return (hash & NODE_HASH_FOLDER_BIT) != 0;
    }

    static inline bool NodeHashIsFile(u32 hash)
    {
        return (hash & NODE_HASH_FOLDER_BIT) == 0;
    }

    static inline bool NodeComparePaths(const char* lhs, const char* rhs, usize len)
    {
        for (usize i = 0; i != len; ++i)
        {
            if (lhs[i] != rhs[i])
                return false;
        }

        return true;
    }

    // Like SplitPath, but allows a trailing slash in the basename
    static inline Pair<StringView, StringView> ParentPath(StringView path)
    {
        // Ignore the last character. Either it is a slash (which we want to skip), or it isn't (which wouldn't match)
        usize split = path.substr(0, path.size() - 1).rfind('/') + 1;

        return {path.substr(0, split), path.substr(split)};
    }

    struct VFS::Node
    {
        // Hash of full path
        // The highest bit indicates if the node is a folder
        u32 Hash {};

        // Length of Name
        u16 NameLength {};

        static constexpr usize MaxNameLength = UINT16_MAX;

        // Next node in current hash bucket
        Node* NextHash {};

        // Parent folder
        Node* Parent {};

        // Next/Previous nodes in current folder
        Node* NextSibling {};
        Node* PrevSibling {};

        union
        {
            // Data if the node is a file
            File* FileData;

            // Data if the node is a folder
            Folder* FolderData;
        };

        // Base name of this node
        // Must be at the end of the class
        char Name[1];

        Node(u32 hash, StringView name) noexcept;
        ~Node();

        Node(const Node&) = delete;
        Node operator=(const Node&) = delete;

        // Is this node a folder
        inline bool IsFolder() const
        {
            return NodeHashIsFolder(Hash);
        }

        // Get the basename of this node
        inline StringView GetName() const
        {
            return StringView(Name, NameLength);
        }

        // Compare the full path of this node
        bool ComparePath(StringView path) const;

        // Add a node to the end of the child list
        void AddChild(Node* node);
    };

    // TODO: RefCount?
    struct VFS::Folder final
    {
        // TODO: Add stats? (File Count, Folder Count)
        Node* FirstChild {};
        Node* LastChild {};
    };

    inline VFS::Node::Node(u32 hash, StringView name) noexcept
        : Hash(hash)
        , NameLength(static_cast<u16>(name.size()))
    {
        std::memcpy(Name, name.data(), name.size());
        Name[name.size()] = '\0';

        if (IsFolder())
            FolderData = nullptr;
        else
            FileData = nullptr;
    }

    inline VFS::Node::~Node()
    {
        if (IsFolder())
        {
            if (Folder* folder = FolderData)
            {
                delete folder;
            }
        }
        else
        {
            if (File* file = FileData)
            {
                Rc(file->Ops)->Delete(file);
            }
        }
    }

    bool VFS::Node::ComparePath(StringView path) const
    {
        const Node* node = this;

        // Compare the segments of each node
        // Avoid one loop iteration by stopping at the root node (as the name is always empty)
        while (const Node* parent = node->Parent)
        {
            // Get the name of the current node
            StringView name = node->GetName();

            // Make sure the name isn't too long
            if (name.size() > path.size())
                return false;

            // Check if the name matches
            if (!NodeComparePaths(path.data() + path.size() - name.size(), name.data(), name.size()))
                return false;

            // Remove the segment of the name which was just compared.
            path.remove_suffix(name.size());

            // Move to the next (parent) node
            node = parent;
        }

        // We've compared the whole node path, so make sure we aren't expecting any more characters.
        return path.empty();
    }

    inline void VFS::Node::AddChild(Node* node)
    {
        SwAssert(IsFolder());

        // Ensure the node is not already linked somewhere
        SwAssert(node->Parent == nullptr);
        node->Parent = this;

        Folder* folder = FolderData;

        Node* tail = folder->LastChild;
        node->NextSibling = nullptr;
        node->PrevSibling = tail;

        if (tail)
            tail->NextSibling = node;
        else
            folder->FirstChild = node;

        folder->LastChild = node;
    }

    VFS::VFS() = default;

    VFS::~VFS()
    {
        Clear();
    }

    void VFS::Reserve(usize capacity)
    {
        // x + (x >> 2) == x * 1.25. load factor = 1 / 1.25 = 0.8
        capacity += capacity >> 2;

        // Don't resize if the bucket count is less.
        if (capacity <= bucket_count_)
            return;

        // Calculate the next power of 2 capacity
        capacity = GetNextCapacity(capacity);
        Ptr<Node*[]> buckets = MakeUnique<Node*[]>(capacity);

        std::swap(bucket_count_, capacity);
        std::swap(buckets_, buckets);

        usize total = 0;

        for (usize i = 0; i < capacity; ++i)
        {
            for (Node* n = buckets[i]; n; ++total)
                LinkHash(std::exchange(n, n->NextHash));
        }

        SwDebugAssert(total == node_count_);
    }

    void VFS::Clear()
    {
        usize total = 0;

        for (usize i = 0; i < bucket_count_; ++i)
        {
            for (Node* n = buckets_[i]; n; ++total)
                FreeNode(std::exchange(n, n->NextHash));
        }

        SwDebugAssert(total == node_count_);

        buckets_.reset();
        bucket_count_ = 0;
        node_count_ = 0;
    }

    usize VFS::GetNextCapacity(usize capacity)
    {
        usize result = bucket_count_ ? bucket_count_ : 32;

        while (result < capacity)
            result <<= 1;

        return result;
    }

    void VFS::LinkHash(Node* node)
    {
        SwDebugAssert(bucket_count_ != 0);

        node->NextHash = std::exchange(buckets_[node->Hash & (bucket_count_ - 1)], node);
    }

    VFS::File* VFS::GetFile(StringView name) const
    {
        if (u32 hash = NodeHashPath(name); NodeHashIsFile(hash))
        {
            if (Node* node = FindNode(name))
                return node->FileData;
        }

        return nullptr;
    }

    VFS::Node* VFS::GetFolder(StringView name) const
    {
        if (u32 hash = NodeHashPath(name); NodeHashIsFolder(hash))
            return FindNode(name, hash);

        return nullptr;
    }

    VFS::Node* VFS::FindNode(StringView name, u32 hash) const
    {
        if (bucket_count_ == 0)
            return nullptr;

        for (Node* n = buckets_[hash & (bucket_count_ - 1)]; n; n = n->NextHash)
        {
            if (n->Hash == hash && n->ComparePath(name))
                return n;
        }

        return nullptr;
    }

    VFS::Node* VFS::FindNode(StringView name) const
    {
        return FindNode(name, NodeHashPath(name));
    }

    Pair<VFS::Node*, bool> VFS::AddNode(StringView name, u32 hash)
    {
        // Don't create the node if it already exists
        if (Node* node = FindNode(name, hash))
        {
            // Return the existing node
            return {node, false};
        }

        // Split path into dirname and basename
        const auto [dirname, basename] = ParentPath(name);

        Node* parent = nullptr;

        // If this isn't the root folder, try getting the parent folder
        if (!name.empty())
        {
            // TODO: Add option for auto-creating folders
            parent = AddFolder(dirname);
            // parent = GetFolder(dirname);

            // Can't create a node if its parent folder doesn't exist
            if (parent == nullptr)
                return {nullptr, false};
        }
        else
        {
            SwDebugAssert(hash == NodeHashPath(""));
        }

        // Allocate the node
        Node* node = AllocNode(hash, basename);

        // Add the new node to the hash table
        Reserve(node_count_ + 1);
        LinkHash(node);
        ++node_count_;

        // Add the new node to the parent folder
        if (parent)
            parent->AddChild(node);

        // Return the new node
        return {node, true};
    }

    VFS::Node* VFS::AllocNode(u32 hash, StringView name)
    {
        if (name.size() > Node::MaxNameLength)
            throw std::runtime_error("Node name is too long");

        // The behavior of offsetof is undefined when the type does not have a standard layout
        static_assert(std::is_standard_layout_v<Node>);
        static_assert(alignof(Node) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);

        return new (vla_size(Node, Name, name.size() + 1)) Node(hash, name);
    }

    void VFS::FreeNode(Node* node)
    {
        node->~Node();

        operator delete(node);
    }

    bool VFS::Exists(StringView path) const
    {
        return FindNode(path) != nullptr;
    }

    VFS::File** VFS::AddFile(StringView name)
    {
        // Hash the full path
        u32 hash = NodeHashPath(name);

        // Make sure the name is not a folder
        if (!NodeHashIsFile(hash))
            return nullptr;

        auto [node, is_new] = AddNode(name, hash);

        return is_new ? &node->FileData : nullptr;
    }

    VFS::Node* VFS::AddFolder(StringView name)
    {
        // Hash the full path
        u32 hash = NodeHashPath(name);

        // Make sure the name is a folder
        if (!NodeHashIsFolder(hash))
            return nullptr;

        auto [node, is_new] = AddNode(name, hash);

        if (is_new)
            node->FolderData = new Folder();

        return node;
    }

    class VFS::Enumerator final : public FileEnumerator
    {
    public:
        Enumerator(Folder* folder)
            : node_(folder->FirstChild)
        {}

        bool Next(FolderEntry& entry) override
        {
            entry.Reset();

            if (node_ == nullptr)
                return false;

            Node* node = node_;
            node_ = node->NextSibling;

            StringView name = node->GetName();

            if (node->IsFolder())
            {
                // All folder names end with a slash (apart from the root folder, which can never be a child)
                SwDebugAssert(name.back() == '/');
                name.remove_suffix(1);

                entry.IsFolder = true;
                entry.Name = name;
            }
            else
            {
                entry.IsFolder = false;
                entry.Name = name;

                if (File* file = node->FileData)
                    file->Ops->Stat(*file, entry);
            }

            return true;
        }

    private:
        Node* node_ {};
    };

    Ptr<FileEnumerator> VFS::Enumerate(StringView path)
    {
        if (Node* folder = GetFolder(path))
            return MakeUnique<Enumerator>(folder->FolderData);

        return nullptr;
    }

    bool VFS::FileOperations::Extension([[maybe_unused]] File& file, [[maybe_unused]] FileDeviceExtension& data)
    {
        return false;
    }

    Pair<StringView, StringView> SplitPath(StringView path)
    {
        usize split = path.rfind('/') + 1;

        return {path.substr(0, split), path.substr(split)};
    }
} // namespace Swage