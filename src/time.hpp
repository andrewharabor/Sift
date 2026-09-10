#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <limits>

#include "color.hpp"
#include "move.hpp"
#include "score.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"


namespace Sift {

using TimePoint = std::chrono::steady_clock::time_point;
using MS = std::chrono::milliseconds;

struct SearchLimits {
    Int32 depth = std::numeric_limits<Int32>::max();
    UInt64 nodes = std::numeric_limits<UInt64>::max();
    MS time = MS::max();
    bool infinite = false;
    MoveList moves;

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
            const MS time = std::max(MS(1), limits.clock.time[color.index()] - limits.overhead);
            const MS increment = limits.clock.increment[color.index()];

            const Float64 baseTimeScale = static_cast<Float64>((limits.clock.movesToGo > 0) ? limits.clock.movesToGo : TIME_BASE_TIME_SCALE);
            const auto baseTime = (time / baseTimeScale) + (increment * Utils::floatDiv100(TIME_BASE_TIME_INCREMENT_SCALE));

            softBound_ = std::chrono::duration_cast<MS>(baseTime * Utils::floatDiv100(TIME_SOFT_BOUND_SCALE));
            hardBound_ = std::chrono::duration_cast<MS>(time * Utils::floatDiv100(TIME_HARD_BOUND_SCALE));

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

    bool stopSoft(const SearchLimits &limits, Int32 depth, Move bestMove, Int32 score, UInt64 bestMoveNodes, UInt64 nodes) noexcept {
        if (limits.clock.enabled && elapsed() > std::chrono::duration_cast<MS>(softBound_ * softBoundScale(depth, bestMove, score, bestMoveNodes, nodes))) {
            return true;
        }

        return false;
    }

    bool stopHard(const SearchLimits &limits, UInt64 nodes, USize numThreads) noexcept {
        if (nodes * static_cast<UInt64>(numThreads) >= limits.nodes) {
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
            return Utils::floatDiv100(TIME_SCALE_MATE_SCORE);
        }

        if (Score::decisive(score)) {
            return Utils::floatDiv100(TIME_SCALE_DECISIVE_SCORE);
        }

        Float64 scale = 1.0;

        const Float64 nodeFraction = static_cast<Float64>(bestMoveNodes) / static_cast<Float64>(nodes + 1);
        scale *= std::max(Utils::floatDiv100(TIME_SCALE_NODE_MIN), Utils::floatDiv100(TIME_SCALE_NODE_BASE) - (nodeFraction * Utils::floatDiv100(TIME_SCALE_NODE_SCALE)));

        if (depth >= TIME_SCALE_STABILITY_MIN_DEPTH) {
            scale *= std::min(Utils::floatDiv100(TIME_SCALE_STABILITY_MAX), Utils::floatDiv100(TIME_SCALE_STABILITY_BASE) + (Utils::floatDiv100(TIME_SCALE_STABILITY_SCALE) * std::pow(static_cast<Float64>(stability_) + Utils::floatDiv100(TIME_SCALE_STABILITY_OFFSET), Utils::floatDiv100(TIME_SCALE_STABILITY_POWER))));
        }

        const Float64 scoreChange = static_cast<Float64>(score - averageScore_) / Utils::floatDiv100(TIME_SCALE_SCORE_CHANGE_DIVISOR);
        const Float64 scoreScaleSignCoeff = (scoreChange > 0) ? Utils::floatDiv100(TIME_SCALE_POS_SCORE_SCALE) : Utils::floatDiv100(TIME_SCALE_NEG_SCORE_SCALE);
        const Float64 invScale = (scoreChange * Utils::floatDiv100(TIME_SCALE_SCORE_SCALE)) / (std::abs(scoreChange) + Utils::floatDiv100(TIME_SCALE_SCORE_OFFSET)) * scoreScaleSignCoeff;
        scale *= std::clamp(1.0 - invScale, Utils::floatDiv100(TIME_SCALE_SCORE_MIN), Utils::floatDiv100(TIME_SCALE_SCORE_MAX));
        averageScore_ = Utils::linInterp<8>(averageScore_, score, 1);

        scale = std::max(scale, Utils::floatDiv100(TIME_SCALE_MIN));
        return scale;
    }
};

}
