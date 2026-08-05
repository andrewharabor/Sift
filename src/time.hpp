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


namespace Sift {

using TimePoint = std::chrono::steady_clock::time_point;
using MS = std::chrono::milliseconds;

struct SearchLimits {
    Int32 depth = std::numeric_limits<Int32>::max();
    UInt64 nodes = std::numeric_limits<UInt64>::max();
    bool softNodes = false;
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
            const MS time = std::max(MS(1), limits.clock.time[static_cast<USize>(color)] - limits.overhead);
            const MS increment = limits.clock.increment[static_cast<USize>(color)];

            const Float64 baseTimeScale = static_cast<Float64>((limits.clock.movesToGo > 0) ? limits.clock.movesToGo : TIME_BASE_SCALE);
            const auto baseTime = (time / baseTimeScale) + (increment * floatDiv100(TIME_INCREMENT_SCALE));

            softBound_ = std::chrono::duration_cast<MS>(baseTime * floatDiv100(TIME_SOFT_SCALE));
            hardBound_ = std::chrono::duration_cast<MS>(time * floatDiv100(TIME_HARD_SCALE));

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

        if (limits.softNodes && nodes >= limits.nodes) {
            return true;
        }

        return false;
    }

    bool stopHard(const SearchLimits &limits, UInt64 nodes, Int32 numThreads) noexcept {
        if (!limits.softNodes && nodes * static_cast<UInt64>(numThreads) >= limits.nodes) {
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

    static constexpr Int32 STABILITY_SCALE_MIN_DEPTH = 6;
    static constexpr Int32 INTERPOLATION_COEFF = 8;

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
            return floatDiv100(TIME_MATE_SCORE_SCALE);
        }

        if (score >= Score::KNOWN_WIN) {
            return floatDiv100(TIME_WIN_SCORE_SCALE);
        }

        if (score <= Score::KNOWN_LOSS) {
            return floatDiv100(TIME_LOSS_SCORE_SCALE);
        }

        Float64 scale = 1.0;

        const Float64 nodeFraction = static_cast<Float64>(bestMoveNodes) / static_cast<Float64>(nodes + 1);
        scale *= std::max(floatDiv100(TIME_NODE_SCALE_MIN), floatDiv100(TIME_NODE_SCALE_BASE) - (nodeFraction * floatDiv100(TIME_NODE_SCALE_COEFF)));

        if (depth >= STABILITY_SCALE_MIN_DEPTH) {
            scale *= std::min(floatDiv100(TIME_STABILITY_SCALE_MAX), floatDiv100(TIME_STABILITY_SCALE_MIN) + (floatDiv100(TIME_STABILITY_SCALE_COEFF) * std::pow(static_cast<Float64>(stability_) + floatDiv100(TIME_STABILITY_SCALE_OFFSET), floatDiv100(TIME_STABILITY_SCALE_POWER))));
        }

        const Float64 scoreChange = static_cast<Float64>(score - averageScore_) / floatDiv100(TIME_SCORE_SCALE_CHANGE_COEFF);
        const Float64 scoreScaleSignCoeff = (scoreChange > 0) ? floatDiv100(TIME_SCORE_SCALE_POS_SCALE) : floatDiv100(TIME_SCORE_SCALE_NEG_SCALE);
        const Float64 invScale = (scoreChange * floatDiv100(TIME_SCORE_SCALE_COEFF)) / (std::abs(scoreChange) + floatDiv100(TIME_SCORE_SCALE_OFFSET)) * scoreScaleSignCoeff;
        scale *= std::clamp(1.0 - invScale, floatDiv100(TIME_SCORE_SCALE_MIN), floatDiv100(TIME_SCORE_SCALE_MAX));
        averageScore_ = (averageScore_ * (INTERPOLATION_COEFF - 1) + score) / INTERPOLATION_COEFF;

        scale = std::max(scale, floatDiv100(TIME_SCALE_MIN));
        return scale;
    }

    static constexpr Float64 floatDiv100(Int32 value) noexcept { return static_cast<Float64>(value) / 100.0; }
};

}
