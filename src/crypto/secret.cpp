#include "secret.h"

#include "asset/stream.h"
#include "core/bits.h"
#include "crc.h"

namespace Swage
{
    template <typename T>
    inline bool bit_test(const T* bits, usize index)
    {
        constexpr usize Radix = sizeof(T) * CHAR_BIT;

#if defined(_MSC_VER) && !defined(__clang__) && (defined(_M_IX86) || defined(_M_X64))
        // `index % Radix` should be a no-op on x86, as it is masked off by shl/shr/bt
        // Yet MSVC still generates a horrible `movzx, and, movzx` sequence
        // Note also, _bittest{64} is not recommended due to its high latency
        return (bits[index / Radix] & (T(1) << (index /*% Radix*/))) != 0;
#else
        return (bits[index / Radix] & (T(1) << (index % Radix))) != 0;
#endif
    }

    template <typename T>
    inline void bit_set(T* bits, usize index)
    {
        constexpr usize Radix = sizeof(T) * CHAR_BIT;

        bits[index / Radix] |= (T(1) << (index % Radix));
    }

    // b85alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~"
    // https://github.com/python/cpython/blob/22df2e0322300d25c1255ceb73cacc0ebd96b20e/Lib/base64.py#L461

    inline char b85encode1(u8 value)
    {
        static const char encoder[85 + 1] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";

        return (value < 85) ? encoder[value] : '~';
    }

    usize b85encode(const void* input, usize input_len, char* output, usize output_len)
    {
        const u8* next_in = static_cast<const u8*>(input);
        char* next_out = output;

        usize avail_in = input_len;
        usize avail_out = output_len;

        while (avail_in && avail_out)
        {
            u32 acc = 0;
            usize valid = 1;

            for (usize i = 0; i < 4; ++i)
            {
                acc <<= 8;

                if (avail_in)
                {
                    acc |= static_cast<u32>(*next_in++);
                    --avail_in;

                    ++valid;
                }
            }

            char chunk[5] {};

            for (usize i = 0; i < 5; ++i)
            {
                chunk[4 - i] = b85encode1(acc % 85);
                acc /= 85;
            }

            for (usize i = 0; i < valid; ++i)
            {
                if (avail_out)
                {
                    *next_out++ = chunk[i];
                    --avail_out;
                }
            }
        }

        return output_len - avail_out;
    }

    inline u8 b85decode1(char value)
    {
        static const u8 decoder[94] = {
            // clang-format off
            62, 85, 63, 64, 65, 66, 85, 67, 68, 69, 70, 85, 71, 85, 85,
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  85, 72, 73, 74, 75,
            76, 77, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
            23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 85, 85,
            85, 78, 79, 80, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
            47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
            81, 82, 83, 84,
            // clang-format on
        };

        return (value >= 33 && value <= 126) ? decoder[value - 33] : 85;
    }

    usize b85decode(const char* input, usize input_len, void* output, usize output_len)
    {
        const char* next_in = input;
        u8* next_out = static_cast<u8*>(output);

        usize avail_in = input_len;
        usize avail_out = output_len;

        while (avail_in && avail_out)
        {
            u32 acc = 0;
            usize valid = 0;

            for (usize i = 0; i < 5; ++i)
            {
                u8 v = 84;

                if (avail_in)
                {
                    v = b85decode1(*next_in++);
                    --avail_in;

                    if (v >= 85)
                        return 0;

                    ++valid;
                }

                acc = (acc * 85) + v;
            }

            for (usize i = 1; i < valid; ++i)
            {
                if (avail_out)
                {
                    *next_out++ = acc >> 24;
                    --avail_out;
                    acc <<= 8;
                }
            }
        }

        return output_len - avail_out;
    }

