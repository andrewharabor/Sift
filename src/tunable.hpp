#pragma once

#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "types.hpp"


namespace Sift {

class Tunable {
public:
    using Callback = std::function<void()>;

    Tunable() : name_(), value_(), min_(), max_(), step_(), callback_() {}
    Tunable(const std::string &name, Int32 value, Int32 min, Int32 max, Int32 step, Callback callback);

    constexpr explicit operator std::string() const { return name_ + ", int, " + std::to_string(value_) + ", " + std::to_string(min_) + ", " + std::to_string(max_) + ", " + std::to_string(step_) + ", " + LEARNING_RATE; }

    const std::string &name() const noexcept { return name_; }
    constexpr Int32 &value() noexcept { return value_; }
    constexpr Int32 min() const noexcept { return min_; }
    constexpr Int32 max() const noexcept { return max_; }
    constexpr Int32 step() const noexcept { return step_; }
    Callback callback() const noexcept { return callback_; }

    constexpr void update(Int32 value) noexcept {
        if (value < min_) {
            value = min_;
        } else if (value > max_) {
            value = max_;
        }
        value_ = value;
        callback_();
    }

private:
    static constexpr std::string LEARNING_RATE = "0.002";

    std::string name_;
    Int32 value_;
    Int32 min_;
    Int32 max_;
    Int32 step_;
    Callback callback_;
};

class TunableList {
public:
    constexpr TunableList() noexcept : tunables_() {}

    static void init() noexcept;

    constexpr std::vector<Tunable>::iterator begin() noexcept { return tunables_.begin(); }
    constexpr std::vector<Tunable>::const_iterator begin() const noexcept { return tunables_.begin(); }
    constexpr std::vector<Tunable>::iterator end() noexcept { return tunables_.end(); }
    constexpr std::vector<Tunable>::const_iterator end() const noexcept { return tunables_.end(); }

    void add(const Tunable &tunable) { tunables_.push_back(tunable); }

    void update(const std::string &name, Int32 value) {
        for (Tunable &tunable : tunables_) {
            if (tunable.name() == name) {
                tunable.update(value);
                break;
            }
        }
    }

