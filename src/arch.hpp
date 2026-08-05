#pragma once

#include <array>

#include "types.hpp"


namespace Sift {

namespace Arch {

static constexpr USize PSQ_SIZE = 704;
static constexpr USize TI_SIZE = 60144;
static constexpr USize L1_SIZE = 1280;
static constexpr USize L2_SIZE = 16;
static constexpr USize L3_SIZE = 32;

static constexpr USize KING_BUCKETS = 16;
static constexpr std::array<USize, 32> KING_BUCKET_LAYOUT = {
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  8,  9,  9,
    10, 10, 11, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15,
};

static constexpr USize OUTPUT_BUCKETS = 8;

static constexpr Int32 SCALE = 279;
static constexpr Int32 QUANT_A = 255;
static constexpr Int32 QUANT_B = 128;
static constexpr Int32 QUANT_C = 64;
static constexpr Int32 SHIFT = 8;

}

enum class NetPerm : UInt8 {
    DEFAULT,
    AVX2,
    AVX512,
};

struct NetParams {
    alignas(64) MultiArray<Int16, Arch::KING_BUCKETS, Arch::PSQ_SIZE, Arch::L1_SIZE> ftWeights;
    alignas(64) std::array<Int16, Arch::L1_SIZE> ftBiases;
    alignas(64) MultiArray<Int8, Arch::OUTPUT_BUCKETS, Arch::L1_SIZE / 4, Arch::L2_SIZE * 4> l1Weights;
    alignas(64) MultiArray<Int32, Arch::OUTPUT_BUCKETS, Arch::L2_SIZE> l1Biases;
    alignas(64) MultiArray<Int32, Arch::OUTPUT_BUCKETS, Arch::L2_SIZE, Arch::L3_SIZE> l2Weights;
    alignas(64) MultiArray<Int32, Arch::OUTPUT_BUCKETS, Arch::L3_SIZE> l2Biases;
    alignas(64) MultiArray<Int32, Arch::OUTPUT_BUCKETS, Arch::L3_SIZE> l3Weights;
    alignas(64) MultiArray<Int32, Arch::OUTPUT_BUCKETS> l3Biases;

    NetPerm perm;
    bool sparsityPerm;
};

}
