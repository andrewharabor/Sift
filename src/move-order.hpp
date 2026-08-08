#pragma once

#include <algorithm>
#include <array>
#include <limits>

#include "history.hpp"
#include "position.hpp"
#include "move-gen.hpp"
#include "move.hpp"
#include "score.hpp"
#include "tunable.hpp"
#include "types.hpp"


namespace Sift {

struct ScoredMove {
    Move move;
    Int32 score;
};

enum class MoveOrderStage : UInt8 {
    TTABLE,
    GEN_NOISY,
    GOOD_NOISY,
    GEN_QUIET,
    BAD_NOISY_QUIET,
    QSEARCH_TTABLE,
    QSEARCH_GEN_NOISY,
    QSEARCH_NOISY
};

inline MoveOrderStage operator++(MoveOrderStage &type, int) {
    assert(type != MoveOrderStage::QSEARCH_NOISY);
    MoveOrderStage old = type;
    type = static_cast<MoveOrderStage>(static_cast<UInt8>(type) + 1);
    return old;
}

class MoveOrder {
public:
    constexpr MoveOrder(const Position &position, const History &history, const Move tTableMove, USize ply) noexcept : position_(position), history_(history), moveOrderStage_(MoveOrderStage::TTABLE), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), tTableMove_(tTableMove), rootPly_(ply) {}
    constexpr MoveOrder(const Position &position, const History &history, const Move tTableMove) noexcept : position_(position), history_(history), moveOrderStage_(MoveOrderStage::QSEARCH_TTABLE), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), tTableMove_(tTableMove), rootPly_(0) {}

    ScoredMove next() noexcept {
        if (moveOrderStage_ == MoveOrderStage::TTABLE) {
            moveOrderStage_++;
            if (tTableMove_ != Move::NULL_MOVE && position_.legal(tTableMove_)) {
                return ScoredMove(tTableMove_, MoveScore::TTABLE);
            }
        }

        if (moveOrderStage_ == MoveOrderStage::GEN_NOISY) {
            moveOrderStage_++;
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            for (USize i = 0; i < moves_.size(); i++) {
                moveScores_[i] = scoreNoisy(moves_[i]);
            }
            firstQuietIndex_ = moves_.size();
        }

        if (moveOrderStage_ == MoveOrderStage::GOOD_NOISY) {
            while (moveIndex_ < moves_.size()) {
                ScoredMove scoredMove = findHighest();
                if (scoredMove.move == tTableMove_) {
                    continue;
                }
                if (scoredMove.score <= MoveScore::BAD_NOISY) {
                    moveIndex_--;
                    break;
                }
                return scoredMove;
            }
            moveOrderStage_++;
        }

        if (moveOrderStage_ == MoveOrderStage::GEN_QUIET) {
            moveOrderStage_++;
            MoveGen::legal<MoveGenType::QUIET>(position_, moves_);
            for (USize i = firstQuietIndex_; i < moves_.size(); i++) {
                moveScores_[i] = scoreQuiet(moves_[i]);
            }
        }

        if (moveOrderStage_ == MoveOrderStage::BAD_NOISY_QUIET) {
            while (moveIndex_ < moves_.size()) {
                ScoredMove scoredMove = findHighest();
                if (scoredMove.move == tTableMove_) {
                    continue;
                }
                return scoredMove;
            }
            return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
        }

        if (moveOrderStage_ == MoveOrderStage::QSEARCH_TTABLE) {
            moveOrderStage_++;
            if (tTableMove_ != Move::NULL_MOVE && position_.legal(tTableMove_) && !position_.quiet(tTableMove_)) {
                return ScoredMove(tTableMove_, MoveScore::TTABLE);
            }
        }

        if (moveOrderStage_ == MoveOrderStage::QSEARCH_GEN_NOISY) {
            moveOrderStage_++;
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            for (USize i = 0; i < moves_.size(); i++) {
                moveScores_[i] = scoreQSearchNoisy(moves_[i]);
            }
        }

        if (moveOrderStage_ == MoveOrderStage::QSEARCH_NOISY) {
            while (moveIndex_ < moves_.size()) {
                ScoredMove scoredMove = findHighest();
                if (scoredMove.move == tTableMove_) {
                    continue;
                }
                return scoredMove;
            }
            return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
        }

        if (moveIndex_ >= moves_.size()) {
            return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
        }
        return findHighest();
    }

private:
    const Position &position_;
    const History &history_;

    MoveOrderStage moveOrderStage_;

    MoveList moves_;
    std::array<Int32, MoveList::MAX_MOVES> moveScores_;

    USize moveIndex_;
    USize firstQuietIndex_;

    Move tTableMove_;

    USize rootPly_;

    ScoredMove findHighest() noexcept {
        Int32 bestScore = std::numeric_limits<Int32>::min();
        USize bestIndex = moveIndex_;
        for (USize i = moveIndex_; i < moves_.size(); i++) {
            if (moveScores_[i] > bestScore) {
                bestScore = moveScores_[i];
                bestIndex = i;
            }
        }

        std::swap(moves_[moveIndex_], moves_[bestIndex]);
        std::swap(moveScores_[moveIndex_], moveScores_[bestIndex]);

        return ScoredMove(moves_[moveIndex_], moveScores_[moveIndex_++]);
    }

    constexpr Int32 scoreNoisy(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);

        const bool capture = position_.capture(move);
        const bool promotion = (move.type() == MoveType::PROMOTION);

        Int32 score = history_.noisyScore(position_, move);

        if (promotion) {
            score += (move.promotion() == PieceType::QUEEN) ? MoveScore::PROMOTION_BONUS : 0;
        }

        if (capture) {
            score += position_.mvv(move);
        }

        if (promotion || position_.see(move, -score / NOISY_MOVE_SEE_THRESHOLD_SCALE)) {
            score += MoveScore::GOOD_NOISY;
        }

        return score;
    }

    constexpr Int32 scoreQuiet(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return history_.quietScore(position_, move, rootPly_);
    }

    constexpr Int32 scoreQSearchNoisy(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);

        const bool capture = position_.capture(move);
        const bool promotion = (move.type() == MoveType::PROMOTION);

        Int32 score = history_.noisyScore(position_, move);

        if (promotion) {
            score += (move.promotion() == PieceType::QUEEN) ? MoveScore::QSEARCH_PROMOTION_BONUS : 0;
        }

        if (capture) {
            score += position_.mvv(move);
        }

        return score;
    }

};

}
