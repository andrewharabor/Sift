#pragma once

#include <array>
#include <chrono>
#include <limits>

#include "color.hpp"
#include "move.hpp"
#include "types.hpp"


namespace Clownfish {

using TimePoint = std::chrono::steady_clock::time_point;
using MS = std::chrono::milliseconds;

struct SearchLimits {
    Int32 depth = std::numeric_limits<Int32>::max();
    UInt64 nodes = std::numeric_limits<UInt64>::max();
    UInt64 softNodes = std::numeric_limits<UInt64>::max();
    MS time = MS::max();

    struct {
        std::array<MS, 2> time;
        std::array<MS, 2> increment;
        bool enabled = false;
    } clock;

    MS overhead = MS(0);
};

class TimeManager {
public:
    constexpr TimeManager() noexcept : checkCount_(0), startTime_(), softBound_(), hardBound_(), prevBestMove_(), stability_(0) {}

    MS elapsed() const noexcept {
        return std::chrono::duration_cast<MS>(std::chrono::steady_clock::now() - startTime_);
    }

    void searchLimits(const SearchLimits &limits, Color color) noexcept {
        if (limits.clock.enabled) {
            MS time = std::max(MS(1), limits.clock.time[static_cast<USize>(color)] - limits.overhead);
            MS increment = limits.clock.increment[static_cast<USize>(color)];

            auto baseTime = (time / BASE_TIME_SCALE) + (increment * INCREMENT_SCALE);
            softBound_ = std::chrono::duration_cast<MS>(baseTime * SOFT_TIME_SCALE);
            hardBound_ = std::chrono::duration_cast<MS>(time * HARD_TIME_SCALE);
        }
    }

    void start() noexcept {
        checkCount_ = 0;
        startTime_ = std::chrono::steady_clock::now();
        stability_ = 0;
        prevBestMove_ = Move::NULL_MOVE;
    }

    bool stopSoft(const SearchLimits &limits, Move bestMove, UInt64 moveNodes, UInt64 nodes) noexcept {
        if (nodes > limits.softNodes) {
            return true;
        }

        if (bestMove == prevBestMove_) {
            stability_++;
        } else {
            stability_ = 0;
            prevBestMove_ = bestMove;
        }

        Float64 nodeFraction = static_cast<Float64>(moveNodes) / static_cast<Float64>(nodes + 1);
        Float64 nodeScale = (NODE_TIME_BASE - nodeFraction) * NODE_TIME_SCALE;
        Float64 stabilityScale = std::max(MOVE_STABILITY_MIN, MOVE_STABILITY_BASE + MOVE_STABILITY_SCALE * std::pow(stability_ + MOVE_STABILITY_OFFSET, MOVE_STABILITY_POWER));
        Float64 scale = nodeScale * stabilityScale;

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

    static constexpr Float64 BASE_TIME_SCALE = 0.2;
    static constexpr Float64 HARD_TIME_SCALE = 0.61;
    static constexpr Float64 SOFT_TIME_SCALE = 0.7;
    static constexpr Float64 INCREMENT_SCALE = 0.89;

    static constexpr Float64 NODE_TIME_BASE = 1.4;
    static constexpr Float64 NODE_TIME_SCALE = 1.59;

    static constexpr Float64 MOVE_STABILITY_BASE = 0.77;
    static constexpr Float64 MOVE_STABILITY_MIN = 0.93;
    static constexpr Float64 MOVE_STABILITY_SCALE = 9.69;
    static constexpr Float64 MOVE_STABILITY_OFFSET = 2.66;
    static constexpr Float64 MOVE_STABILITY_POWER = -1.53;

    TimePoint startTime_;
    MS softBound_;
    MS hardBound_;

    UInt32 checkCount_;

    Move prevBestMove_;
    UInt32 stability_;
};

}
