#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <limits>

#include "color.hpp"
#include "move.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Clownfish {

using TimePoint = std::chrono::steady_clock::time_point;
using MS = std::chrono::milliseconds;

struct SearchLimits {
    Int32 depth = std::numeric_limits<Int32>::max();
    UInt64 nodes = std::numeric_limits<UInt64>::max();
    MS time = MS::max();

    struct {
        std::array<MS, 2> time = {MS(0), MS(0)};
        std::array<MS, 2> increment = {MS(0), MS(0)};
        Int32 movesToGo = 0;
        bool enabled = false;
    } clock;

    MS overhead = MS(0);
};

class TimeManager {
public:
    constexpr TimeManager() noexcept : startTime_(), softBound_(), hardBound_(), checkCount_(0), prevBestMove_(), stability_(1), averageScore_(0) {}

    MS elapsed() const noexcept { return std::chrono::duration_cast<MS>(std::chrono::steady_clock::now() - startTime_); }

    void limits(const SearchLimits &limits, Color color, USize moveCount) noexcept {
        if (limits.clock.enabled) {
            const MS time = std::max(MS(1), limits.clock.time[static_cast<USize>(color)] - limits.overhead);
            const MS increment = limits.clock.increment[static_cast<USize>(color)];

            // const Float64 baseTimeScale = (limits.clock.movesToGo > 0) ? (static_cast<Float64>(limits.clock.movesToGo)) : BASE_TIME_SCALE;
            // const auto baseTime = (time / baseTimeScale) + (increment * INCREMENT_SCALE);

            // softBound_ = std::chrono::duration_cast<MS>(baseTime * SOFT_TIME_SCALE);
            // hardBound_ = std::chrono::duration_cast<MS>(time * HARD_TIME_SCALE);

            // FIXME: go back to sophisticated time management once the search is more stable
            softBound_ = std::chrono::duration_cast<MS>((time / 20.0) + (increment / 2.0));
            hardBound_ = std::chrono::duration_cast<MS>(time * 0.25);

            if (moveCount == 1) {
                softBound_ = std::min(softBound_, ONE_MOVE_BOUND);
                hardBound_ = std::min(hardBound_, ONE_MOVE_BOUND);
            }
        }
    }

    void start() noexcept {
        checkCount_ = 0;
        startTime_ = std::chrono::steady_clock::now();
        stability_ = 0;
        prevBestMove_ = Move::NULL_MOVE;
    }

    // FIXME: remove [[maybe_unused]] after reverting time management changes
    bool stopSoft(const SearchLimits &limits, [[maybe_unused]] Int32 depth, [[maybe_unused]] Move bestMove, [[maybe_unused]] Int32 score, [[maybe_unused]] UInt64 bestMoveNodes, [[maybe_unused]] UInt64 nodes) noexcept {
        // const Float64 scale = softBoundScale(depth, bestMove, score, bestMoveNodes, nodes);

        // FIXME: go back to sophisticated time management once the search is more stable
        const Float64 scale = 1.0;
        if (limits.clock.enabled && elapsed() > std::chrono::duration_cast<MS>(softBound_ * scale)) {
            return true;
        }

        return false;
    }

    bool stopHard(const SearchLimits &limits, UInt64 nodes) noexcept {
        if (nodes >= limits.nodes) {
            return true;
        }

        if (++checkCount_ == CHECK_INTERVAL) {
            checkCount_ = 0;
            if (elapsed() > limits.time) {
                return true;
            }
            if (limits.clock.enabled && elapsed() > hardBound_) {
                return true;
            }
        }
        return false;
    }

private:
    static constexpr UInt32 CHECK_INTERVAL = 2048;

    static constexpr MS ONE_MOVE_BOUND = MS(500);

    static constexpr Float64 BASE_TIME_SCALE = 19.0;
    static constexpr Float64 INCREMENT_SCALE = 0.83;
    static constexpr Float64 SOFT_TIME_SCALE = 0.68;
    static constexpr Float64 HARD_TIME_SCALE = 0.56;

