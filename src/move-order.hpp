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

    constexpr MoveOrder(const Position &position, Move hashMove, const std::array<Move, 2> &killerMoves) noexcept : position_(position), moves_(), moveIndex_(0), /*firstQuietIndex_(0),*/ hashMove_(hashMove), killerMoves_(killerMoves) {
        // FIXME

        MoveGen::legal(position_, moves_);

        for (const Move move : moves_) {
            Int32 score = 0;
            if (move == hashMove_) {
                score = 0;
            } else if (move == killerMoves_[0]) {
                score = 1;
            } else if (move == killerMoves_[1]) {
                score = 2;
            } else if (position_.noisy(move)) {
                score = 3;
            } else {
                score = 4;
            }
            scoredMoves_.push_back(ScoredMove(move, score));
        }

        const auto compare = [](const ScoredMove &move1, const ScoredMove &move2) {
            return move1.score < move2.score;
        };
        std::stable_sort(scoredMoves_.begin(), scoredMoves_.end(), compare);
    }

    constexpr MoveOrder(const Position &position, Move hashMove) noexcept : position_(position), moves_(), moveIndex_(0), /*firstQuietIndex_(0),*/ hashMove_(hashMove), killerMoves_() {
        // FIXME

        MoveGen::legal<MoveGenType::NOISY>(position_, moves_);

        for (const Move move : moves_) {
            Int32 score = 0;
            if (move == hashMove_) {
                score = 0;
            } else {
                score = 1;
            }
            scoredMoves_.push_back(ScoredMove(move, score));
        }

        const auto compare = [](const ScoredMove &move1, const ScoredMove &move2) {
            return move1.score < move2.score;
        };
        std::stable_sort(scoredMoves_.begin(), scoredMoves_.end(), compare);
    }

    ScoredMove next() noexcept {
        // FIXME

        if (moveIndex_ >= scoredMoves_.size()) {
            return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
        }
        return scoredMoves_.at(moveIndex_++);
    }

private:
    const Position &position_;

    MoveList moves_;
    // std::array<Int32, MoveList::MAX_MOVES> moveScores_; // FIXME
    USize moveIndex_;
    // USize firstQuietIndex_; // FIXME

    std::vector<ScoredMove> scoredMoves_; // FIXME

    Move hashMove_;
    std::array<Move, 2> killerMoves_;
};

}