    void SearchOverlappingChunks(usize chunk_size, usize chunk_overlap, usize num_chunks, usize num_workers,
        usize num_threads, Callback<Pair<u64, usize>(void*, usize)> read_data,
        Callback<bool(usize, const u8*, const u8*, usize)> process_data)
    {
        struct Chunk
        {
            // Start of new Data
            u8* Data {};

            // Length of new data
            usize Length {};

            // Length of data saved from the previous chunk
            usize Overlap {};

            // Number of workers that need to start processing this chunk
            usize Pending {};

            // Number of workers that need to finish processing this chunk
            usize Running {};

            // Pointer to the start of the raw data
            Ptr<u8[]> RawData {};
        };

        std::mutex chunk_lock;
        Vec<Chunk> chunks(num_chunks);
        usize reader_index = 0;
        usize writer_index = 0;
        bool finished = false;

        std::condition_variable reader_cond;
        std::condition_variable writer_cond;

        const auto worker_func = [&, process_data] {
            std::unique_lock lock(chunk_lock);

            while (true)
            {
                auto& chunk = chunks[reader_index];

                if (chunk.Pending == 0)
                {
                    if (finished)
                        break;

                    reader_cond.wait(lock);
                    continue;
                }

                usize work_index = num_workers - chunk.Pending;

                if (--chunk.Pending == 0)
                    reader_index = (reader_index + 1) % chunks.size();

                lock.unlock();

                bool stop =
                    !process_data(work_index, chunk.Data - chunk.Overlap, chunk.Data + chunk.Length, chunk.Overlap);

                lock.lock();

                if (--chunk.Running == 0)
                    writer_cond.notify_one();

                if (stop)
                    finished = true;
            }
        };

        Vec<std::thread> workers;

        for (usize i = 0; i < num_threads; ++i)
            workers.emplace_back(worker_func);

        u8* saved_data = nullptr;
        usize saved = 0;
        u64 prev_pos = 0;

        {
            std::unique_lock lock(chunk_lock);

            while (!finished)
            {
                auto& chunk = chunks[writer_index];

                if (chunk.Running != 0)
                {
                    writer_cond.wait(lock);
                    continue;
                }

                writer_index = (writer_index + 1) % num_chunks;

                lock.unlock();

                if (chunk.Data == nullptr)
                {
                    chunk.RawData.reset(new u8[chunk_overlap + chunk_size]);
                    chunk.Data = &chunk.RawData[chunk_overlap];
                }

                auto [read_pos, read] = read_data(chunk.Data, chunk_size);

                if (read == 0)
                    break;

                if (read_pos != prev_pos)
                    saved = 0;

                if (saved)
                    std::memmove(chunk.Data - saved, saved_data, saved);

                chunk.Length = read;
                chunk.Overlap = saved;

                prev_pos = read_pos + read;
                saved = (std::min)(saved + read, chunk_overlap);
                saved_data = chunk.Data + read - saved;

                lock.lock();

                chunk.Pending = num_workers;
                chunk.Running = num_workers;

                reader_cond.notify_all();
            }

            finished = true;
            reader_cond.notify_all();
        }

        for (auto& worker : workers)
            worker.join();
    }

    // Closest prime to floor(2**32/golden_ratio)
    static constexpr u32 PolyFactor32 = 0x9E3779B1;

    // Using a non-zero seed will break the "rolling" property of the hash.
    // It is also the same as just returning `hash + seed * pow(PolyFactor32, length)`
    inline u32 PolyHash32(const void* data, usize length)
    {
        const u8* data8 = static_cast<const u8*>(data);

        u32 hash = 0;

        for (usize i = 0; i < length; ++i)
        {
            hash += data8[i];
            hash *= PolyFactor32;
        }

        return hash;
    }

    // Returns `-1 * pow(HashFactor, length, 2**32)`
    // Used to remove (subtract) the length-1'th value from a hash
    inline u32 PolyHashInverse(usize length)
    {
        u32 v = 0xFFFFFFFF;

        for (u32 x = PolyFactor32; length; x *= x, length >>= 1)
        {
            if (length & 1)
                v *= x;
        }

        return v;
    }

    // Returns a permutation of the integer to use as a second hash
    inline u32 Permute32(u32 x)
    {
        return ~x * PolyFactor32;
    }

    static_assert(sizeof(SecretId) == 0x10);

    String SecretId::To85() const
    {
        char buffer[20];
        usize encoded = b85encode(this, sizeof(*this), buffer, std::size(buffer));
        SwAssert(encoded == std::size(buffer));
        return String(buffer, encoded);
    }

    SecretId SecretId::From85(StringView text)
    {
        SecretId result;
        usize decoded = b85decode(text.data(), text.size(), &result, sizeof(result));

        if (decoded != sizeof(result))
            throw std::runtime_error("Failed to decode secret");

        if (result.Length == 0)
            throw std::runtime_error("Invalid secret length");

        return result;
    }

    SecretId SecretId::FromData(const void* data, usize length)
    {
        return {static_cast<u32>(length), PolyHash32(data, length), CRC64(data, length)};
    }

    struct SecretSearcher
    {
        SecretSearcher(usize length);

        void AddSecret(SecretId secret);
        void Finalize();

        u32 Begin(const u8* data) const;
        u32 Next(const u8*& start, const u8* end, u32& state) const;

        bool Search(
            const u8* start, const u8* end, usize overlap, Callback<bool(SecretId, const u8*)> add_result) const;

