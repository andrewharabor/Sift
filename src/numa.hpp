#pragma once

#include <cassert>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(USE_NUMA)
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#endif

#include "types.hpp"


namespace Sift {

#if defined(USE_NUMA)

class NUMA {
public:
    static inline void init() {
        assert(numa_available() >= 0);
        threadMapping();
    }

    static inline USize nodeCount() noexcept { return static_cast<USize>(threadMapping().size()); }

    static inline USize getNode(USize threadID) noexcept { return static_cast<USize>(threadID % nodeCount()); }

    static inline void bindThread(USize threadID) noexcept {
        const auto node = getNode(threadID);
        const auto handle = pthread_self();
        const auto *cpuSet = &threadMapping()[node];
        pthread_setaffinity_np(handle, sizeof(cpu_set_t), cpuSet);
    }

private:
    static inline std::span<const cpu_set_t> threadMapping() {
        static const auto mapping = [] {
            const auto numNodes = numa_max_node() + 1;
            std::vector<cpu_set_t> masks = {};
            masks.reserve(numNodes);

            for (int node = 0; node < numNodes; node++) {
                auto *cpuMask = numa_allocate_cpumask();

                assert(numa_node_to_cpus(node, cpuMask) == 0);

                cpu_set_t cpuSet;
                CPU_ZERO(&cpuSet);
                for (int cpu = 0; cpu < cpuMask->size; cpu++) {
                    if (numa_bitmask_isbitset(cpuMask, cpu)) {
                        CPU_SET(cpu, &cpuSet);
                    }
                }

                numa_free_cpumask(cpuMask);
                masks.push_back(cpuSet);
            }

            assert(masks.size() >= 1);

            return masks;
        }();

        return mapping;
    }
};

template<typename Type>
class NUMAUniqueAllocation {
public:
    NUMAUniqueAllocation() : data_() {
        const auto count = NUMA::nodeCount();
        data_.reserve(count);
        for (USize node = 0; node < count; node++) {
            auto *storage = numa_alloc_onnode(sizeof(Type), node);
            auto *obj = new (storage) Type();
            data_.push_back(obj);
        }
    }

    ~NUMAUniqueAllocation() {
        for (auto *obj : data_) {
            obj->~Type();
            numa_free(obj, sizeof(Type));
        }
        data_.clear();
    }

    Type *get(USize threadID) noexcept { return data_[NUMA::getNode(threadID)]; }
    const Type *get(USize threadID) const noexcept { return data_[NUMA::getNode(threadID)]; }

    Type *getForNode(USize nodeID) noexcept { return data_[nodeID]; }
    const Type *getForNode(USize nodeID) const noexcept { return data_[nodeID]; }

private:
    std::vector<Type *> data_;
};

#else

class NUMA {
public:
    static inline void init() noexcept {}

    static inline USize nodeCount() noexcept { return 1; }

    static inline USize getNode(USize) noexcept { return 0; }

    static inline void bindThread(USize) noexcept {}
};

template<typename Type>
class NUMAUniqueAllocation {
public:
    NUMAUniqueAllocation() : data_() {
        data_.reserve(1);
        data_.push_back(new Type());
    }

    ~NUMAUniqueAllocation() {
        for (auto *obj : data_) {
            delete obj;
        }
        data_.clear();
    }

    Type *get(USize threadID) noexcept { return data_[NUMA::getNode(threadID)]; }
    const Type *get(USize threadID) const noexcept { return data_[NUMA::getNode(threadID)]; }

    Type *getForNode(USize nodeID) noexcept { return data_[nodeID]; }
    const Type *getForNode(USize nodeID) const noexcept { return data_[nodeID]; }

private:
    std::vector<Type *> data_;
};

#endif

};
