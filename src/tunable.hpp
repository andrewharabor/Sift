#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "types.hpp"
#include "utils.hpp"

#if defined(EXTERNAL_TUNE)
    #define TUNABLE_CALLBACK(name, val, min, max, callback) \
        inline Tunable name##_TUNABLE = Tunable(#name, val, min, max, callback); \
        inline Int32& name = name##_TUNABLE.value();
#else
    #define TUNABLE_CALLBACK(name, val, min, max, callback) static constexpr Int32 name = val;
#endif

#define TUNABLE(name, val, min, max) TUNABLE_CALLBACK(name, val, min, max, []() {})

namespace Sift {
    class Tunable {
    public:
        using Callback = std::function<void()>;

        Tunable() : name_(), value_(), min_(), max_(), callback_() {}

        Tunable(const std::string& name, Int32 value, Int32 min, Int32 max, Callback callback);

        constexpr operator Int32() const noexcept { return value_; }

        const std::string& name() const noexcept { return name_; }

        constexpr Int32& value() noexcept { return value_; }

        constexpr Int32 value() const noexcept { return value_; }

        constexpr Int32 min() const noexcept { return min_; }

        constexpr Int32 max() const noexcept { return max_; }

        constexpr Float64 step() const noexcept { return std::abs(max_ - min_) / 20.0; }

        constexpr Float64 learningRate() const noexcept { return 0.002; }

        Callback callback() const noexcept { return callback_; }

        constexpr void update(Int32 value) noexcept {
            value_ = std::clamp(value, min_, max_);
            callback_();
        }

    private:
        std::string name_;
        Int32 value_;
        Int32 min_;
        Int32 max_;
        Callback callback_;
    };

    class TunableList {
    public:
        constexpr TunableList() noexcept : tunables_() {}

        static void init() noexcept;

        constexpr std::vector<Tunable*>::iterator begin() noexcept { return tunables_.begin(); }

        constexpr std::vector<Tunable*>::const_iterator begin() const noexcept { return tunables_.begin(); }

        constexpr std::vector<Tunable*>::iterator end() noexcept { return tunables_.end(); }

        constexpr std::vector<Tunable*>::const_iterator end() const noexcept { return tunables_.end(); }

        void add(Tunable& tunable) { tunables_.push_back(&tunable); }

        void update(const std::string& name, Int32 value) {
            for (Tunable* tunable : tunables_) {
                if (tunable->name() == name) {
                    tunable->update(value);
                    break;
                }
            }
        }

        void openBenchConfig() const noexcept {
            for (const Tunable* tunable : tunables_) {
                std::cout << tunable->name() + ", int, " + std::to_string(tunable->value()) + ", " + std::to_string(tunable->min()) + ", " +
                                 std::to_string(tunable->max()) + ", " + std::to_string(tunable->step()) + ", " +
                                 std::to_string(tunable->learningRate())
                          << std::endl;
            }
        }

        void weatherFactoryConfig() const noexcept {
            std::cout << "{" << std::endl;
            bool first = true;
            for (const Tunable* tunable : tunables_) {
                if (!first) { std::cout << "," << std::endl; }

                first = false;

                std::cout << "  \"" << tunable->name() << "\": {" << std::endl;
                std::cout << "    \"value\": " << tunable->value() << "," << std::endl;
                std::cout << "    \"min_value\": " << tunable->min() << "," << std::endl;
                std::cout << "    \"max_value\": " << tunable->max() << "," << std::endl;
                std::cout << "    \"step\": " << tunable->step() << std::endl;
                std::cout << "  }";
            }

            std::cout << std::endl;
            std::cout << "}" << std::endl;
        }

    private:
        std::vector<Tunable*> tunables_;
    };

    inline TunableList TUNABLES;

    inline Tunable::Tunable(const std::string& name, Int32 value, Int32 min, Int32 max, Callback callback) :
        name_(name), value_(value), min_(min), max_(max), callback_(std::move(callback)) {
        TUNABLES.add(*this);
    }

    static constexpr Int32 FDEPTH_SCALE = 128;