    static constexpr Float64 MATE_SCORE_SCALE = 0.15;
    static constexpr Float64 WIN_SCORE_SCALE = 0.6;
    static constexpr Float64 LOSS_SCORE_SCALE = 0.5;

    static constexpr Float64 NODE_SCALE_BASE = 2.63;
    static constexpr Float64 NODE_SCALE_COEFF = 1.7;
    static constexpr Float64 NODE_SCALE_MIN = 0.1;

    static constexpr Float64 STABILITY_SCALE_MIN = 0.75;
    static constexpr Float64 STABILITY_SCALE_MAX = 2.4;
    static constexpr Float64 STABILITY_SCALE_COEFF = 9.11;
    static constexpr Float64 STABILITY_SCALE_OFFSET = 0.8;
    static constexpr Float64 STABILITY_SCALE_POWER = -2.7;

    static constexpr Int32 STABILITY_SCALE_MIN_DEPTH = 6;

    static constexpr Float64 SCORE_SCALE_MIN = 0.6;
    static constexpr Float64 SCORE_SCALE_MAX = 1.7;
    static constexpr Float64 SCORE_SCALE_CHANGE_COEFF = 4.58;
    static constexpr Float64 SCORE_SCALE_OFFSET = 0.8;
    static constexpr Float64 SCORE_SCALE_COEFF = 0.41;
    static constexpr Float64 SCORE_SCALE_POS_SCALE = 1.09;
    static constexpr Float64 SCORE_SCALE_NEG_SCALE = 1.04;

    static constexpr Int32 AVG_SCORE_INTERPOLATION_COEFF = 8;

    static constexpr Float64 TIME_SCALE_MIN = 0.07;

    TimePoint startTime_;
    MS softBound_;
    MS hardBound_;

    UInt32 checkCount_;

    Move prevBestMove_;
    UInt32 stability_;
    Int32 averageScore_;

    constexpr Float64 softBoundScale(Int32 depth, Move bestMove, Int32 score, UInt64 bestMoveNodes, UInt64 nodes) noexcept {
        if (bestMove == prevBestMove_) {
            stability_++;
        } else {
            stability_ = 1;
            prevBestMove_ = bestMove;
        }

        if (Score::mate(score)) {
            return MATE_SCORE_SCALE;
        }

        if (Score::win(score)) {
            return WIN_SCORE_SCALE;
        }

        if (Score::loss(score)) {
            return LOSS_SCORE_SCALE;
        }

        Float64 scale = 1.0;

        const Float64 nodeFraction = static_cast<Float64>(bestMoveNodes) / static_cast<Float64>(nodes + 1);
        scale *= std::max(NODE_SCALE_MIN, NODE_SCALE_BASE - (nodeFraction * NODE_SCALE_COEFF));

        if (depth >= STABILITY_SCALE_MIN_DEPTH) {
            scale *= std::min(STABILITY_SCALE_MAX, STABILITY_SCALE_MIN + (STABILITY_SCALE_COEFF * std::pow(static_cast<Float64>(stability_) + STABILITY_SCALE_OFFSET, STABILITY_SCALE_POWER)));
        }

        const Float64 scoreChange = static_cast<Float64>(score - averageScore_) / SCORE_SCALE_CHANGE_COEFF;
        const Float64 scoreScaleSignCoeff = (scoreChange > 0) ? SCORE_SCALE_POS_SCALE : SCORE_SCALE_NEG_SCALE;
        const Float64 invScale = (scoreChange * SCORE_SCALE_COEFF) / (std::abs(scoreChange) + SCORE_SCALE_OFFSET) * scoreScaleSignCoeff;
        scale *= std::clamp(1.0 - invScale, SCORE_SCALE_MIN, SCORE_SCALE_MAX);
        averageScore_ = (averageScore_ * (AVG_SCORE_INTERPOLATION_COEFF - 1) + score) / AVG_SCORE_INTERPOLATION_COEFF;

        scale = std::max(scale, TIME_SCALE_MIN);
        return scale;
    }
};

}