        usize Length {};
        u32 InvFactor {};

        using FilterWord = usize;

        Ptr<FilterWord[]> Filter {};
        u8 FilterBits {};

        HashMap<u32, Vec<SecretId>> Secrets {};
        usize NumSecrets {};
    };

    SecretSearcher::SecretSearcher(usize length)
        : Length(length)
        , InvFactor(PolyHashInverse(length))
    {}

    void SecretSearcher::AddSecret(SecretId secret)
    {
        auto& secrets = Secrets[secret.Hash];

        if (std::find(secrets.begin(), secrets.end(), secret) != secrets.end())
            return;

        secrets.push_back(secret);
        ++NumSecrets;
    }

    void SecretSearcher::Finalize()
    {
        constexpr usize LoadFactor = 256;

        FilterBits = std::clamp<u8>(bits::bsr_ceil(Secrets.size() * LoadFactor), 10, 18);

        constexpr usize FilterRadix = sizeof(FilterWord) * CHAR_BIT;
        usize filter_size = usize(1) << FilterBits;
        Filter.reset(new FilterWord[1 + ((filter_size - 1) / FilterRadix)] {});

        u8 filter_shift = 32 - FilterBits;

        // Setup a bloom filter using 3 hash functions.
        // Calculating additional indepenent hashes would be slower than just performing a lookup into the Hashes table.
        for (const auto& [hash, _] : Secrets)
        {
            u32 h = hash;
            bit_set(Filter.get(), h >> filter_shift);

            h = Permute32(h);
            bit_set(Filter.get(), h >> filter_shift);

            h = Permute32(h);
            bit_set(Filter.get(), h >> filter_shift);
        }
    }

    inline u32 SecretSearcher::Begin(const u8* data) const
    {
        return PolyHash32(data, Length - 1);
    }

#if defined(_M_IX86) // Inlining increases register spills on x86
    SW_NOINLINE
#endif
    u32 SecretSearcher::Next(const u8*& inout_start, const u8* end, u32& inout_state) const
    {
        const usize last = Length - 1;
        const u32 inv_factor = InvFactor;

        const FilterWord* filter = Filter.get();
        const u8 filter_shift = 32 - FilterBits;

        const u8* here = inout_start;
        u32 state = inout_state;
        u32 hash = 0;

        while (SW_LIKELY(here < end))
        {
            [[SW_ATTR_LIKELY]];

            u32 h = (state + here[last]) * PolyFactor32;
            state = h + (here[0] * inv_factor);
            ++here;

            if (SW_LIKELY(!bit_test(filter, h >> filter_shift)))
            {
                [[SW_ATTR_LIKELY]];
                continue;
            }

            h = Permute32(h);
            if (SW_LIKELY(!bit_test(filter, h >> filter_shift)))
            {
                [[SW_ATTR_LIKELY]];
                continue;
            }

            h = Permute32(h);
            if (SW_LIKELY(!bit_test(filter, h >> filter_shift)))
            {
                [[SW_ATTR_LIKELY]];
                continue;
            }

            --here;
            hash = state - (here[0] * inv_factor);
            break;
        }

        inout_start = here;
        inout_state = state;

        return hash;
    }

    bool SecretSearcher::Search(
        const u8* start, const u8* end, usize overlap, Callback<bool(SecretId, const u8*)> add_result) const
    {
        const usize last = Length - 1;

        start += overlap - (std::min)(overlap, last);

        if (static_cast<usize>(end - start) <= last)
            return true;

        end -= last;

        u32 state = Begin(start);

        // TODO: Add fast path for NumSecrets == 1

        while (start < end)
        {
            const u8* here = start;
            u32 hash = Next(here, end, state);

            if (here < end)
            {
                if (auto iter = Secrets.find(hash); iter != Secrets.end())
                {
                    for (const auto& secret : iter->second)
                    {
                        if (secret.Crc == CRC64(here, secret.Length))
                        {
                            if (!add_result(secret, here))
                                return false;
                        }
                    }
                }

                ++here;
            }

            start = here;
        }

        return true;
    }

    static Vec<SecretSearcher> MakeSearchers(HashSet<SecretId> secrets)
    {
        Vec<SecretSearcher> searchers;

        for (const auto& secret : secrets)
        {
            auto iter = std::find_if(searchers.begin(), searchers.end(),
                [&](const auto& searcher) { return searcher.Length == secret.Length; });

            if (iter == searchers.end())
            {
                iter = searchers.emplace(iter, secret.Length);
            }

            iter->AddSecret(secret);
        }

        for (auto& searcher : searchers)
            searcher.Finalize();

        return searchers;
    }

