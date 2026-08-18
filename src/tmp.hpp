#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <limits>
#include <span>

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
    QUIET,
    BAD_NOISY,
    QSEARCH_TTABLE,
    QSEARCH_GEN_NOISY,
    QSEARCH_NOISY,
    QSEARCH_EVASIONS_TTABLE,
    QSEARCH_EVASIONS_GEN_NOISY,
    QSEARCH_EVASIONS_NOISY,
    QSEARCH_EVASIONS_GEN_QUIET,
    QSEARCH_EVASIONS_QUIET,
    PROBCUT_TTABLE,
    PROBCUT_GEN_NOISY,
    PROBCUT_NOISY,
    END,
};

inline MoveOrderStage &operator++(MoveOrderStage &stage) {
    assert(stage != MoveOrderStage::END);
    stage = static_cast<MoveOrderStage>(static_cast<UInt8>(stage) + 1);
    return stage;
}

inline std::strong_ordering operator<=>(MoveOrderStage stage1, MoveOrderStage stage2) { return static_cast<UInt8>(stage1) <=> static_cast<UInt8>(stage2); }

class MoveOrder {
public:
    static inline MoveOrder search(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move tTableMove, USize rootPly) noexcept { return MoveOrder(MoveOrderStage::TTABLE, position, history, historyStack, tTableMove, rootPly); }

    static inline MoveOrder qsearch(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move tTableMove, USize rootPly, bool evasions) {
        const MoveOrderStage stage = (evasions || position.inCheck()) ? (MoveOrderStage::QSEARCH_EVASIONS_TTABLE) : (MoveOrderStage::QSEARCH_TTABLE);
        return MoveOrder(stage, position, history, historyStack, tTableMove, rootPly);
    }

    static inline MoveOrder probcut(const Position &position, )


        constexpr void skipQuiets() noexcept { skipQuiets_ = true; }

    constexpr MoveOrderStage stage() const noexcept { return stage_; }

    // constexpr MoveOrder(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, const Move tTableMove, USize ply) noexcept : position_(position), history_(history), historyStack_(historyStack), moveOrderStage_(MoveOrderStage::TTABLE), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), tTableMove_(tTableMove), rootPly_(ply) {}
    // constexpr MoveOrder(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, const Move tTableMove) noexcept : position_(position), history_(history), historyStack_(historyStack), moveOrderStage_(MoveOrderStage::QSEARCH_TTABLE), moves_(), moveScores_(), moveIndex_(0), firstQuietIndex_(0), tTableMove_(tTableMove), rootPly_(0) {}

