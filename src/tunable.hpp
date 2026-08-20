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


static constexpr UInt8 TTABLE_REPLACE_DEPTH_MARGIN = 2;
static constexpr UInt8 TTABLE_REPLACE_DEPTH_PV_SCALE = 2;
static constexpr Int32 TTABLE_QUALITY_AGE_DIFF_SCALE = 2;

static constexpr Int32 FDEPTH_SCALE = 128;

static constexpr Int32 MOVE_ORDER_GOOD_NOISY_SCORE_DIVISOR = 4;
static constexpr Int32 MOVE_ORDER_NOISY_SCORE_DIVISOR = 8;

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

TUNABLE_CALLBACK(SEE_PAWN_VALUE, 97, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_KNIGHT_VALUE, 434, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_BISHOP_VALUE, 464, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_ROOK_VALUE, 646, 0, 0, 0, []() { SEE::init(); });
TUNABLE_CALLBACK(SEE_QUEEN_VALUE, 1289, 0, 0, 0, []() { SEE::init(); });

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

TUNABLE(HISTORY_BONUS_DEPTH_SCALE, 339, 0, 0, 0);
TUNABLE(HISTORY_BONUS_OFFSET, 313, 0, 0, 0);
TUNABLE(HISTORY_BONUS_MAX, 2663, 0, 0, 0);

TUNABLE(HISTORY_PENALTY_DEPTH_SCALE, 384, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_OFFSET, 177, 0, 0, 0);
TUNABLE(HISTORY_PENALTY_MAX, 1318, 0, 0, 0);

TUNABLE(MAIN_HIST_WEIGHT, 481, 0, 0, 0);
TUNABLE(PAWN_HIST_WEIGHT, 469, 0, 0, 0);
TUNABLE(CONT1_HIST_WEIGHT, 1082, 0, 0, 0);
TUNABLE(CONT2_HIST_WEIGHT, 1092, 0, 0, 0);
TUNABLE(CONT4_HIST_WEIGHT, 689, 0, 0, 0);
TUNABLE(CONT6_HIST_WEIGHT, 292, 0, 0, 0);
TUNABLE(CAPTURE_HIST_WEIGHT, 1024, 0, 0, 0);

TUNABLE(MAIN_HIST_UPDATE_WEIGHT, 975, 0, 0, 0);
TUNABLE(PAWN_HIST_UPDATE_WEIGHT, 891, 0, 0, 0);
TUNABLE(CONT1_HIST_UPDATE_WEIGHT, 1084, 0, 0, 0);
TUNABLE(CONT2_HIST_UPDATE_WEIGHT, 1142, 0, 0, 0);
TUNABLE(CONT4_HIST_UPDATE_WEIGHT, 952, 0, 0, 0);
TUNABLE(CONT6_HIST_UPDATE_WEIGHT, 1003, 0, 0, 0);
TUNABLE(CAPTURE_HIST_UPDATE_WEIGHT, 1024, 0, 0, 0);

TUNABLE(CONT_HIST_BASE_MAIN_HIST_WEIGHT, 211, 0, 0, 0);
TUNABLE(CONT_HIST_BASE_PAWN_HIST_WEIGHT, 101, 0, 0, 0);
TUNABLE(CONT_HIST_BASE_CONT1_HIST_WEIGHT, 929, 0, 0, 0);
TUNABLE(CONT_HIST_BASE_CONT2_HIST_WEIGHT, 917, 0, 0, 0);
TUNABLE(CONT_HIST_BASE_CONT4_HIST_WEIGHT, 553, 0, 0, 0);
TUNABLE(CONT_HIST_BASE_CONT6_HIST_WEIGHT, 320, 0, 0, 0);

TUNABLE(CORR_HIST_BONUS_FDEPTH_DIVISOR, 1024, 0, 0, 0);
TUNABLE(CORR_HIST_BONUS_MAX, 256, 0, 0, 0);
TUNABLE(CORR_HIST_PENALTY_MAX, 256, 0, 0, 0);

TUNABLE(PAWN_CORR_HIST_WEIGHT, 52, 0, 0, 0);
TUNABLE(FRIENDLY_NONPAWN_CORR_HIST_WEIGHT, 48, 0, 0, 0);
TUNABLE(ENEMY_NONPAWN_CORR_HIST_WEIGHT, 34, 0, 0, 0);
TUNABLE(MINOR_PIECE_CORR_HIST_WEIGHT, 33, 0, 0, 0);
TUNABLE(MAJOR_PIECE_CORR_HIST_WEIGHT, 51, 0, 0, 0);

TUNABLE(CONT1_CORR_HIST_WEIGHT, 71, 0, 0, 0);
TUNABLE(CONT2_CORR_HIST_WEIGHT, 66, 0, 0, 0);
TUNABLE(CONT4_CORR_HIST_WEIGHT, 44, 0, 0, 0);
TUNABLE(CONT6_CORR_HIST_WEIGHT, 30, 0, 0, 0);