    TUNABLE(TT_REPLACE_DEPTH_OFFSET, 601, 0, 1024);
    TUNABLE(TT_REPLACE_PV_SCALE, 234, 0, 768);
    TUNABLE(TT_QUALITY_DEPTH_SCALE, 133, 0, 512);
    TUNABLE(TT_QUALITY_AGE_DIFF_SCALE, 208, 0, 512);

    static inline std::array<Int32, 7> SEE_PIECE_VALUES;

    namespace SEE { static void init(); }

    TUNABLE_CALLBACK(SEE_PAWN_VALUE, 89, 50, 200, []() { SEE::init(); });
    TUNABLE_CALLBACK(SEE_KNIGHT_VALUE, 447, 300, 700, []() { SEE::init(); });
    TUNABLE_CALLBACK(SEE_BISHOP_VALUE, 481, 300, 700, []() { SEE::init(); });
    TUNABLE_CALLBACK(SEE_ROOK_VALUE, 690, 400, 1000, []() { SEE::init(); });
    TUNABLE_CALLBACK(SEE_QUEEN_VALUE, 1215, 800, 1600, []() { SEE::init(); });

    static inline void SEE::init() {
        SEE_PIECE_VALUES = {SEE_PAWN_VALUE, SEE_KNIGHT_VALUE, SEE_BISHOP_VALUE, SEE_ROOK_VALUE, SEE_QUEEN_VALUE, 0, 0};
    }

    TUNABLE(TIME_BASE_TIME_SCALE, 16, 10, 40);
    TUNABLE(TIME_BASE_TIME_INCREMENT_SCALE, 87, 50, 150);

    TUNABLE(TIME_SOFT_BOUND_SCALE, 67, 50, 100);
    TUNABLE(TIME_HARD_BOUND_SCALE, 56, 20, 100);

    TUNABLE(TIME_SCALE_MATE_SCORE, 21, 1, 100);
    TUNABLE(TIME_SCALE_DECISIVE_SCORE, 54, 1, 100);

    TUNABLE(TIME_SCALE_NODE_BASE, 248, 150, 300);
    TUNABLE(TIME_SCALE_NODE_SCALE, 179, 100, 250);
    TUNABLE(TIME_SCALE_NODE_MIN, 9, 1, 100);

    TUNABLE(TIME_SCALE_STABILITY_BASE, 72, 40, 100);
    TUNABLE(TIME_SCALE_STABILITY_MAX, 273, 120, 1000);
    TUNABLE(TIME_SCALE_STABILITY_SCALE, 876, 200, 1500);
    TUNABLE(TIME_SCALE_STABILITY_OFFSET, 77, 50, 200);
    TUNABLE(TIME_SCALE_STABILITY_POWER, -259, -400, -150);

    TUNABLE(TIME_SCALE_SCORE_CHANGE_DIVISOR, 355, 10, 1000);
    TUNABLE(TIME_SCALE_POS_SCORE_SCALE, 125, 50, 200);
    TUNABLE(TIME_SCALE_NEG_SCORE_SCALE, 112, 50, 200);

    TUNABLE(TIME_SCALE_SCORE_OFFSET, 75, 10, 200);
    TUNABLE(TIME_SCALE_SCORE_SCALE, 48, 10, 90);
    TUNABLE(TIME_SCALE_SCORE_MIN, 68, 40, 100);
    TUNABLE(TIME_SCALE_SCORE_MAX, 204, 120, 1000);

    TUNABLE(TIME_SCALE_MIN, 7, 1, 100);

    static constexpr Int32 TIME_SCALE_STABILITY_MIN_DEPTH = 6;

    TUNABLE(EVAL_ADJUST_PAWN_SCALE, 40, 20, 200);
    TUNABLE(EVAL_ADJUST_KNIGHT_SCALE, 427, 300, 700);
    TUNABLE(EVAL_ADJUST_BISHOP_SCALE, 419, 300, 700);
    TUNABLE(EVAL_ADJUST_ROOK_SCALE, 639, 400, 1000);
    TUNABLE(EVAL_ADJUST_QUEEN_SCALE, 1139, 800, 1600);
    TUNABLE(EVAL_ADJUST_MATERIAL_BASE, 24546, 10000, 40000);
    TUNABLE(EVAL_ADJUST_OPTIMISM_SCALE_BASE, 1697, 0, 12000);
    TUNABLE(EVAL_ADJUST_OPTIMISM_SCALE_MATERIAL_SCALE, 748, 0, 2048);
    TUNABLE(EVAL_ADJUST_HALF_MOVE_SCALE, 155, 101, 400);

