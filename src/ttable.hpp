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
    enum class Bound : U8 {
        NONE,
        EXACT,
        LOWER,
        UPPER
    };

    I32 score;
    I32 staticEval;
    Move move;
    I32 depth;
    bool pv;
    Bound bound;
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

    std::pair<bool, TTableEntry> probe(U64 key, I32 ply) const {
        const Bucket &bucket = table_[index(key)];
        USize entryIndex = 0;
        bool found = false;
        U16 key16 = static_cast<U16>(key & 0xFFFF);
        for (USize i = 0; i < ENTRIES; i++) {
            if (bucket.entries[i].key16 == key16) {
                entryIndex = i;
                found = true;
                break;
            }
        }

        if (!found) {
            return std::make_pair(found, TTableEntry{});
        }

        const RawEntry &entry = bucket.entries[entryIndex];
        TTableEntry result = TTableEntry{};
        result.score = retrieve(entry.score, ply);
        result.staticEval = static_cast<I32>(entry.staticEval);
        result.move = entry.move;
        result.depth = static_cast<I32>(entry.depth);
        result.pv = entry.pv();
        result.bound = entry.bound();
        return std::make_pair(found, result);
    }

    void write(U64 key, I32 ply, I32 score, I32 staticEval, Move move, I32 depth, bool pv, TTableEntry::Bound bound) {
        U16 key16 = static_cast<U16>(key & 0xFFFF);
        Bucket &bucket = table_[index(key)];
        I32 bestQuality = std::numeric_limits<I32>::max();
        USize replaceIndex = 0;
        for (USize i = 0; i < ENTRIES; i++) {
            if (bucket.entries[i].key16 == key16) {
                replaceIndex = i;
                break;
            }

            I32 entryQuality = quality(bucket.entries[i].gen(), bucket.entries[i].depth);
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
            replace.staticEval = static_cast<I16>(staticEval);
            replace.depth = static_cast<U8>(depth);
            replace.setBoundPVGen(bound, pv, static_cast<U8>(age_));
        }
    }

    I32 occupancy() const {
        I32 count = 0;
        for (USize i = 0; i < size_; i++) {
            for (USize j = 0; j < ENTRIES; j++) {
                const RawEntry &entry = table_[i].entries[j];
                if (entry.bound() != TTableEntry::Bound::NONE && entry.gen() == age_) {
                    count++;
                }
            }
        }
        return (count * 1000) / (size_ * ENTRIES);
    }

    void incrementAge() {
        age_ = (age_ + 1) % GENERATIONS;
    }

private:
    static constexpr USize ENTRIES = 3;
    static constexpr I32 GENERATIONS = 8;

    struct RawEntry {
        U16 key16;
        I16 score;
        I16 staticEval;
        Move move;
        U8 depth;
        U8 boundPVGen;

        TTableEntry::Bound bound() const {
            return static_cast<TTableEntry::Bound>(boundPVGen & 3);
        }

        bool pv() const {
            return boundPVGen & 4;
        }

        U8 gen() const {
            return boundPVGen >> 3;
        }

        void setBoundPVGen(TTableEntry::Bound bound, bool pv, U8 gen) {
            boundPVGen = static_cast<U8>(bound) | (pv << 2) | (gen << 3);
        }
    };

    struct alignas(32) Bucket {
        std::array<RawEntry, ENTRIES> entries;
        U8 padding[2];
    };

    Bucket *table_;
    USize size_;
    I32 age_;

    I32 quality(I32 age, I32 depth) const {
        I32 ageDiff = (age_ - age) % GENERATIONS;
        if (ageDiff < 0) {
            ageDiff += GENERATIONS;
        }
        return depth - (2 * ageDiff);
    }

    I32 retrieve(I16 score, I32 ply) const {
        if (Score::mate(score)) {
            if (score < 0) {
                return score + ply;
            } else {
                return score - ply;
            }
        }
        return score;
    }

    I16 store(I32 score, I32 ply) const {
        if (Score::mate(score)) {
            if (score < 0) {
                return score - ply;
            } else {
                return score + ply;
            }
        }
        return static_cast<I16>(score);
    }

    USize index(U64 key) const {
        return mulHi64(key, size_);
    }

#if defined(__GNUC__) || defined(__clang__)

    U64 mulHi64(U64 a, U64 b) const {
        return __uint128_t(a) * __uint128_t(b) >> 64;
    }

#elif defined(_MSC_VER) && !defined(__clang__)

    U64 mulHi64(U64 a, U64 b) const {
        return __umulh(a, b);
    }

#else

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
