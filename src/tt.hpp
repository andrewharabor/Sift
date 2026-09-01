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

enum class TTBound : UInt8 {
    NONE,
    EXACT,
    LOWER,
    UPPER
};

struct TTEntry {
    Int32 score;
    Int32 staticEval;
    Move move;
    Int32 fdepth;
    bool pv;
    TTBound bound;

    constexpr TTEntry() noexcept : score(Score::NONE), staticEval(Score::NONE), move(Move::NULL_MOVE), fdepth(0), pv(false), bound(TTBound::NONE) {}
};

class TT {
public:
    explicit TT(USize sizeMB) noexcept : table_(nullptr), size_(0), capacity_(0), age_(0) { resize(sizeMB, 1); }

    ~TT() noexcept {
        if (table_) {
            std::free(table_);
        }
    }

    void resize(USize sizeMB, USize numThreads) noexcept {
        assert(sizeMB > 0);

        const USize newSize = (sizeMB * 1024 * 1024) / sizeof(Cluster);

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
                std::fill(table_ + start, table_ + end, Cluster());
            });
        }
    }

    bool probe(TTEntry &entry, UInt64 hash, Int32 ply) const noexcept {
        const Cluster &cluster = table_[index(hash)];
        const UInt16 hash16 = static_cast<UInt16>(hash & 0xFFFF);
        for (USize i = 0; i < CLUSTER_SIZE; i++) {
            if (cluster.entries[i].filled && cluster.entries[i].hash16 == hash16) {
                const RawEntry &raw = cluster.entries[i];
                entry.score = retrieve(raw.score, ply);
                entry.staticEval = static_cast<Int32>(raw.staticEval);
                entry.move = raw.move;
                entry.fdepth = static_cast<Int32>(raw.fdepth);
                entry.pv = raw.pv();
                entry.bound = raw.bound();
                return true;
            }
        }

        return false;
    }

    void write(UInt64 hash, Int32 ply, Int32 score, Int32 staticEval, Move move, Int32 fdepth, bool pv, TTBound bound) noexcept {
        const UInt16 hash16 = static_cast<UInt16>(hash & 0xFFFF);
        Cluster &cluster = table_[index(hash)];
        Int32 replaceQuality = std::numeric_limits<Int32>::max();
        USize replaceIdx = 0;
        for (USize i = 0; i < CLUSTER_SIZE; i++) {
            if (!cluster.entries[i].filled || cluster.entries[i].hash16 == hash16) {
                replaceIdx = i;
                break;
            }

            const Int32 entryQuality = quality(static_cast<Int32>(cluster.entries[i].age()), static_cast<Int32>(cluster.entries[i].fdepth));
            if (entryQuality < replaceQuality) {
                replaceQuality = entryQuality;
                replaceIdx = i;
            }
        }

        RawEntry &replace = cluster.entries[replaceIdx];
        if (bound == TTBound::EXACT || replace.hash16 != hash16 || replace.age() != age_ || fdepth + TT_REPLACE_DEPTH_MARGIN + (TT_REPLACE_PV_SCALE * pv) > static_cast<Int32>(replace.fdepth)) {
            if (move != Move::NULL_MOVE || replace.hash16 != hash16) {
                replace.move = move;
            }
            replace.hash16 = hash16;
            replace.score = store(score, ply);
            replace.staticEval = static_cast<Int16>(staticEval);
            replace.fdepth = static_cast<UInt16>(fdepth);
            replace.setBoundPVAge(bound, pv, static_cast<UInt8>(age_));
            replace.filled = 1;
        }
    }

    void prefetch(UInt64 hash) const noexcept { Utils::prefetchPtr(static_cast<const void *>(&table_[index(hash)])); }

    USize hashfull() const noexcept {
        USize count = 0;
        USize sampleSize = std::min(size_, static_cast<USize>(1000));
        for (USize i = 0; i < sampleSize; i++) {
            for (USize j = 0; j < CLUSTER_SIZE; j++) {
                const RawEntry &entry = table_[i].entries[j];
                if (entry.filled && entry.age() == age_) {
                    count++;
                }
            }
        }
        return (count * 1000) / (sampleSize * CLUSTER_SIZE);
    }

    void age() noexcept { age_ = (age_ + 1) & MAX_AGE; }

private:
    static constexpr USize CLUSTER_SIZE = 5;
    static constexpr Int32 MAX_AGE = 31;

    struct RawEntry {
        UInt16 hash16;
        Int16 score;
        Int16 staticEval;
        Move move;
        UInt16 fdepth;
        UInt8 boundPVAge;
        UInt8 filled;

        TTBound bound() const { return static_cast<TTBound>(boundPVAge & 3); }
        bool pv() const { return boundPVAge & 4; }
        UInt8 age() const { return boundPVAge >> 3; }

        void setBoundPVAge(TTBound bound, bool pv, UInt8 age) { boundPVAge = static_cast<UInt8>(bound) | (static_cast<UInt8>(pv << 2) | static_cast<UInt8>(age << 3)); }
    };

    struct alignas(64) Cluster {
        RawEntry entries[CLUSTER_SIZE];
        UInt8 padding[4];
    };

    static_assert(sizeof(Cluster) == 64);

    Cluster *table_;
    USize size_;
    USize capacity_;
    Int32 age_;

    Int32 quality(Int32 age, Int32 fdepth) const noexcept {
        Int32 ageDiff = (MAX_AGE + 1 + age_ - age) & MAX_AGE;
        return TT_QUALITY_FDEPTH_SCALE * fdepth / FDEPTH_SCALE - (TT_QUALITY_AGE_DIFF_SCALE * ageDiff);
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

    USize index(UInt64 hash) const noexcept { return Utils::mulHi64(hash, size_); }

    void allocate(USize newSize) noexcept {
        assert(table_ == nullptr);
        assert(size_ == 0);
        assert(capacity_ == 0);

#if defined(__linux__)
        static constexpr USize PAGE_SIZE = 2 * 1024 * 1024;
#else
        static constexpr USize PAGE_SIZE = 4096;
#endif

        const USize trueNewSize = ((newSize * sizeof(Cluster) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

        table_ = static_cast<Cluster *>(Utils::alignedAlloc(trueNewSize, PAGE_SIZE));

#if defined(__linux__)
        madvise(table_, trueNewSize, MADV_HUGEPAGE);
#endif

        size_ = newSize;
        capacity_ = trueNewSize / sizeof(Cluster);
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
