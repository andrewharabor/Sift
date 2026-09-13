#pragma once

#include "arch.hpp"
#include "features.hpp"
#include "network.hpp"
#include "types.hpp"


namespace Sift {

namespace NetConfig {

constexpr USize L1_SIZE = 1024;
constexpr USize L2_SIZE = 32;
constexpr USize L3_SIZE = 64;

constexpr Int32 FT_SCALE_BITS = 7;
constexpr Int32 FT_QUANT_BITS = 8;
constexpr Int32 L1_QUANT_BITS = 7;

constexpr Int32 SCALE = 400;

constexpr bool DUAL_ACTIVATION = true;
constexpr bool SKIP_L2 = true;

using PSQFeatureSet = MergedMirroredKingBucketInputs<
    MirroredKingSide::ABCD,
    0, 1, 2, 3,
    4, 5, 6, 7,
    8, 9, 10, 11,
    8, 9, 10, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15
>;

using InputFeatureSet = PawnPawnThreatInputs<PSQFeatureSet>;

using Updates = InputFeatureSet::Updates;

using FeatureTransformer = Sift::FeatureTransformer<L1_SIZE, InputFeatureSet>;
using PSQAccumulator = FeatureTransformer::PSQAccumulator;
using RefreshTable = FeatureTransformer::RefreshTable;

using Output = MaterialCountBucketOutput<8>;

using Arch = PairwiseMultilayerArch<
    InputFeatureSet,
    L1_SIZE,
    L2_SIZE,
    L3_SIZE,
    FT_SCALE_BITS,
    FT_QUANT_BITS,
    L1_QUANT_BITS,
    DUAL_ACTIVATION,
    SKIP_L2,
    Output,
    SCALE
>;

}

using Network = PerspectiveNetwork<NetConfig::FeatureTransformer, NetConfig::Output, NetConfig::Arch>;

};
