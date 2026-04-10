#pragma once

#include <algorithm>
#include <array>

#include "position.hpp"
#include "move-gen.hpp"
#include "move.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Clownfish {

struct ScoredMove {
    Move move;
    Int32 score;
};

class MoveOrder {
public:

    constexpr MoveOrder(const Position &position, Move hashMove, const std::array<Move, 2> &killerMoves) noexcept : position_(position), hashMove_(hashMove), killerMoves_(killerMoves), moveIndex_(0), firstQuietIndex_(0) {
        // FIXME

        MoveGen::legal(position_, moves_);
        auto compare = [this](const Move &move1, const Move &move2) {
            return position_.noisy(move1) > position_.noisy(move2);
        };
        moves_.sort(compare);
    }

    constexpr MoveOrder(const Position &position, Move hashMove) noexcept : position_(position), hashMove_(hashMove), moveIndex_(0) {
        // FIXME

        MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
    }

    ScoredMove next() noexcept {
        // FIXME

        if (moveIndex_ == 0 && hashMove_ != Move::NULL_MOVE) {
            return ScoredMove(hashMove_, MoveScore::HASH);
        }

        if (moveIndex_ == 0 && killerMoves_[0] != Move::NULL_MOVE && killerMoves_[0] != hashMove_) {
            return ScoredMove(killerMoves_[0], MoveScore::KILLER1);
        }

        if (moveIndex_ == 0 && killerMoves_[1] != Move::NULL_MOVE && killerMoves_[1] != hashMove_) {
            return ScoredMove(killerMoves_[1], MoveScore::KILLER2);
        }

        Move move = Move();
        do {
            if (moveIndex_ >= moves_.size()) {
                return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
            }
            move = moves_.at(moveIndex_++);
        } while (move == hashMove_ || move == killerMoves_[0] || move == killerMoves_[1]);

        Int32 score = 0;
        if (position_.noisy(move)) {
            score = MoveScore::BAD_NOISY;
        }

        return ScoredMove(move, score);
    }

private:
    const Position &position_;

    MoveList moves_;
    std::array<Int32, MoveList::MAX_MOVES> moveScores_;
    USize moveIndex_;
    USize firstQuietIndex_;

    Move hashMove_;
    std::array<Move, 2> killerMoves_;
};

}
