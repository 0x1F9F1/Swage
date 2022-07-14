#pragma once

#include "core/callback.h"

namespace Swage
{
    struct SecretId
    {
        u32 Length {};
        u32 Hash {};
        u64 Crc {};

        String To85() const;

        static Option<SecretId> From85(StringView text);
        static SecretId FromData(const void* data, usize length);

        inline bool operator==(const SecretId& rhs) const
        {
            return (Length == rhs.Length) && (Hash == rhs.Hash) && (Crc == rhs.Crc);
        }
    };
} // namespace Swage

template <>
struct std::hash<Swage::SecretId>
{
    std::size_t operator()(const Swage::SecretId& s) const noexcept
    {
        std::size_t seed = 0;
        Swage::HashCombine(seed, s.Length);
        Swage::HashCombine(seed, s.Hash);
        Swage::HashCombine(seed, s.Crc);
        return seed;
    }
};

namespace Swage
{
    class Stream;

    class SecretFinder
    {
    public:
        void Add(SecretId id);
        void Add(const char* id);

        HashMap<SecretId, Vec<u8>> Search(Stream& stream);

    private:
        HashSet<SecretId> secrets_;
    };

    class SecretStore
    {
    public:
        void Add(Vec<u8> data);
        void Add(const HashMap<SecretId, Vec<u8>>& secrets);

        bool Get(SecretId id, void* output, usize length);
        bool Get(const char* id, void* output, usize length);

        void Load(Stream& input);
        void Save(Stream& output);

        void Load();
        void Save();

    private:
        HashMap<SecretId, Vec<u8>> secrets_;
    };

    extern SecretStore Secrets;
} // namespace Swage