    TUNABLE(THREAD_WEIGHT_SCORE_OFFSET, 12, 0, 20);

    TUNABLE(MOVE_ORDER_GOOD_NOISY_MARGIN_SCORE_SCALE, 343, 0, 1024);
    TUNABLE(MOVE_ORDER_GOOD_NOISY_MARGIN_OFFSET, -5, -384, 384);

    TUNABLE(MOVE_ORDER_NOISY_SCORE_SCALE, 89, 0, 512);

    TUNABLE(MOVE_ORDER_DIRECT_CHECK_BONUS, 6116, 2048, 16384);
    TUNABLE(MOVE_ORDER_DIRECT_CHECK_MARGIN, -85, -300, 150);

    TUNABLE(TT_CUTOFF_MAX_HALFMOVES, 91, 75, 99);

    TUNABLE(TT_CUTOFF_BONUS_DEPTH_SCALE, 243, 128, 512);
    TUNABLE(TT_CUTOFF_BONUS_OFFSET, 435, 0, 768);
    TUNABLE(TT_CUTOFF_BONUS_MAX, 2920, 1024, 4096);

    TUNABLE(EVAL_POLICY_BONUS_BASE, 710, 0, 2048);
    TUNABLE(EVAL_POLICY_BONUS_GAIN_SCALE, 9, 0, 20);
    TUNABLE(EVAL_POLICY_BONUS_MIN, -1159, -4096, 0);
    TUNABLE(EVAL_POLICY_BONUS_MAX, 2685, 0, 4096);

    TUNABLE(POST_LMR_BONUS_DEPTH_SCALE, 266, 128, 512);
    TUNABLE(POST_LMR_BONUS_OFFSET, 622, 0, 768);
    TUNABLE(POST_LMR_BONUS_MAX, 2989, 1024, 4096);

    TUNABLE(POST_LMR_PENALTY_DEPTH_SCALE, 335, 64, 512);
    TUNABLE(POST_LMR_PENALTY_OFFSET, 234, 0, 768);
    TUNABLE(POST_LMR_PENALTY_MAX, 1164, 768, 4096);

    TUNABLE(QUIET_BONUS_DEPTH_SCALE, 379, 128, 512);
    TUNABLE(QUIET_BONUS_OFFSET, 250, 0, 768);
    TUNABLE(QUIET_BONUS_MAX, 2425, 1024, 4096);

    TUNABLE(QUIET_PENALTY_DEPTH_SCALE, 411, 128, 512);
    TUNABLE(QUIET_PENALTY_OFFSET, 162, 0, 768);
    TUNABLE(QUIET_PENALTY_MAX, 1173, 1024, 4096);

    TUNABLE(NOISY_BONUS_DEPTH_SCALE, 285, 128, 512);
    TUNABLE(NOISY_BONUS_OFFSET, 490, 0, 768);
    TUNABLE(NOISY_BONUS_MAX, 2419, 1024, 4096);

    TUNABLE(QUIET_MOVE_NOISY_PENALTY_DEPTH_SCALE, 435, 128, 512);
    TUNABLE(QUIET_MOVE_NOISY_PENALTY_OFFSET, 71, 0, 768);
    TUNABLE(QUIET_MOVE_NOISY_PENALTY_MAX, 1798, 1024, 4096);

    TUNABLE(NOISY_MOVE_NOISY_PENALTY_DEPTH_SCALE, 324, 128, 512);
    TUNABLE(NOISY_MOVE_NOISY_PENALTY_OFFSET, 165, 0, 768);
    TUNABLE(NOISY_MOVE_NOISY_PENALTY_MAX, 1374, 1024, 4096);

    TUNABLE(PCM_QUIET_BONUS_DEPTH_SCALE, 154, 64, 512);
    TUNABLE(PCM_QUIET_BONUS_OFFSET, 49, 0, 768);
    TUNABLE(PCM_QUIET_BONUS_MAX, 1303, 768, 4096);

    TUNABLE(PCM_NOISY_BONUS, 122, 0, 2048);

