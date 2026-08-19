#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#include "move.hpp"
#include "score.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"


namespace Sift {

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
    explicit TTable(USize sizeMB) noexcept : table_(nullptr), size_(0), capacity_(0), age_(0) { resize(sizeMB, 1); }

    ~TTable() noexcept {
        if (table_) {
            std::free(table_);
        }
    }

    void resize(USize sizeMB, USize numThreads) noexcept {
        assert(sizeMB > 0);

        const USize newSize = (sizeMB * 1024 * 1024) / sizeof(Bucket);

        if (newSize > capacity_ || newSize <= capacity_ / 2) {
            deallocate();
            allocate(newSize);
        }
        reset(numThreads);
    }

    void reset(USize numThreads) noexcept {
        age_ = 0;
        std::vector<std::jthread> threads;
        threads.reserve(numThreads);
        for (USize i = 0; i < numThreads; i++) {
            threads.emplace_back([this, i, numThreads] {
                USize start = (size_ * i) / numThreads;
                USize end = (size_ * (i + 1)) / numThreads;
                std::fill(table_ + start, table_ + end, Bucket());
            });
        }
    }

    std::pair<TTableEntry, bool> probe(UInt64 key, Int32 ply) const noexcept {
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

    void write(UInt64 key, Int32 ply, Int32 score, Int32 staticEval, Move move, Int32 depth, bool pv, TTableEntry::Bound bound) noexcept {
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

        if (bound == TTableEntry::Bound::EXACT || replace.key16 != key16 || depth >= replace.depth - TTABLE_REPLACE_DEPTH_MARGIN - (TTABLE_REPLACE_DEPTH_PV_SCALE * pv) || replace.gen() != age_) {
            replace.key16 = key16;
            replace.score = store(score, ply);
            replace.staticEval = static_cast<Int16>(staticEval);
            replace.depth = static_cast<UInt8>(depth);
            replace.setBoundPVGen(bound, pv, static_cast<UInt8>(age_));
        }
    }

    void prefetch(UInt64 key) const noexcept { Utils::prefetchPtr(static_cast<const void *>(&table_[index(key)])); }

    USize hashfull() const noexcept {
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

    void incrementAge() noexcept { age_ = (age_ + 1) % GENERATIONS; }

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

        TTableEntry::Bound bound() const { return static_cast<TTableEntry::Bound>(boundPVGen & 3); }
        bool pv() const { return boundPVGen & 4; }
        UInt8 gen() const { return boundPVGen >> 3; }

        void setBoundPVGen(TTableEntry::Bound bound, bool pv, UInt8 gen) {
            boundPVGen = static_cast<UInt8>(bound) | (static_cast<UInt8>(pv << 2) | static_cast<UInt8>(gen << 3));
        }
    };

    struct alignas(64) Bucket {
        RawEntry entries[ENTRIES];
        UInt8 padding[2];
    };

    Bucket *table_;
    USize size_;
    USize capacity_;
    Int32 age_;

    Int32 quality(Int32 age, Int32 depth) const noexcept {
        Int32 ageDiff = (age_ - age) % GENERATIONS;
        if (ageDiff < 0) {
            ageDiff += GENERATIONS;
        }
        return depth - (2 * ageDiff);
    }

    Int32 retrieve(Int16 score, Int32 ply) const noexcept {
        if (Score::mate(score)) {
            return (score < 0) ? (score + ply) : (score - ply);
        }
        return score;
    }

    Int16 store(Int32 score, Int32 ply) const noexcept {
        if (Score::mate(score)) {
            return (score < 0) ? static_cast<Int16>(score - ply) : static_cast<Int16>(score + ply);
        }
        return static_cast<Int16>(score);
    }

    USize index(UInt64 key) const noexcept { return Utils::mulHi64(key, size_); }

    void allocate(USize newSize) noexcept {
        assert(table_ == nullptr);
        assert(size_ == 0);
        assert(capacity_ == 0);

#if defined(__linux__)
        static constexpr USize PAGE_SIZE = 2 * 1024 * 1024;
#else
        static constexpr USize PAGE_SIZE = 4096;
#endif

        const USize trueNewSize = ((newSize * sizeof(Bucket) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

        table_ = static_cast<Bucket *>(Utils::alignedAlloc(trueNewSize, PAGE_SIZE));

#if defined(__linux__)
        madvise(table_, trueNewSize, MADV_HUGEPAGE);
#endif

        size_ = newSize;
        capacity_ = trueNewSize / sizeof(Bucket);
    }

    void deallocate() noexcept {
        if (table_ == nullptr) {
            return;
        }

        assert(size_ > 0);
        assert(capacity_ > 0);

        Utils::alignedFree(table_);
        table_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }
};

}