    void openBenchConfig() const noexcept {
        for (const Tunable &tunable : tunables_) {
            std::cout << std::string(tunable) << std::endl;
        }
    }

private:
    std::vector<Tunable> tunables_;
};

inline TunableList TUNABLES;

inline Tunable::Tunable(const std::string &name, Int32 value, Int32 min, Int32 max, Int32 step, Callback callback) : name_(name), value_(value), min_(min), max_(max), step_(step), callback_(std::move(callback)) { TUNABLES.add(*this); }

#if defined(OPEN_BENCH_TUNE)
#define TUNABLE_CALLBACK(name, val, min, max, step, callback) \
    inline Tunable name##_TUNABLE = Tunable(#name, val, min, max, step, callback); \
    inline Int32 &name = name##_TUNABLE.value();
#else
#define TUNABLE_CALLBACK(name, val, min, max, step, callback) \
    static constexpr Int32 name = val;
#endif

#define TUNABLE(name, val, min, max, step) \
    TUNABLE_CALLBACK(name, val, min, max, step, []() {})


using MVVTable = std::array<Int32, 7>;

static inline MVVTable MVV_PIECE_VALUES;

namespace MVV {

static void init();

}

TUNABLE_CALLBACK(MVV_PAWN_VALUE, 964, 0, 0, 0, []() { MVV::init(); });
TUNABLE_CALLBACK(MVV_KNIGHT_VALUE, 2465, 0, 0, 0, []() { MVV::init(); });
TUNABLE_CALLBACK(MVV_BISHOP_VALUE, 2360, 0, 0, 0, []() { MVV::init(); });
TUNABLE_CALLBACK(MVV_ROOK_VALUE, 4725, 0, 0, 0, []() { MVV::init(); });
TUNABLE_CALLBACK(MVV_QUEEN_VALUE, 7181, 0, 0, 0, []() { MVV::init(); });

static inline void MVV::init() { MVV_PIECE_VALUES = {MVV_PAWN_VALUE, MVV_KNIGHT_VALUE, MVV_BISHOP_VALUE, MVV_ROOK_VALUE, MVV_QUEEN_VALUE, 0, 0}; }

using SEETable = std::array<Int32, 7>;

static inline SEETable SEE_PIECE_VALUES;

namespace SEE {

static void init();

}

TUNABLE_CALLBACK(SEE_PAWN_VALUE, 100, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_KNIGHT_VALUE, 450, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_BISHOP_VALUE, 450, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_ROOK_VALUE, 675, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_QUEEN_VALUE, 1300, 0, 0, 0, []() { SEE::init(); });

static inline void SEE::init() { SEE_PIECE_VALUES = {SEE_PAWN_VALUE, SEE_KNIGHT_VALUE, SEE_BISHOP_VALUE, SEE_ROOK_VALUE, SEE_QUEEN_VALUE, 0, 0}; }

TUNABLE(TIME_BASE_SCALE, 19, 0, 0, 0);
TUNABLE(TIME_INCREMENT_SCALE, 83, 0, 0, 0);
TUNABLE(TIME_SOFT_SCALE, 68, 0, 0, 0);
TUNABLE(TIME_HARD_SCALE, 56, 0, 0, 0);

TUNABLE(TIME_MATE_SCORE_SCALE, 15, 0, 0, 0);
TUNABLE(TIME_WIN_SCORE_SCALE, 60, 0, 0, 0);
TUNABLE(TIME_LOSS_SCORE_SCALE, 50, 0, 0, 0);

TUNABLE(TIME_NODE_SCALE_BASE, 263, 0, 0, 0);
TUNABLE(TIME_NODE_SCALE_COEFF, 170, 0, 0, 0);
TUNABLE(TIME_NODE_SCALE_MIN, 10, 0, 0, 0);

TUNABLE(TIME_STABILITY_SCALE_MIN, 75, 0, 0, 0);
TUNABLE(TIME_STABILITY_SCALE_MAX, 240, 0, 0, 0);
TUNABLE(TIME_STABILITY_SCALE_COEFF, 911, 0, 0, 0);
TUNABLE(TIME_STABILITY_SCALE_OFFSET, 80, 0, 0, 0);
TUNABLE(TIME_STABILITY_SCALE_POWER, -270, 0, 0, 0);

TUNABLE(TIME_SCORE_SCALE_MIN, 60, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_MAX, 170, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_CHANGE_COEFF, 458, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_OFFSET, 80, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_COEFF, 41, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_POS_SCALE, 109, 0, 0, 0);
TUNABLE(TIME_SCORE_SCALE_NEG_SCALE, 104, 0, 0, 0);

TUNABLE(TIME_SCALE_MIN, 7, 0, 0, 0);

TUNABLE(CORR_HIST_MAX, 8091, 0, 0, 0);
TUNABLE(CORR_HIST_UPDATE_MAX, 2009, 0, 0, 0);

TUNABLE(PAWN_CORR_HIST_WEIGHT, 418, 0, 0, 0);
TUNABLE(FRIENDLY_NONPAWN_CORR_HIST_WEIGHT, 387, 0, 0, 0);
TUNABLE(ENEMY_NONPAWN_CORR_HIST_WEIGHT, 273, 0, 0, 0);
TUNABLE(THREAT_CORR_HIST_WEIGHT, 228, 0, 0, 0);
TUNABLE(MINOR_PIECE_CORR_HIST_WEIGHT, 266, 0, 0, 0);
TUNABLE(MAJOR_PIECE_CORR_HIST_WEIGHT, 404, 0, 0, 0);

static constexpr USize MIN_CONT_CORR_HIST_PLY = 2;
static constexpr USize MAX_CONT_CORR_HIST_PLY = 7;

using ContCorrHistWeightsTable = std::array<Int32, MAX_CONT_CORR_HIST_PLY + 1>;

static inline ContCorrHistWeightsTable CONT_CORR_HISTORY_WEIGHTS;

namespace ContCorrHistWeights {

static void init();

}

TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT2, 349, 0, 0, 0, []() { ContCorrHistWeights::init(); });
TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT3, 175, 0, 0, 0, []() { ContCorrHistWeights::init(); });
TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT4, 230, 0, 0, 0, []() { ContCorrHistWeights::init(); });
TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT5, 219, 0, 0, 0, []() { ContCorrHistWeights::init(); });
TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT6, 163, 0, 0, 0, []() { ContCorrHistWeights::init(); });
TUNABLE_CALLBACK(CONT_CORR_HIST_WEIGHT7, 142, 0, 0, 0, []() { ContCorrHistWeights::init(); });

static inline void ContCorrHistWeights::init() { CONT_CORR_HISTORY_WEIGHTS = {0, 0, CONT_CORR_HIST_WEIGHT2, CONT_CORR_HIST_WEIGHT3, CONT_CORR_HIST_WEIGHT4, CONT_CORR_HIST_WEIGHT5, CONT_CORR_HIST_WEIGHT6, CONT_CORR_HIST_WEIGHT7}; }