    TUNABLE(HISTORY_FDEPTH_STATIC_EVAL_SCALE, 146, 0, 512);

    TUNABLE(MAIN_HIST_WEIGHT, 698, 0, 4096);
    TUNABLE(PAWN_HIST_WEIGHT, 780, 0, 4096);
    TUNABLE(CONT1_HIST_WEIGHT, 1067, 0, 4096);
    TUNABLE(CONT2_HIST_WEIGHT, 1019, 0, 4096);
    TUNABLE(CONT4_HIST_WEIGHT, 937, 0, 4096);
    TUNABLE(CONT6_HIST_WEIGHT, 282, 0, 4096);
    TUNABLE(CAPTURE_HIST_WEIGHT, 1029, 0, 4096);

    TUNABLE(MAIN_HIST_UPDATE_WEIGHT, 980, 0, 2048);
    TUNABLE(PAWN_HIST_UPDATE_WEIGHT, 837, 0, 2048);
    TUNABLE(CONT1_HIST_UPDATE_WEIGHT, 1025, 0, 2048);
    TUNABLE(CONT2_HIST_UPDATE_WEIGHT, 1192, 0, 2048);
    TUNABLE(CONT4_HIST_UPDATE_WEIGHT, 826, 0, 2048);
    TUNABLE(CONT6_HIST_UPDATE_WEIGHT, 993, 0, 2048);
    TUNABLE(CAPTURE_HIST_UPDATE_WEIGHT, 1043, 0, 2048);

    TUNABLE(CONT_HIST_BASE_MAIN_HIST_WEIGHT, 130, 0, 4096);
    TUNABLE(CONT_HIST_BASE_PAWN_HIST_WEIGHT, 395, 0, 4096);
    TUNABLE(CONT_HIST_BASE_CONT1_HIST_WEIGHT, 497, 0, 4096);
    TUNABLE(CONT_HIST_BASE_CONT2_HIST_WEIGHT, 868, 0, 4096);
    TUNABLE(CONT_HIST_BASE_CONT4_HIST_WEIGHT, 180, 0, 4096);
    TUNABLE(CONT_HIST_BASE_CONT6_HIST_WEIGHT, 813, 0, 4096);

    TUNABLE(CORR_HIST_BONUS_MAX, 278, 0, 2048);
    TUNABLE(CORR_HIST_PENALTY_MAX, 490, 0, 2048);

    TUNABLE(PAWN_CORR_HIST_WEIGHT, 50, 0, 128);
    TUNABLE(FRIENDLY_NONPAWN_CORR_HIST_WEIGHT, 42, 0, 128);
    TUNABLE(ENEMY_NONPAWN_CORR_HIST_WEIGHT, 36, 0, 128);
    TUNABLE(MINOR_PIECE_CORR_HIST_WEIGHT, 31, 0, 128);
    TUNABLE(MAJOR_PIECE_CORR_HIST_WEIGHT, 32, 0, 128);
    TUNABLE(WINNING_THREATS_CORR_HIST_WEIGHT, 11, 0, 128);

    TUNABLE(CONT1_CORR_HIST_WEIGHT, 67, 0, 128);
    TUNABLE(CONT2_CORR_HIST_WEIGHT, 63, 0, 128);
    TUNABLE(CONT4_CORR_HIST_WEIGHT, 40, 0, 128);
    TUNABLE(CONT6_CORR_HIST_WEIGHT, 30, 0, 128);

    static constexpr Int32 WINDOW_MIN_DEPTH = 3;

    TUNABLE(WINDOW_INIT_DELTA, 5, 1, 50);
    TUNABLE(WINDOW_SQUARED_SCORE_SCALE, 83, 0, 1024);
    TUNABLE(WINDOW_FAIL_HIGH_FREDUCTION, 99, 0, 768);
    TUNABLE(WINDOW_MAX_FREDUCTION, 450, 0, 896);
    TUNABLE(WINDOW_WIDENING_SCALE, 129, 1, 256);

    static constexpr Int32 OPTIMISM_MIN_DEPTH = 2;

    TUNABLE(OPTIMISM_SCORE_SCALE, 150, 75, 300);
    TUNABLE(OPTIMISM_DIVISOR_OFFSET, 107, 50, 200);