    HashMap<SecretId, Vec<u8>> FindSecrets(
        HashSet<SecretId> secrets, Callback<Pair<u64, usize>(void*, usize)> read_data)
    {
        const auto searchers = MakeSearchers(std::move(secrets));

        usize num_secrets = 0;
        usize prefix_length = 0;
        HashMap<usize, Atomic<usize>> num_pending;

        for (const auto& searcher : searchers)
        {
            num_pending.emplace(searcher.Length, searcher.NumSecrets);
            num_secrets += searcher.NumSecrets;
            prefix_length = std::max<usize>(prefix_length, searcher.Length);
        }

        prefix_length = (prefix_length + 0x3F) & ~usize(0x3F);

        Mutex results_lock;
        HashMap<SecretId, Vec<u8>> results;

        const auto add_result = [&results_lock, &results, &num_pending, num_secrets](SecretId secret, const u8* data) {
            LockGuard<Mutex> lock(results_lock);

            if (auto [iter, added] = results.try_emplace(secret, data, data + secret.Length); added)
            {
                --num_pending.at(secret.Length);
            }
            else if (std::memcmp(data, iter->second.data(), iter->second.size()))
            {
                // TODO: How should this be handled?

                SwLogError("Duplicate result!");
            }

            return results.size() < num_secrets;
        };

        const auto process_data = [&searchers, &num_pending, &add_result](
                                      usize index, const u8* start, const u8* end, usize overlap) {
            const auto& searcher = searchers.at(index);

            if (num_pending.at(searcher.Length) == 0)
                return true;

            return searcher.Search(start, end, overlap, add_result);
        };

        usize const num_workers = searchers.size();
        usize const num_threads = std::clamp<usize>(std::thread::hardware_concurrency(), 4, 16) - 1;

        usize const num_chunks = 16;
        usize const chunk_size = usize(2) << 20;

        SearchOverlappingChunks(
            chunk_size, prefix_length, num_chunks, num_workers, num_threads, read_data, process_data);

        return results;
    }

    void SecretFinder::Add(SecretId id)
    {
        secrets_.emplace(id);
    }

    void SecretFinder::Add(const char* id)
    {
        Add(SecretId::From85(id));
    }

    HashMap<SecretId, Vec<u8>> SecretFinder::Search(Stream& stream)
    {
        HashMap<SecretId, Vec<u8>> results;
        HashSet<SecretId> needles;

        Vec<u8> data;

        for (const auto& secret : secrets_)
        {
            data.resize(secret.Length);

            if (Secrets.Get(secret, data.data(), ByteSize(data)))
            {
                results.emplace(secret, data);
            }
            else
            {
                needles.emplace(secret);
            }
        }

        const auto read_data = [&stream](void* buffer, usize length) -> Pair<u64, usize> {
            length = stream.Read(buffer, length);

            // Calculate offset after reading, to support sparse streams (Win32ProcStream)
            u64 here = stream.Tell() - length;

            return {here, length};
        };

        if (!needles.empty())
        {
            HashMap<SecretId, Vec<u8>> found = FindSecrets(needles, read_data);
            results.insert(found.begin(), found.end());
        }

        return results;
    }

    SecretStore Secrets;

    void SecretStore::Add(Vec<u8> data)
    {
        SecretId id = SecretId::FromData(data.data(), data.size());

        secrets_.emplace(id, std::move(data));
    }

    void SecretStore::Add(const HashMap<SecretId, Vec<u8>>& secrets)
    {
        for (const auto& [_, data] : secrets)
            Add(data);
    }

    bool SecretStore::Get(SecretId id, void* output, usize length)
    {
        auto iter = secrets_.find(id);

        if (iter == secrets_.end())
            return false;

        if (iter->second.size() != length)
            return false;

        std::memcpy(output, iter->second.data(), iter->second.size());

        return true;
    }

    bool SecretStore::Get(const char* id, void* output, usize length)
    {
        return Get(SecretId::From85(id), output, length);
    }

    void SecretStore::Load(Stream& input)
    {
        u32 length = 0;

        while (input.TryRead(&length, sizeof(length)))
        {
            Vec<u8> data(length);

            if (!input.TryRead(data.data(), ByteSize(data)))
                break;

            Add(std::move(data));
        }
    }

    void SecretStore::Save(Stream& output)
    {
        for (const auto& [_, data] : secrets_)
        {
            u32 length = static_cast<u32>(data.size());
            output.Write(&length, sizeof(length));
            output.Write(data.data(), ByteSize(data));
        }
    }
} // namespace Swage