TUNABLE(HISTORY_BONUS_MAX, 2036, 0, 0, 0);
TUNABLE(HISTORY_BONUS_SCALE, 64, 0, 0, 0);
TUNABLE(HISTORY_BONUS_QUADRATIC, 441, 0, 0, 0);
TUNABLE(HISTORY_BONUS_LINEAR, 219, 0, 0, 0);
TUNABLE(HISTORY_BONUS_OFFSET, 98, 0, 0, 0);

TUNABLE(HISTORY_PENALTY_MAX, 1093, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_SCALE, 64, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_QUADRATIC, 292, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_LINEAR, 302, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_OFFSET, 27, 0, 0, 0);

TUNABLE(NOISY_MOVE_SEE_THRESHOLD_SCALE, 32, 0, 0, 0);

TUNABLE(WINDOW_INIT_DELTA, 10, 0, 0, 0);
TUNABLE(WINDOW_WIDENING_COEFF, 58, 0, 0, 0);
TUNABLE(WINDOW_WIDENING_SCALE, 256, 0, 0, 0);

TUNABLE(HIGH_COMPLEXITY_MARGIN, 87, 0, 0, 0);

TUNABLE(RFP_IMPROVING_MARGIN, 27, 0, 0, 0);
TUNABLE(RFP_NON_IMPROVING_MARGIN, 78, 0, 0, 0);
TUNABLE(RFP_WINNING_THREATS, 21, 0, 0, 0);
TUNABLE(RFP_OPPONENT_WORSENING, 14, 0, 0, 0);
TUNABLE(RFP_HISTORY_DIVISOR, 410, 0, 0, 0);
TUNABLE(RFP_MIN_MARGIN, 20, 0, 0, 0);

TUNABLE(RAZORING_MARGIN, 456, 0, 0, 0);
TUNABLE(RAZORING_MAX_ALPHA, 2000, 0, 0, 0);

TUNABLE(NMP_EVAL_MARGIN, 30, 0, 0, 0);
TUNABLE(NMP_STATIC_EVAL_BASE_MARGIN, 184, 0, 0, 0);
TUNABLE(NMP_STATIC_EVAL_DEPTH_MARGIN, 19, 0, 0, 0);
TUNABLE(NMP_BASE_REDUCTION, 1320, 0, 0, 0);
TUNABLE(NMP_DEPTH_REDUCTION_SCALE, 74, 0, 0, 0);
TUNABLE(NMP_REDUCTION_DIVISOR, 256, 0, 0, 0);
TUNABLE(NMP_EVAL_REDUCTION_SCALE, 215, 0, 0, 0);
TUNABLE(NMP_MAX_EVAL_REDUCTION, 4, 0, 0, 0);

TUNABLE(PROBCUT_BETA_MARGIN, 190, 0, 0, 0);

static constexpr USize LMR_TABLE_SIZE_DEPTH = 64;
static constexpr USize LMR_TABLE_SIZE_MOVES = 64;

using LMRTable = MultiArray<Int32, LMR_TABLE_SIZE_DEPTH, LMR_TABLE_SIZE_MOVES>;

static inline LMRTable LMR_TABLE;

namespace LMR {

static void init();

}

TUNABLE_CALLBACK(LMR_BASE, 775, 0, 0, 0, []() { LMR::init(); })
TUNABLE_CALLBACK(LMR_SCALE, 427, 0, 0, 0, []() { LMR::init(); })

static inline void LMR::init() {
    LMR_TABLE.fill({});
    static constexpr Float64 BASE = static_cast<Float64>(LMR_BASE);
    static constexpr Float64 SCALE = static_cast<Float64>(LMR_SCALE);
    for (USize depth = 1; depth < LMR_TABLE_SIZE_DEPTH; depth++) {
        for (USize moves = 1; moves < LMR_TABLE_SIZE_MOVES; moves++) {
            LMR_TABLE[depth][moves] = static_cast<Int32>(BASE + SCALE * std::log(static_cast<Float64>(depth)) * std::log(static_cast<Float64>(moves)));
        }
    }
}

TUNABLE(LMR_HISTORY_SCALE, 1024, 0, 0, 0);
TUNABLE(LMR_QUIET_HISTORY_DIVISOR, 9043, 0, 0, 0);
TUNABLE(LMR_NOISY_HISTORY_DIVISOR, 6598, 0, 0, 0);
TUNABLE(LMR_NON_IMPROVING_SCALE, 1478, 0, 0, 0);
TUNABLE(LMR_NOISY_HASH_MOVE_SCALE, 1082, 0, 0, 0);
TUNABLE(LMR_TABLE_PV_SCALE, 954, 0, 0, 0);
TUNABLE(LMR_TABLE_PV_NON_FAIL_LOW_SCALE, 484, 0, 0, 0);
TUNABLE(LMR_GIVES_CHECK_SCALE, 573, 0, 0, 0);
TUNABLE(LMR_IN_CHECK_SCALE, 592, 0, 0, 0);
TUNABLE(LMR_HIGH_COMPLEXITY_SCALE, 593, 0, 0, 0);
TUNABLE(LMR_CUTNODE_SCALE, 1612, 0, 0, 0);
TUNABLE(LMR_FAIL_HIGH_COUNT_SCALE, 1042, 0, 0, 0);
TUNABLE(LMR_FAIL_HIGH_COUNT_MARGIN, 2, 0, 0, 0);
TUNABLE(LMR_REDUCTION_DIVISOR, 1024, 0, 0, 0);
TUNABLE(LMR_BASE_DIVISOR, 1024, 0, 0, 0);