    TUNABLE(HINDSIGHT_FEXT_MIN_FREDUCTION, 327, 0, 1024);
    TUNABLE(HINDSIGHT_FEXTENSION, 67, 0, 512);
    TUNABLE(HINDSIGHT_FRED_MIN_FDEPTH, 200, 128, 1024);
    TUNABLE(HINDSIGHT_FRED_MIN_FREDUCTION, 261, 0, 1024);
    TUNABLE(HINDSIGHT_FRED_MARGIN, 265, 0, 500);
    TUNABLE(HINDSIGHT_FREDUCTION, 97, 0, 512);

    TUNABLE(IIR_MIN_FDEPTH, 413, 128, 1024);
    TUNABLE(IIR_TT_FDEPTH_OFFSET, 315, 0, 1024);
    TUNABLE(IIR_FREDUCTION, 118, 0, 512);

    TUNABLE(RFP_MARGIN_LINEAR_DEPTH_SCALE, 38, 0, 128);
    TUNABLE(RFP_MARGIN_QUADRATIC_DEPTH_SCALE, 7, 0, 25);
    TUNABLE(RFP_MARGIN_IMPROVING_SCALE, 4, 0, 150);
    TUNABLE(RFP_MARGIN_COMPLEXITY_SCALE, 199, 0, 512);
    TUNABLE(RFP_MAX_FDEPTH, 1266, 0, 2560);
    TUNABLE(RFP_FAIL_FIRM_T, 805, 0, 1024);

    TUNABLE(RAZORING_MARGIN_DEPTH_SCALE, 215, 0, 512);
    TUNABLE(RAZORING_MAX_FDEPTH, 452, 0, 1280);
    TUNABLE(RAZORING_FULL_ALWAYS_MAX_FDEPTH, 133, 0, 256);
    TUNABLE(RAZORING_FULL_MAX_FDEPTH, 259, 0, 512);
    TUNABLE(RAZORING_FULL_MARGIN, 180, 0, 400);
    TUNABLE(RAZORING_FRED_MAX_FDEPTH, 351, 0, 768);
    TUNABLE(RAZORING_FREDUCTION, 103, 0, 512);

    TUNABLE(NMP_MARGIN_BASE, 200, 0, 400);
    TUNABLE(NMP_MARGIN_DEPTH_SCALE, 1307, 0, 2560);
    TUNABLE(NMP_MARGIN_IMPROVING_SCALE, 24, 0, 100);
    TUNABLE(NMP_MIN_FDEPTH, 394, 0, 1280);
    TUNABLE(NMP_FRED_BASE, 490, 0, 1024);
    TUNABLE(NMP_FRED_FDEPTH_SCALE, 212, 0, 1024);
    TUNABLE(NMP_FRED_STATIC_EVAL_DIFF_SCALE, 86, 0, 128);
    TUNABLE(NMP_FRED_STATIC_EVAL_DIFF_MAX, 353, 0, 768);
    TUNABLE(NMP_NO_VERIF_MAX_FDEPTH, 1913, 1024, 3072);
    TUNABLE(NMP_NO_VERIF_MARGIN, 42, 0, 200);
    TUNABLE(NMP_MIN_PLY_FDEPTH_SCALE, 111, 0, 128);

    TUNABLE(PROBCUT_BETA_OFFSET, 359, 0, 500);
    TUNABLE(PROBCUT_BETA_IMPROVING_SCALE, -36, -250, 250);
    TUNABLE(PROBCUT_FREDUCTION, 369, 0, 768);
    TUNABLE(PROBCUT_MIN_FDEPTH, 870, 0, 1536);
    TUNABLE(PROBCUT_SEE_STATIC_EVAL_DIFF_SCALE, 146, 0, 256);

    TUNABLE(MINI_PROBCUT_BETA_OFFSET, 459, 0, 800);
    TUNABLE(MINI_PROBCUT_TT_FDEPTH_OFFSET, 388, 0, 1024);

    TUNABLE(IIR2_MIN_FDEPTH, 369, 128, 1024);
    TUNABLE(IIR2_FREDUCTION, 82, 0, 384);

    static inline MultiArray<Int32, 2, 256, 256> LMR_TABLE;

    namespace LMR { static void init(); }