TUNABLE(NOISY_MOVE_SEE_THRESHOLD_SCALE, 32, 0, 0, 0);

static constexpr Int32 WINDOW_MIN_DEPTH = 6;

TUNABLE(WINDOW_FREDUCTION, 128, 0, 0, 0);
TUNABLE(WINDOW_MAX_FREDUCTION, 640, 0, 0, 0);
TUNABLE(WINDOW_MIN_FDEPTH, 128, 0, 0, 0);
TUNABLE(WINDOW_INIT_DELTA, 10, 0, 0, 0);
TUNABLE(WINDOW_WIDENING_COEFF, 58, 0, 0, 0);

TUNABLE(HIGH_COMPLEXITY_MARGIN, 87, 0, 0, 0);

TUNABLE(RFP_MAX_FDEPTH, 1024, 0, 0, 0);
TUNABLE(RFP_IMPROVING_MARGIN, 27, 0, 0, 0);
TUNABLE(RFP_NON_IMPROVING_MARGIN, 78, 0, 0, 0);
TUNABLE(RFP_WINNING_THREATS, 21, 0, 0, 0);
TUNABLE(RFP_OPPONENT_WORSENING, 14, 0, 0, 0);
TUNABLE(RFP_HISTORY_DIVISOR, 410, 0, 0, 0);
TUNABLE(RFP_MIN_MARGIN, 20, 0, 0, 0);

TUNABLE(RAZORING_MAX_FDEPTH, 384, 0, 0, 0);
TUNABLE(RAZORING_MARGIN, 456, 0, 0, 0);
TUNABLE(RAZORING_MAX_ALPHA, 2000, 0, 0, 0);

static constexpr Int32 NMP_MAX_EVAL_REDUCTION = 4;

TUNABLE(NMP_MIN_FDEPTH, 256, 0, 0, 0);
TUNABLE(NMP_NO_VERIFICATION_MAX_FDEPTH, 1920, 0, 0, 0);
TUNABLE(NMP_MIN_PLY_FDEPTH_SCALE, 768, 0, 0, 0);
TUNABLE(NMP_EVAL_MARGIN, 30, 0, 0, 0);
TUNABLE(NMP_STATIC_EVAL_BASE_MARGIN, 184, 0, 0, 0);
TUNABLE(NMP_STATIC_EVAL_DEPTH_MARGIN, 19, 0, 0, 0);
TUNABLE(NMP_BASE_REDUCTION, 660, 0, 0, 0);
TUNABLE(NMP_DEPTH_REDUCTION_SCALE, 37, 0, 0, 0);
TUNABLE(NMP_EVAL_REDUCTION_SCALE, 215, 0, 0, 0);

TUNABLE(PROBCUT_MIN_FDEPTH, 640, 0, 0, 0);
TUNABLE(PROBCUT_TTABLE_FDEPTH_MARGIN, 384, 0, 0, 0);
TUNABLE(PROBCUT_FREDUCTION, 512, 0, 0, 0);
TUNABLE(PROBCUT_BETA_MARGIN, 190, 0, 0, 0);

TUNABLE(IIR_MIN_FDEPTH, 512, 0, 0, 0);
TUNABLE(IIR_TTABLE_FDEPTH_MARGIN, 640, 0, 0, 0);
TUNABLE(IIR_FREDUCTION, 128, 0, 0, 0);

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
    const Float64 base = static_cast<Float64>(LMR_BASE);
    const Float64 scale = static_cast<Float64>(LMR_SCALE);
    for (USize depth = 1; depth < LMR_TABLE_SIZE_DEPTH; depth++) {
        for (USize moves = 1; moves < LMR_TABLE_SIZE_MOVES; moves++) {
            LMR_TABLE[depth][moves] = static_cast<Int32>(base + scale * std::log(static_cast<Float64>(depth)) * std::log(static_cast<Float64>(moves)));
        }
    }
}

static constexpr Int32 LMR_MIN_MOVES_PV = 4;
static constexpr Int32 LMR_MIN_MOVES_NON_PV = 3;

TUNABLE(LMR_MIN_FDEPTH, 384, 0, 0, 0);
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

TUNABLE(FP_MAX_FDEPTH, 1024, 0, 0, 0);
TUNABLE(FP_BASE_MARGIN, 146, 0, 0, 0);
TUNABLE(FP_DEPTH_SCALE, 128, 0, 0, 0);
TUNABLE(FP_HISTORY_DIVISOR, 393, 0, 0, 0);
TUNABLE(FP_MARGIN_MIN, 20, 0, 0, 0);