TUNABLE(DEEPER_SEARCH_MARGIN_BASE, 38, 0, 0, 0);
TUNABLE(DEEPER_SEARCH_MARGIN_DEPTH_SCALE, 143, 0, 0, 0);
TUNABLE(DEEPER_SEARCH_MARGIN_DEPTH_DIVISOR, 64, 0, 0, 0);
TUNABLE(SHALLOWER_SEARCH_MARGIN, 8, 0, 0, 0);

TUNABLE(FP_BASE_MARGIN, 146, 0, 0, 0);
TUNABLE(FP_DEPTH_SCALE, 128, 0, 0, 0);
TUNABLE(FP_HISTORY_DIVISOR, 393, 0, 0, 0);
TUNABLE(FP_MARGIN_MIN, 20, 0, 0, 0);

TUNABLE(NOISY_FP_BASE_MARGIN, 4, 0, 0, 0);
TUNABLE(NOISY_FP_DEPTH_SCALE, 113, 0, 0, 0);
TUNABLE(NOISY_FP_HISTORY_DIVISOR, 253, 0, 0, 0);
TUNABLE(NOISY_FP_MARGIN_MIN, 20, 0, 0, 0);

TUNABLE(LMP_MARGIN_IMPROVING_BASE, 553, 0, 0, 0);
TUNABLE(LMP_MARGIN_IMPROVING_DEPTH_SCALE, 333, 0, 0, 0);
TUNABLE(LMP_MARGIN_NON_IMPROVING_BASE, 566, 0, 0, 0);
TUNABLE(LMP_MARGIN_NON_IMPROVING_DEPTH_SCALE, 103, 0, 0, 0);
TUNABLE(LMP_MARGIN_DIVISOR, 256, 0, 0, 0);

TUNABLE(SEE_PRUNING_MARGIN_NOISY, -96, 0, 0, 0);
TUNABLE(SEE_PRUNING_MARGIN_QUIET, -67, 0, 0, 0);
TUNABLE(SEE_CAPT_HISTORY_DEPTH_SCALE, 103, 0, 0, 0);
TUNABLE(SEE_CAPT_HISTORY_DIVISOR, 30, 0, 0, 0);

TUNABLE(HISTORY_PRUNING_MARGIN, -1743, 0, 0, 0);
TUNABLE(HISTORY_BETA_MARGIN, 39, 0, 0, 0);

TUNABLE(SE_BETA_SCALE, 52, 0, 0, 0);
TUNABLE(SE_BETA_SCALE_PV, 21, 0, 0, 0);
TUNABLE(SE_BETA_DEPTH_DIVISOR, 64, 0, 0, 0);
TUNABLE(SE_DOUBLE_EXT_MARGIN, 10, 0, 0, 0);
TUNABLE(SE_TRIPLE_EXT_MARGIN, 124, 0, 0, 0);

TUNABLE(QSEARCH_FP_MARGIN, 78, 0, 0, 0);

TUNABLE(EVAL_ADJUST_PAWN_SCALE, 48, 0, 0, 0);
TUNABLE(EVAL_ADJUST_KNIGHT_SCALE, 435, 0, 0, 0);
TUNABLE(EVAL_ADJUST_BISHOP_SCALE, 455, 0, 0, 0);
TUNABLE(EVAL_ADJUST_ROOK_SCALE, 642, 0, 0, 0);
TUNABLE(EVAL_ADJUST_QUEEN_SCALE, 1217, 0, 0, 0);
TUNABLE(EVAL_ADJUST_MATERIAL_BASE, 26000, 0, 0, 0);
TUNABLE(EVAL_ADJUST_MATERIAL_DIVISOR, 32768, 0, 0, 0);
TUNABLE(EVAL_ADJUST_HALF_MOVE_SCALE, 200, 0, 0, 0);

inline void TunableList::init() noexcept {
    MVV::init();
    SEE::init();
    ContCorrHistWeights::init();
    LMR::init();
}

}