    TUNABLE_CALLBACK(LMR_NOISY_BASE, -4, -50, 75, []() { LMR::init(); })
    TUNABLE_CALLBACK(LMR_NOISY_DIVISOR, 250, 150, 350, []() { LMR::init(); })
    TUNABLE_CALLBACK(LMR_QUIET_BASE, 77, 50, 125, []() { LMR::init(); })
    TUNABLE_CALLBACK(LMR_QUIET_DIVISOR, 246, 100, 300, []() { LMR::init(); })

    static inline void LMR::init() {
        LMR_TABLE.fill({});

        const Float64 noisyBase = Utils::floatDiv100(LMR_NOISY_BASE);
        const Float64 noisyDivisor = Utils::floatDiv100(LMR_NOISY_DIVISOR);
        const Float64 quietBase = Utils::floatDiv100(LMR_QUIET_BASE);
        const Float64 quietDivisor = Utils::floatDiv100(LMR_QUIET_DIVISOR);

        for (Int32 depth = 1; depth < 256; depth++) {
            for (Int32 moves = 1; moves < 256; moves++) {
                const Float64 lnDepth = std::log(static_cast<Float64>(depth));
                const Float64 lnMoves = std::log(static_cast<Float64>(moves));
                LMR_TABLE[0][static_cast<USize>(depth)][static_cast<USize>(moves)] =
                    static_cast<Int32>(1024.0 * (noisyBase + lnDepth * lnMoves / noisyDivisor));
                LMR_TABLE[1][static_cast<USize>(depth)][static_cast<USize>(moves)] =
                    static_cast<Int32>(1024.0 * (quietBase + lnDepth * lnMoves / quietDivisor));
            }
        }
    }

    TUNABLE(LMR_FDEPTH_TT_PV_SCALE, 810, 0, 1536);

    static inline MultiArray<Int32, 2, 256> LMP_TABLE;

    namespace LMP { static void init(); }

    TUNABLE_CALLBACK(LMP_TABLE_BASE, 4, 0, 20, []() { LMP::init(); })

    static inline void LMP::init() {
        LMP_TABLE.fill({});

        for (Int32 improving = 0; improving < 2; improving++) {
            for (Int32 depth = 0; depth < 256; depth++) {
                LMP_TABLE[static_cast<USize>(improving)][static_cast<USize>(depth)] = (LMP_TABLE_BASE + depth * depth) / (2 - improving);
            }
        }
    }

    TUNABLE(LMP_MARGIN_HISTORY_SCALE, 751, 0, 2048);

    TUNABLE(QUIET_HISTORY_PRUNING_MAX_FDEPTH, 548, 0, 1280);
    TUNABLE(QUIET_HISTORY_PRUNING_MARGIN_DEPTH_SCALE, -2287, -4096, 0);
    TUNABLE(QUIET_HISTORY_PRUNING_MARGIN_OFFSET, -1652, -4096, 4096);

    TUNABLE(QUIET_FP_MARGIN_BASE, 245, 0, 500);
    TUNABLE(QUIET_FP_MARGIN_DEPTH_SCALE, 54, 0, 128);
    TUNABLE(QUIET_FP_MARGIN_HISTORY_DIVISOR, 105, 32, 384);
    TUNABLE(QUIET_FP_MAX_FDEPTH, 940, 0, 2048);

    TUNABLE(NOISY_HISTORY_PRUNING_MAX_FDEPTH, 454, 0, 1280);
    TUNABLE(NOISY_HISTORY_PRUNING_MARGIN_DEPTH_SCALE, -750, -4096, 0);
    TUNABLE(NOISY_HISTORY_PRUNING_MARGIN_OFFSET, -1175, -4096, 4096);

    TUNABLE(BNFP_MARGIN_BASE, 319, 0, 500);
    TUNABLE(BNFP_MARGIN_DEPTH_SCALE, 81, 0, 128);
    TUNABLE(BNFP_MARGIN_HISTORY_DIVISOR, 104, 32, 384);
    TUNABLE(BNFP_MAX_FDEPTH, 943, 0, 2048);

    TUNABLE(CAPTURE_FP_MARGIN_BASE, 370, 0, 600);
    TUNABLE(CAPTURE_FP_MARGIN_DEPTH_SCALE, 310, 0, 512);
    TUNABLE(CAPTURE_FP_MAX_FDEPTH, 610, 0, 2048);