    // ScoredMove next() noexcept {
    //     if (moveOrderStage_ == MoveOrderStage::TTABLE) {
    //         moveOrderStage_++;
    //         if (tTableMove_ != Move::NULL_MOVE && position_.legal(tTableMove_)) {
    //             return ScoredMove(tTableMove_, MoveScore::TTABLE);
    //         }
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::GEN_NOISY) {
    //         moveOrderStage_++;
    //         MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
    //         for (USize i = 0; i < moves_.size(); i++) {
    //             moveScores_[i] = scoreNoisy(moves_[i]);
    //         }
    //         firstQuietIndex_ = moves_.size();
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::GOOD_NOISY) {
    //         while (moveIndex_ < moves_.size()) {
    //             ScoredMove scoredMove = findHighest();
    //             if (scoredMove.move == tTableMove_) {
    //                 continue;
    //             }
    //             if (scoredMove.score <= MoveScore::BAD_NOISY) {
    //                 moveIndex_--;
    //                 break;
    //             }
    //             return scoredMove;
    //         }
    //         moveOrderStage_++;
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::GEN_QUIET) {
    //         moveOrderStage_++;
    //         MoveGen::legal<MoveGenType::QUIET>(position_, moves_);
    //         for (USize i = firstQuietIndex_; i < moves_.size(); i++) {
    //             moveScores_[i] = scoreQuiet(moves_[i]);
    //         }
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::BAD_NOISY_QUIET) {
    //         while (moveIndex_ < moves_.size()) {
    //             ScoredMove scoredMove = findHighest();
    //             if (scoredMove.move == tTableMove_) {
    //                 continue;
    //             }
    //             return scoredMove;
    //         }
    //         return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::QSEARCH_TTABLE) {
    //         moveOrderStage_++;
    //         if (tTableMove_ != Move::NULL_MOVE && position_.legal(tTableMove_) && !position_.quiet(tTableMove_)) {
    //             return ScoredMove(tTableMove_, MoveScore::TTABLE);
    //         }
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::QSEARCH_GEN_NOISY) {
    //         moveOrderStage_++;
    //         MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
    //         for (USize i = 0; i < moves_.size(); i++) {
    //             moveScores_[i] = scoreQSearchNoisy(moves_[i]);
    //         }
    //     }

    //     if (moveOrderStage_ == MoveOrderStage::QSEARCH_NOISY) {
    //         while (moveIndex_ < moves_.size()) {
    //             ScoredMove scoredMove = findHighest();
    //             if (scoredMove.move == tTableMove_) {
    //                 continue;
    //             }
    //             return scoredMove;
    //         }
    //         return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
    //     }

    //     if (moveIndex_ >= moves_.size()) {
    //         return ScoredMove(Move::NULL_MOVE, MoveScore::NONE);
    //     }
    //     return findHighest();
    // }

private:
    MoveOrderStage stage_;

    const Position &position_;
    const History &history_;
    std::span<const HistoryStackEntry> historyStack_;

    Move tTableMove_;

    USize rootPly_;

    MoveList moves_;
    std::array<Int32, MoveList::MAX_MOVES> moveScores_;

    USize idx_;
    USize end_;
    USize badNoisyEnd_;

    bool skipQuiets_;

    explicit MoveOrder(MoveOrderStage initialStage, const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move tTableMove, USize rootPly) noexcept : stage_(initialStage), position_(position), history_(history), historyStack_(historyStack), tTableMove_(tTableMove), rootPly_(rootPly), moves_(), moveScores_(), idx_(0), end_(0), badNoisyEnd_(0), skipQuiets_(false) {}

    // ScoredMove findHighest() noexcept {
    //     Int32 bestScore = std::numeric_limits<Int32>::min();
    //     USize bestIndex = moveIndex_;
    //     for (USize i = moveIndex_; i < moves_.size(); i++) {
    //         if (moveScores_[i] > bestScore) {
    //             bestScore = moveScores_[i];
    //             bestIndex = i;
    //         }
    //     }

    //     std::swap(moves_[moveIndex_], moves_[bestIndex]);
    //     std::swap(moveScores_[moveIndex_], moveScores_[bestIndex]);

    //     return ScoredMove(moves_[moveIndex_], moveScores_[moveIndex_++]);
    // }

    // constexpr Int32 scoreNoisy(const Move move) const noexcept {
    //     assert(move != Move::NULL_MOVE);

    //     const bool capture = position_.capture(move);
    //     const bool promotion = (move.type() == MoveType::PROMOTION);

    //     Int32 score = history_.noisyScore(position_, move);

    //     if (promotion) {
    //         score += (move.promotion() == PieceType::QUEEN) ? MoveScore::PROMOTION_BONUS : 0;
    //     }

    //     if (capture) {
    //         score += position_.mvv(move);
    //     }

    //     if (promotion || position_.see(move, -score / NOISY_MOVE_SEE_THRESHOLD_SCALE)) {
    //         score += MoveScore::GOOD_NOISY;
    //     }

    //     return score;
    // }

    // constexpr Int32 scoreQuiet(const Move move) const noexcept {
    //     assert(move != Move::NULL_MOVE);
    //     return history_.quietScore(position_, historyStack_, move, rootPly_);
    // }

    // constexpr Int32 scoreQSearchNoisy(const Move move) const noexcept {
    //     assert(move != Move::NULL_MOVE);

    //     const bool capture = position_.capture(move);
    //     const bool promotion = (move.type() == MoveType::PROMOTION);

    //     Int32 score = history_.noisyScore(position_, move);

    //     if (promotion) {
    //         score += (move.promotion() == PieceType::QUEEN) ? MoveScore::QSEARCH_PROMOTION_BONUS : 0;
    //     }

    //     if (capture) {
    //         score += position_.mvv(move);
    //     }

    //     return score;
    // }

};

}