TUNABLE(NOISY_FP_MAX_FDEPTH, 640, 0, 0, 0);
TUNABLE(NOISY_FP_BASE_MARGIN, 4, 0, 0, 0);
TUNABLE(NOISY_FP_DEPTH_SCALE, 113, 0, 0, 0);
TUNABLE(NOISY_FP_HISTORY_DIVISOR, 253, 0, 0, 0);
TUNABLE(NOISY_FP_MARGIN_MIN, 20, 0, 0, 0);

TUNABLE(LMP_MARGIN_IMPROVING_BASE, 553, 0, 0, 0);
TUNABLE(LMP_MARGIN_IMPROVING_DEPTH_SCALE, 333, 0, 0, 0);
TUNABLE(LMP_MARGIN_NON_IMPROVING_BASE, 566, 0, 0, 0);
TUNABLE(LMP_MARGIN_NON_IMPROVING_DEPTH_SCALE, 103, 0, 0, 0);

TUNABLE(SEE_PRUNING_MARGIN_NOISY, -96, 0, 0, 0);
TUNABLE(SEE_PRUNING_MARGIN_QUIET, -67, 0, 0, 0);
TUNABLE(SEE_CAPT_HIST_DEPTH_SCALE, 103, 0, 0, 0);
TUNABLE(SEE_CAPT_HIST_DIVISOR, 30, 0, 0, 0);

TUNABLE(HISTORY_PRUNING_MAX_FDEPTH, 896, 0, 0, 0);
TUNABLE(HISTORY_PRUNING_MARGIN, -1743, 0, 0, 0);
TUNABLE(HISTORY_BETA_MARGIN, 39, 0, 0, 0);

static constexpr Int32 SE_ROOT_DEPTH_SCALE = 2;

TUNABLE(SE_MIN_FDEPTH, 640, 0, 0, 0);
TUNABLE(SE_TABLE_FDEPTH_MARGIN, 384, 0, 0, 0);
TUNABLE(SE_BETA_SCALE, 52, 0, 0, 0);
TUNABLE(SE_BETA_SCALE_PV, 21, 0, 0, 0);
TUNABLE(SE_BASE_DOUBLE_FEXTENSTION, 256, 0, 0, 0);
TUNABLE(SE_BASE_DOUBLE_NEG_FEXTENSION, 256, 0, 0, 0);
TUNABLE(SE_SINGLE_FEXTENSION, 128, 0, 0, 0);
TUNABLE(SE_SINGLE_NEG_FEXTENSION, 128, 0, 0, 0);
TUNABLE(SE_DOUBLE_EXT_MARGIN, 10, 0, 0, 0);
TUNABLE(SE_TRIPLE_EXT_MARGIN, 124, 0, 0, 0);

TUNABLE(CHECK_FEXTENSION, 128, 0, 0, 0);

TUNABLE(DEEPER_SEARCH_MARGIN_BASE, 38, 0, 0, 0);
TUNABLE(DEEPER_SEARCH_MARGIN_DEPTH_SCALE, 143, 0, 0, 0);
TUNABLE(DEEPER_SEARCH_FEXTENSION, 128, 0, 0, 0);
TUNABLE(SHALLOWER_SEARCH_MARGIN, 8, 0, 0, 0);
TUNABLE(SHALLOWER_SEARCH_FREDUCTION, 128, 0, 0, 0);

TUNABLE(QSEARCH_FP_MARGIN, 78, 0, 0, 0);

static constexpr Int32 QSEARCH_MAX_MOVES = 2;

TUNABLE(MOVE_ORDER_GOOD_NOISY_SEE_OFFSET, 72, 0, 0, 0);
TUNABLE(MOVE_ORDER_DIRECT_CHECK_BONUS, 5257, 0, 0, 0);
TUNABLE(MOVE_ORDER_DIRECT_CHECK_SEE_MARGIN, -37, 0, 0, 0);

TUNABLE(EVAL_ADJUST_PAWN_SCALE, 48, 0, 0, 0);
TUNABLE(EVAL_ADJUST_KNIGHT_SCALE, 435, 0, 0, 0);
TUNABLE(EVAL_ADJUST_BISHOP_SCALE, 455, 0, 0, 0);
TUNABLE(EVAL_ADJUST_ROOK_SCALE, 642, 0, 0, 0);
TUNABLE(EVAL_ADJUST_QUEEN_SCALE, 1217, 0, 0, 0);
TUNABLE(EVAL_ADJUST_MATERIAL_BASE, 26000, 0, 0, 0);
TUNABLE(EVAL_ADJUST_DIVISOR, 32768, 0, 0, 0);
TUNABLE(EVAL_ADJUST_HALF_MOVE_SCALE, 200, 0, 0, 0);

TUNABLE(THREAD_WEIGHT_SCORE_OFFSET, 11, 0, 0, 0);

inline void TunableList::init() noexcept {
    MVV::init();
    SEE::init();
    LMR::init();
}

}