    TUNABLE(SEE_PRUNING_MARGIN_QUIET_DEPTH_SCALE, -14, -100, -1);
    TUNABLE(SEE_PRUNING_MARGIN_NOISY_DEPTH_SCALE, -114, -150, -1);
    TUNABLE(SEE_PRUNING_MARGIN_NOISY_HISTORY_DIVISOR, 81, 12, 256);

    TUNABLE(SE_MIN_FDEPTH_BASE, 719, 0, 1280);
    TUNABLE(SE_MIN_FDEPTH_TT_PV_SCALE, 102, 0, 512);
    TUNABLE(SE_TT_FDEPTH_OFFSET, 389, 0, 1024);
    TUNABLE(SE_BETA_MARGIN_BASE, 95, 0, 256);
    TUNABLE(SE_BETA_MARGIN_PREV_PV_SCALE, 107, 0, 256);

    TUNABLE(SE_DOUBLE_FEXT_MARGIN_BASE, 9, -50, 150);
    TUNABLE(SE_DOUBLE_FEXT_MARGIN_PV_SCALE, 139, 0, 300);
    TUNABLE(SE_DOUBLE_FEXT_MARGIN_NEW_PV_SCALE, 50, 0, 100);
    TUNABLE(SE_DOUBLE_FEXT_MARGIN_COMPLEXITY_SCALE, 874, 0, 8192);

    TUNABLE(SE_TRIPLE_FEXT_MARGIN_BASE, 111, 0, 300);
    TUNABLE(SE_TRIPLE_FEXT_MARGIN_PV_SCALE, 385, 0, 800);
    TUNABLE(SE_TRIPLE_FEXT_MARGIN_NEW_PV_SCALE, 57, 0, 100);
    TUNABLE(SE_TRIPLE_FEXT_MARGIN_NOISY_TT_MOVE_SCALE, 158, 0, 500);
    TUNABLE(SE_TRIPLE_FEXT_MARGIN_COMPLEXITY_SCALE, 1037, 0, 8192);

    TUNABLE(SE_QUADRUPLE_FEXT_MARGIN_BASE, 278, 0, 500);
    TUNABLE(SE_QUADRUPLE_FEXT_MARGIN_PV_SCALE, 521, 0, 1200);
    TUNABLE(SE_QUADRUPLE_FEXT_MARGIN_NEW_PV_SCALE, 72, 0, 150);
    TUNABLE(SE_QUADRUPLE_FEXT_MARGIN_NOISY_TT_MOVE_SCALE, 332, 0, 800);
    TUNABLE(SE_QUADRUPLE_FEXT_MARGIN_COMPLEXITY_SCALE, 1599, 0, 8192);

    TUNABLE(SE_SINGLE_FEXTENSION, 177, 0, 384);
    TUNABLE(SE_DOUBLE_FEXTENSION, 131, 0, 384);
    TUNABLE(SE_TRIPLE_FEXTENSION, 86, 0, 256);
    TUNABLE(SE_QUADRUPLE_FEXTENSION, 85, 0, 256);
    TUNABLE(SE_FAIL_HIGH_NEG_FEXTENSION, 456, 0, 768);
    TUNABLE(SE_CUT_NODE_NEG_FEXTENSION, 202, 0, 640);
    TUNABLE(SE_FAIL_LOW_NEG_FEXTENSION, 61, 0, 256);
    TUNABLE(SE_SCORE_NEG_FEXTENSION, 45, 0, 256);

    TUNABLE(MULTICUT_FAIL_FIRM_T, 567, 0, 1024);

    TUNABLE(MLDE_MAX_FDEPTH, 1262, 0, 2560);
    TUNABLE(MLDE_MARGIN, 19, 0, 150);
    TUNABLE(MLDE_FEXTENSION, 69, 0, 256);

