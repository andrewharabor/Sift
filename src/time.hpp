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
    MS time = MS::max();

    struct {
        std::array<MS, 2> time = {MS(0), MS(0)};
        std::array<MS, 2> increment = {MS(0), MS(0)};
        Int32 movesToGo = 100;
        // Int32 movesToGo = 20; // FIXME
        bool enabled = false;
    } clock;

    MS overhead = MS(10);
};

class TimeManager {
public:
    constexpr TimeManager() noexcept : startTime_(), softBound_(), hardBound_(), checkCount_(0), prevBestMove_(), stability_(0) {}

    MS elapsed() const noexcept { return std::chrono::duration_cast<MS>(std::chrono::steady_clock::now() - startTime_); }

    void limits(const SearchLimits &limits, Color color, USize moveCount) noexcept {
        if (limits.clock.enabled) {
            const MS time = std::max(MS(1), limits.clock.time[static_cast<USize>(color)] - limits.overhead);
            const MS increment = limits.clock.increment[static_cast<USize>(color)];

            const auto baseTime = (time / static_cast<Float64>(limits.clock.movesToGo)) + (increment * INCREMENT_SCALE);
            softBound_ = std::chrono::duration_cast<MS>(baseTime * SOFT_TIME_SCALE);
            hardBound_ = std::chrono::duration_cast<MS>(time * HARD_TIME_SCALE);

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

    bool stopSoft(const SearchLimits &limits, Move bestMove, UInt64 bestMoveNodes, UInt64 nodes) noexcept {
        if (bestMove == prevBestMove_) {
            stability_++;
        } else {
            stability_ = 0;
            prevBestMove_ = bestMove;
        }

        const Float64 nodeFraction = static_cast<Float64>(bestMoveNodes) / static_cast<Float64>(nodes + 1);
        const Float64 nodeScale = (NODE_TIME_BASE - nodeFraction) * NODE_TIME_SCALE;
        const Float64 stabilityScale = std::max(MOVE_STABILITY_MIN, MOVE_STABILITY_BASE + MOVE_STABILITY_SCALE * std::pow(stability_ + MOVE_STABILITY_OFFSET, MOVE_STABILITY_POWER));
        const Float64 scale = nodeScale * stabilityScale;

        // FIXME
        // #include <iostream>
        //         std::cout << "debug:" << std::endl;
        //         std::cout << "\tnodeFraction " << nodeFraction << std::endl;
        //         std::cout << "\tnodeScale " << nodeScale << std::endl;
        //         std::cout << "\tstability " << stability_ << std::endl;
        //         std::cout << "\tstabilityScale " << stabilityScale << std::endl;
        //         std::cout << "\tscale " << scale << std::endl;
        //         std::cout << "\tsoftBound " << softBound_.count() << std::endl;
        //         std::cout << "\thardBound " << hardBound_.count() << std::endl;
        //         std::cout << "\teffective softBound " << (softBound_ * scale).count() << std::endl;

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

    static constexpr Float64 INCREMENT_SCALE = 0.89;
    static constexpr Float64 SOFT_TIME_SCALE = 0.28;
    // static constexpr Float64 SOFT_TIME_SCALE = 0.7; // FIXME
    static constexpr Float64 HARD_TIME_SCALE = 0.61;

    static constexpr Float64 NODE_TIME_BASE = 1.4;
    static constexpr Float64 NODE_TIME_SCALE = 1.59;

    static constexpr Float64 MOVE_STABILITY_MIN = 0.93;
    static constexpr Float64 MOVE_STABILITY_BASE = 0.77;
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
