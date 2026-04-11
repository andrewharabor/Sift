#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <utility>

#include "move.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Clownfish {

struct TTableEntry {
    enum class Bound : UInt8 {
        NONE,
        EXACT,
        LOWER,
        UPPER
    };

    Int32 score;
    Int32 staticEval;
    Move move;
    Int32 depth;
    bool pv;
    Bound bound;

    constexpr TTableEntry() noexcept : score(Score::NONE), staticEval(Score::NONE), move(Move::NULL_MOVE), depth(0), pv(false), bound(Bound::NONE) {}
};

class TTable {
public:
    TTable(USize sizeMB) : table_(nullptr), size_(0), age_(0) {
        resize(sizeMB);
    }

    ~TTable() {
        if (table_) {
            std::free(table_);
        }
    }

    void resize(USize sizeMB) {
        assert(sizeMB > 0);

        USize buckets = (sizeMB * 1024 * 1024) / sizeof(Bucket);
        if (table_) {
            std::free(table_);
        }
        table_ = static_cast<Bucket *>(std::aligned_alloc(64, buckets * sizeof(Bucket)));
        size_ = buckets;
        reset();
    }

    constexpr void reset() {
        age_ = 0;
        std::fill_n(table_, size_, Bucket{});
    }

    std::pair<TTableEntry, bool> probe(UInt64 key, Int32 ply) const {
        const Bucket &bucket = table_[index(key)];
        USize entryIndex = 0;
        bool found = false;
        const UInt16 key16 = static_cast<UInt16>(key & 0xFFFF);
        for (USize i = 0; i < ENTRIES; i++) {
            if (bucket.entries[i].key16 == key16) {
                entryIndex = i;
                found = true;
                break;
            }
        }

        if (!found) {
            return {TTableEntry(), false};
        }

        const RawEntry &entry = bucket.entries[entryIndex];
        TTableEntry result = TTableEntry();
        result.score = retrieve(entry.score, ply);
        result.staticEval = static_cast<Int32>(entry.staticEval);
        result.move = entry.move;
        result.depth = static_cast<Int32>(entry.depth);
        result.pv = entry.pv();
        result.bound = entry.bound();
        return {result, true};
    }

    void write(UInt64 key, Int32 ply, Int32 score, Int32 staticEval, Move move, Int32 depth, bool pv, TTableEntry::Bound bound) {
        const UInt16 key16 = static_cast<UInt16>(key & 0xFFFF);
        Bucket &bucket = table_[index(key)];
        Int32 bestQuality = std::numeric_limits<Int32>::max();
        USize replaceIndex = 0;
        for (USize i = 0; i < ENTRIES; i++) {
            if (bucket.entries[i].key16 == key16) {
                replaceIndex = i;
                break;
            }

            const Int32 entryQuality = quality(bucket.entries[i].gen(), bucket.entries[i].depth);
            if (entryQuality < bestQuality) {
                bestQuality = entryQuality;
                replaceIndex = i;
            }
        }

        RawEntry &replace = bucket.entries[replaceIndex];
        if (move != Move::NULL_MOVE || replace.key16 != key16) {
            replace.move = move;
        }

        if (bound == TTableEntry::Bound::EXACT || replace.key16 != key16 || depth >= replace.depth - 2 - (2 * pv) || replace.gen() != age_) {
            replace.key16 = key16;
            replace.score = store(score, ply);
            replace.staticEval = static_cast<Int16>(staticEval);
            replace.depth = static_cast<UInt8>(depth);
            replace.setBoundPVGen(bound, pv, static_cast<UInt8>(age_));
        }
    }

    void prefetch(UInt64 key) const {
        prefetchPtr(static_cast<const void *>(&table_[index(key)]));
    }

    USize hashfull() const {
        USize count = 0;
        USize sampleSize = std::min(size_, static_cast<USize>(1000));
        for (USize i = 0; i < sampleSize; i++) {
            for (USize j = 0; j < ENTRIES; j++) {
                const RawEntry &entry = table_[i].entries[j];
                if (entry.bound() != TTableEntry::Bound::NONE && entry.gen() == age_) {
                    count++;
                }
            }
        }
        return (count * 1000) / (sampleSize * ENTRIES);
    }

    void incrementAge() {
        age_ = (age_ + 1) % GENERATIONS;
    }

private:
    static constexpr USize ENTRIES = 3;
    static constexpr Int32 GENERATIONS = 8;

    struct RawEntry {
        UInt16 key16;
        Int16 score;
        Int16 staticEval;
        Move move;
        UInt8 depth;
        UInt8 boundPVGen;

        TTableEntry::Bound bound() const {
            return static_cast<TTableEntry::Bound>(boundPVGen & 3);
        }

        bool pv() const {
            return boundPVGen & 4;
        }

        UInt8 gen() const {
            return boundPVGen >> 3;
        }

        void setBoundPVGen(TTableEntry::Bound bound, bool pv, UInt8 gen) {
            boundPVGen = static_cast<UInt8>(bound) | (static_cast<UInt8>(pv << 2) | static_cast<UInt8>(gen << 3));
        }
    };

    struct alignas(32) Bucket {
        std::array<RawEntry, ENTRIES> entries;
        UInt8 padding[2];
    };

    Bucket *table_;
    USize size_;
    Int32 age_;

    Int32 quality(Int32 age, Int32 depth) const {
        Int32 ageDiff = (age_ - age) % GENERATIONS;
        if (ageDiff < 0) {
            ageDiff += GENERATIONS;
        }
        return depth - (2 * ageDiff);
    }

    Int32 retrieve(Int16 score, Int32 ply) const {
        if (Score::mate(score)) {
            if (score < 0) {
                return score + ply;
            } else {
                return score - ply;
            }
        }
        return score;
    }

    Int16 store(Int32 score, Int32 ply) const {
        if (Score::mate(score)) {
            if (score < 0) {
                return static_cast<Int16>(score - ply);
            } else {
                return static_cast<Int16>(score + ply);
            }
        }
        return static_cast<Int16>(score);
    }

    USize index(UInt64 key) const {
        return mulHi64(key, size_);
    }

#if defined(__GNUC__) || defined(__clang__)

    void prefetchPtr(const void *ptr) const {
        __builtin_prefetch(ptr);
    }

    UInt64 mulHi64(UInt64 a, UInt64 b) const {
        return __uint128_t(a) * __uint128_t(b) >> 64;
    }

#elif defined(_MSC_VER) && !defined(__clang__)

    void prefetchPtr(const void *ptr) const {
        _mm_prefetch(static_cast<const char *>(ptr), _MM_HINT_T0);
    }

    U64 mulHi64(U64 a, U64 b) const {
        return __umulh(a, b);
    }

#else

    void prefetchPtr(const void *ptr) const {}

    U64 mulHi64(U64 a, U64 b) const {
        U64 aLo = a & 0xFFFFFFFF;
        U64 aHi = a >> 32;
        U64 bLo = b & 0xFFFFFFFF;
        U64 bHi = b >> 32;
        U64 c1 = (aLo * bLo) >> 32;
        U64 c2 = aHi * bLo + c1;
        U64 c3 = aLo * bHi + (c2 & 0xFFFFFFFF);
        return aHi * bHi + (c2 >> 32) + (c3 >> 32);
    }

#endif
};

}