    TUNABLE(LDSE_MAX_FDEPTH, 863, 0, 2048);
    TUNABLE(LDSE_SINGLE_FEXT_MARGIN, 32, 0, 150);
    TUNABLE(LDSE_DOUBLE_FEXT_MARGIN, 26, 0, 300);
    TUNABLE(LDSE_DOUBLE_FEXT_TT_FDEPTH_OFFSET, 296, 0, 1024);
    TUNABLE(LDSE_SINGLE_FEXTENSION, 107, 0, 384);
    TUNABLE(LDSE_DOUBLE_FEXTENSION, 141, 0, 384);

    TUNABLE(CUT_NODE_FRED_MIN_FDEPTH, 820, 0, 1536);
    TUNABLE(CUT_NODE_FREDUCTION, 31, 0, 256);

    static constexpr Int32 LMR_MIN_MOVES = 2;

    TUNABLE(LMR_MIN_FDEPTH, 239, 0, 1024);
    TUNABLE(LMR_FRED_OFFSET, 329, -2048, 2048);
    TUNABLE(LMR_FRED_QUIET_HISTORY_SCALE, 395, 0, 768);
    TUNABLE(LMR_FRED_NOISY_HISTORY_SCALE, 463, 0, 768);
    TUNABLE(LMR_FRED_NON_PV_SCALE, 925, 0, 3072);
    TUNABLE(LMR_FRED_TT_PV_SCALE, 1141, 0, 3072);
    TUNABLE(LMR_FRED_IMPROVING_SCALE, 1312, 0, 3072);
    TUNABLE(LMR_FRED_GIVES_CHECK_SCALE, 926, 0, 3072);
    TUNABLE(LMR_FRED_CUT_NODE_SCALE, 1983, 0, 3072);
    TUNABLE(LMR_FRED_TT_PV_FAIL_LOW_SCALE, 1119, 0, 3072);
    TUNABLE(LMR_FRED_ALPHA_RAISES_SCALE, 486, 0, 3072);
    TUNABLE(LMR_FRED_NOISY_TT_MOVE_SCALE, 1193, 0, 3072);
    TUNABLE(LMR_FRED_MOVES_TRIED_SCALE, 56, 0, 128);
    TUNABLE(LMR_FRED_COMPLEXITY_SCALE, 640, 0, 2048);

    TUNABLE(DEEPER_SEARCH_MARGIN_BASE, 44, 0, 150);
    TUNABLE(DEEPER_SEARCH_MARGIN_DEPTH_SCALE, 4, 0, 20);
    TUNABLE(DEEPER_SEARCH_FEXTENSION, 139, 0, 384);

    TUNABLE(SHALLOWER_SEARCH_MARGIN_BASE, 14, 0, 150);
    TUNABLE(SHALLOWER_SEARCH_MARGIN_DEPTH_SCALE, 4, 0, 20);
    TUNABLE(SHALLOWER_SEARCH_FREDUCTION, 114, 0, 384);

    TUNABLE(PCM_WEIGHT_BASE, -1, -1024, 1024);
    TUNABLE(PCM_WEIGHT_DEPTH_SCALE, 411, 0, 1024);
    TUNABLE(PCM_WEIGHT_DEPTH_MAX, 3520, 0, 8192);
    TUNABLE(PCM_WEIGHT_PREV_MOVES_TRIED_SCALE, 1202, 0, 2048);
    TUNABLE(PCM_WEIGHT_PREV_MOVES_TRIED_MIN, 7, 1, 21);
    TUNABLE(PCM_WEIGHT_PREV_TT_MOVE_SCALE, 1021, 0, 2048);
    TUNABLE(PCM_WEIGHT_STATIC_EVAL_SCALE, 1031, 0, 2048);
    TUNABLE(PCM_WEIGHT_PREV_STATIC_EVAL_SCALE, 1096, 0, 2048);
    TUNABLE(PCM_WEIGHT_MARGIN, 128, 0, 250);
    TUNABLE(PCM_WEIGHT_PREV_MARGIN, 134, 0, 250);

    TUNABLE(QSEARCH_FAIL_FIRM_T, 650, 0, 1024);
    TUNABLE(QSEARCH_FP_MARGIN, 135, 0, 400);
    TUNABLE(QSEARCH_SEE_PRUNING_MARGIN, -131, -2000, 200);

    static constexpr Int32 QSEARCH_MAX_MOVES = 2;

    inline void TunableList::init() noexcept {
        SEE::init();
        LMR::init();
        LMP::init();
    }
}
