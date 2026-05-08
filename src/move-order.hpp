#pragma once

#include <algorithm>
#include <array>
#include <limits>

#include "history.hpp"
#include "position.hpp"
#include "move-gen.hpp"
#include "move.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Syft {

struct ScoredMove {
    Move move;
    Int32 score;
};

enum class MoveOrderStage : UInt8 {
    HASH,
    GEN_NOISY,
    GOOD_NOISY,
    KILLER1,
    KILLER2,
    GEN_QUIET,
    BAD_NOISY_QUIET,
    QSEARCH_HASH,
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
    constexpr MoveOrder(const Position &position, const History &history, const Move hashMove, const std::array<Move, 2> &killerMoves, USize ply) noexcept : position_(position), history_(history), moveOrderStage_(MoveOrderStage::HASH), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), hashMove_(hashMove), killerMoves_(killerMoves), rootPly_(ply) {}
    constexpr MoveOrder(const Position &position, const History &history, const Move hashMove) noexcept : position_(position), history_(history), moveOrderStage_(MoveOrderStage::QSEARCH_HASH), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), hashMove_(hashMove), killerMoves_(), rootPly_(0) {}

    ScoredMove next() noexcept {
        if (moveOrderStage_ == MoveOrderStage::HASH) {
            moveOrderStage_++;
            if (hashMove_ != Move::NULL_MOVE && position_.legal(hashMove_)) {
                return ScoredMove(hashMove_, MoveScore::HASH);
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
                if (scoredMove.move == hashMove_) {
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

        if (moveOrderStage_ == MoveOrderStage::KILLER1) {
            moveOrderStage_++;
            if (killerMoves_[0] != Move::NULL_MOVE && position_.legal(killerMoves_[0]) && position_.quiet(killerMoves_[0])) {
                if (killerMoves_[0] != hashMove_) {
                    return ScoredMove(killerMoves_[0], MoveScore::KILLER1);
                }
            } else {
                killerMoves_[0] = Move::NULL_MOVE;
            }
        }

        if (moveOrderStage_ == MoveOrderStage::KILLER2) {
            moveOrderStage_++;
            if (killerMoves_[1] != Move::NULL_MOVE && position_.legal(killerMoves_[1]) && position_.quiet(killerMoves_[1])) {
                if (killerMoves_[1] != hashMove_) {
                    return ScoredMove(killerMoves_[1], MoveScore::KILLER2);
                }
            } else {
                killerMoves_[1] = Move::NULL_MOVE;
            }
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
                if (scoredMove.move == hashMove_ || scoredMove.move == killerMoves_[0] || scoredMove.move == killerMoves_[1]) {
                    continue;
                }
                return scoredMove;
            }
            return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
        }

        if (moveOrderStage_ == MoveOrderStage::QSEARCH_HASH) {
            moveOrderStage_++;
            if (hashMove_ != Move::NULL_MOVE && position_.legal(hashMove_) && !position_.quiet(hashMove_)) {
                return ScoredMove(hashMove_, MoveScore::HASH);
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
                if (scoredMove.move == hashMove_) {
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
    static constexpr Int32 SEE_THRESHOLD_SCALE = 32;

    const Position &position_;
    const History &history_;

    MoveOrderStage moveOrderStage_;

    MoveList moves_;
    std::array<Int32, MoveList::MAX_MOVES> moveScores_;

    USize moveIndex_;
    USize firstQuietIndex_;

    Move hashMove_;
    std::array<Move, 2> killerMoves_;

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

        Int32 score = history_.noisyStats(position_, move);

        if (promotion) {
            score += (move.promotion() == PieceType::QUEEN) ? MoveScore::PROMOTION_BONUS : 0;
        }

        if (capture) {
            score += position_.mvv(move);
        }

        if (promotion || position_.see(move, -score / SEE_THRESHOLD_SCALE)) {
            score += MoveScore::GOOD_NOISY;
        }

        return score;
    }

    constexpr Int32 scoreQuiet(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return history_.quietStats(position_, move, rootPly_);
    }

    constexpr Int32 scoreQSearchNoisy(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);

        const bool capture = position_.capture(move);
        const bool promotion = (move.type() == MoveType::PROMOTION);

        Int32 score = history_.noisyStats(position_, move);

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
