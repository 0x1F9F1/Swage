#include "vfs.h"

#include "core/bits.h"

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
    static constexpr u32 NodeHashFactor = 0x9E3779B1;
    static constexpr u32 NodeHashFolderBit = 0x00000001;

    static inline u32 NodeHashFinalize(u32 hash, bool is_folder)
    {
        hash *= NodeHashFactor;
        hash &= ~NodeHashFolderBit;
        hash |= is_folder ? NodeHashFolderBit : 0;

        return hash;
    }

    static inline bool NodeHashIsFolder(u32 hash)
    {
        return (hash & NodeHashFolderBit) != 0;
    }

    static inline bool NodeHashIsFile(u32 hash)
    {
        return (hash & NodeHashFolderBit) == 0;
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
                delete folder;
        }
        else
        {
            if (File* file = FileData)
                Rc(file->Ops)->Delete(file);
        }
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

    static u32 NodeHashPath(u32 hash, const char* path, usize len)
    {
        for (usize i = 0; i != len; ++i)
        {
            hash *= NodeHashFactor;
            hash += static_cast<unsigned char>(path[i]);
        }

        return hash;
    }

    static u32 NodeHashPathI(u32 hash, const char* path, usize len)
    {
        for (usize i = 0; i != len; ++i)
        {
            hash *= NodeHashFactor;
            hash += static_cast<unsigned char>(ToLower(path[i]));
        }

        return hash;
    }

    static usize NodeMatchSuffix(const char* path, usize path_len, const char* name, usize name_len)
    {
        if (name_len > path_len)
            return SIZE_MAX;

        path += path_len - name_len;

        for (usize i = 0; i != name_len; ++i)
        {
            if (path[i] != name[i])
                return SIZE_MAX;
        }

        return name_len;
    }

    static usize NodeMatchSuffixI(const char* path, usize path_len, const char* name, usize name_len)
    {
        if (name_len > path_len)
            return SIZE_MAX;

        path += path_len - name_len;

        for (usize i = 0; i != name_len; ++i)
        {
            if (ToLower(path[i]) != ToLower(name[i]))
                return SIZE_MAX;
        }

        return name_len;
    }

    VFS::VFS(bool case_sensitive)
        : hash_path_(case_sensitive ? NodeHashPath : NodeHashPathI)
        , match_suffix_(case_sensitive ? NodeMatchSuffix : NodeMatchSuffixI)
    {}

    VFS::~VFS()
    {
        Clear();
    }

    void VFS::Clear()
    {
        Vec<Node*> buckets;
        buckets.swap(buckets_);

        node_count_ = 0;
        hash_shift_ = 0;

        for (Node* n : buckets)
        {
            while (n)
                FreeNode(std::exchange(n, n->NextHash));
        }
    }

    void VFS::LinkHash(Node* node)
    {
        SwDebugAssert(!buckets_.empty());

        node->NextHash = std::exchange(buckets_[node->Hash >> hash_shift_], node);
    }

    inline u32 VFS::HashPath(StringView path) const
    {
        u32 hash = 0;
        bool is_folder = true;

        if (!path.empty())
        {
            hash = hash_path_(hash, path.data(), path.size());
            is_folder = path.back() == '/';
        }

        return NodeHashFinalize(hash, is_folder);
    }

    inline bool VFS::ComparePath(const Node* node, StringView full_path) const
    {
        StringView path = full_path;

        // Compare the segments of each node
        // Avoid one loop iteration by stopping at the root node (as the name is always empty)
        while (const Node* parent = node->Parent)
        {
            // Get the name of the current node
            StringView name = node->GetName();

            // Try and match this name segment
            usize matched = match_suffix_(path.data(), path.size(), name.data(), name.size());

            // Check if the name matches
            if (matched == SIZE_MAX)
                return false;

            // Remove the segment of the name which was just compared.
            path.remove_suffix(matched);

            // Move to the next (parent) node
            node = parent;
        }

        // We've compared the whole node path, so make sure we aren't expecting any more characters.
        return path.empty();
    }

    inline void VFS::Reserve(usize capacity)
    {
        // x + (x >> 2) == x * 1.25. load factor = 1 / 1.25 = 0.8
        capacity += capacity >> 2;

        // Don't resize if the bucket count is less.
        if (capacity > buckets_.size())
            Resize(capacity);
    }

    void VFS::Resize(usize capacity)
    {
        SwAssert(capacity > 0);
        u8 cap_bits = bits::bsr_ceil(capacity);
        cap_bits = std::max<u8>(cap_bits, 5);

        SwAssert(cap_bits < 32);
        capacity = usize(1) << cap_bits;
        hash_shift_ = 32 - cap_bits;

        Vec<Node*> buckets(capacity);
        buckets.swap(buckets_);

        for (Node* n : buckets)
        {
            while (n)
                LinkHash(std::exchange(n, n->NextHash));
        }
    }

    VFS::File* VFS::GetFile(StringView name) const
    {
        if (u32 hash = HashPath(name); NodeHashIsFile(hash))
        {
            if (Node* node = FindNode(name, hash))
                return node->FileData;
        }

        return nullptr;
    }

    VFS::Node* VFS::GetFolder(StringView name) const
    {
        if (u32 hash = HashPath(name); NodeHashIsFolder(hash))
            return FindNode(name, hash);

        return nullptr;
    }

    VFS::Node* VFS::FindNode(StringView name, u32 hash) const
    {
        if (hash_shift_ == 0)
            return nullptr;

        for (Node* n = buckets_[hash >> hash_shift_]; n; n = n->NextHash)
        {
            if (n->Hash == hash && ComparePath(n, name))
                return n;
        }

        return nullptr;
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
            SwDebugAssert(hash == HashPath(""));
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
        // Hash the full path
        u32 hash = HashPath(path);

        // Check if a node exists at this path
        return FindNode(path, hash) != nullptr;
    }

    VFS::File** VFS::AddFile(StringView name)
    {
        // Hash the full path
        u32 hash = HashPath(name);

        // Make sure the name is not a folder
        if (!NodeHashIsFile(hash))
            return nullptr;

        auto [node, is_new] = AddNode(name, hash);

        return is_new ? &node->FileData : nullptr;
    }

    VFS::Node* VFS::AddFolder(StringView name)
    {
        // Hash the full path
        u32 hash = HashPath(name);

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