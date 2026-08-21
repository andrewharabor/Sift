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

enum class MoveOrderStage : UInt8 {
    TT,
    GEN_NOISY,
    GOOD_NOISY,
    GEN_QUIET,
    QUIET,
    BAD_NOISY,
    QSEARCH_TT,
    QSEARCH_GEN_NOISY,
    QSEARCH_NOISY,
    QSEARCH_EVASIONS_TT,
    QSEARCH_EVASIONS_GEN_NOISY,
    QSEARCH_EVASIONS_NOISY,
    QSEARCH_EVASIONS_GEN_QUIET,
    QSEARCH_EVASIONS_QUIET,
    PROBCUT_TT,
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
    static inline MoveOrder search(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move ttMove, USize rootPly) noexcept { return MoveOrder(MoveOrderStage::TT, position, history, historyStack, ttMove, rootPly); }

    static inline MoveOrder qsearch(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move ttMove, USize rootPly, bool forceEvasions) {
        const MoveOrderStage stage = (forceEvasions || position.inCheck()) ? (MoveOrderStage::QSEARCH_EVASIONS_TT) : (MoveOrderStage::QSEARCH_TT);
        return MoveOrder(stage, position, history, historyStack, ttMove, rootPly);
    }

    static inline MoveOrder probcut(const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move ttMove, USize rootPly) noexcept { return MoveOrder(MoveOrderStage::PROBCUT_TT, position, history, historyStack, ttMove, rootPly); }

    Move next() noexcept {
        if (stage_ == MoveOrderStage::TT) {
            ++stage_;

            if (ttMove_ != Move::NULL_MOVE && position_.legal(ttMove_)) {
                return ttMove_;
            }
        }

        if (stage_ == MoveOrderStage::GEN_NOISY) {
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            end_ = moves_.size();
            scoreNoisies();

            ++stage_;
        }

        if (stage_ == MoveOrderStage::GOOD_NOISY) {
            while (idx_ < end_) {
                const USize i = findNext();
                const Move move = moves_[i];
                const Int32 score = moveScores_[i];

                if (move == ttMove_) {
                    continue;
                }

                const Int32 margin = -score / MOVE_ORDER_GOOD_NOISY_SCORE_DIVISOR + MOVE_ORDER_GOOD_NOISY_SEE_OFFSET;
                if (!position_.see(move, margin)) {
                    moves_[badNoisyEnd_] = moves_[i];
                    moveScores_[badNoisyEnd_] = moveScores_[i];
                    badNoisyEnd_++;
                } else {
                    return move;
                }
            }

            ++stage_;
        }

        if (stage_ == MoveOrderStage::GEN_QUIET) {
            if (!skipQuiets_) {
                MoveGen::legal<MoveGenType::QUIET>(position_, moves_);
                end_ = moves_.size();
                scoreQuiets();
            }

            ++stage_;
        }

        if (stage_ == MoveOrderStage::QUIET) {
            if (!skipQuiets_) {
                if (const Move move = selectNext<true>(); move != Move::NULL_MOVE) {
                    return move;
                }
            }

            idx_ = 0;
            end_ = badNoisyEnd_;

            ++stage_;
        }

        if (stage_ == MoveOrderStage::BAD_NOISY) {
            if (const Move move = selectNext<false>(); move != Move::NULL_MOVE) {
                return move;
            }

            stage_ = MoveOrderStage::END;
            return Move::NULL_MOVE;
        }

        if (stage_ == MoveOrderStage::QSEARCH_TT) {
            ++stage_;

            if (ttMove_ != Move::NULL_MOVE && position_.legal(ttMove_)) {
                return ttMove_;
            }
        }

        if (stage_ == MoveOrderStage::QSEARCH_GEN_NOISY) {
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            end_ = moves_.size();
            scoreNoisies();

            ++stage_;
        }

        if (stage_ == MoveOrderStage::QSEARCH_NOISY) {
            if (const Move move = selectNext<true>(); move != Move::NULL_MOVE) {
                return move;
            }

            stage_ = MoveOrderStage::END;
            return Move::NULL_MOVE;
        }

        if (stage_ == MoveOrderStage::QSEARCH_EVASIONS_TT) {
            ++stage_;

            if (ttMove_ != Move::NULL_MOVE && position_.legal(ttMove_)) {
                return ttMove_;
            }
        }

        if (stage_ == MoveOrderStage::QSEARCH_EVASIONS_GEN_NOISY) {
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            end_ = moves_.size();
            scoreNoisies();

            ++stage_;
        }

        if (stage_ == MoveOrderStage::QSEARCH_EVASIONS_NOISY) {
            if (const Move move = selectNext<true>(); move != Move::NULL_MOVE) {
                return move;
            }

            ++stage_;
        }

        if (stage_ == MoveOrderStage::QSEARCH_EVASIONS_GEN_QUIET) {
            MoveGen::legal<MoveGenType::QUIET>(position_, moves_);
            end_ = moves_.size();
            scoreQuiets();

            ++stage_;
        }

        if (stage_ == MoveOrderStage::QSEARCH_EVASIONS_QUIET) {
            if (!skipQuiets_) {
                if (const Move move = selectNext<true>(); move != Move::NULL_MOVE) {
                    return move;
                }
            }

            stage_ = MoveOrderStage::END;
            return Move::NULL_MOVE;
        }

        if (stage_ == MoveOrderStage::PROBCUT_TT) {
            ++stage_;

            if (ttMove_ != Move::NULL_MOVE && position_.legal(ttMove_)) {
                return ttMove_;
            }
        }

        if (stage_ == MoveOrderStage::PROBCUT_GEN_NOISY) {
            MoveGen::legal<MoveGenType::NOISY>(position_, moves_);
            end_ = moves_.size();
            scoreNoisies();

            ++stage_;
        }

        if (stage_ == MoveOrderStage::PROBCUT_NOISY) {
            if (const Move move = selectNext<true>(); move != Move::NULL_MOVE) {
                return move;
            }

            stage_ = MoveOrderStage::END;
            return Move::NULL_MOVE;
        }

        return Move::NULL_MOVE;
    }

    constexpr void skipQuiets() noexcept { skipQuiets_ = true; }

    constexpr MoveOrderStage stage() const noexcept { return stage_; }

private:
    MoveOrderStage stage_;

    const Position &position_;
    const History &history_;
    std::span<const HistoryStackEntry> historyStack_;

    Move ttMove_;

    USize rootPly_;

    MoveList moves_;
    std::array<Int32, MoveList::MAX_MOVES> moveScores_;

    USize idx_;
    USize end_;
    USize badNoisyEnd_;

    bool skipQuiets_;

    explicit MoveOrder(MoveOrderStage initialStage, const Position &position, const History &history, std::span<const HistoryStackEntry> historyStack, Move ttMove, USize rootPly) noexcept : stage_(initialStage), position_(position), history_(history), historyStack_(historyStack), ttMove_(ttMove), rootPly_(rootPly), moves_(), moveScores_(), idx_(0), end_(0), badNoisyEnd_(0), skipQuiets_(false) {}

    USize findNext() noexcept {
        const auto castUSize = [](Int32 value) -> USize {
            Int64 widened = static_cast<Int64>(value);
            widened -= std::numeric_limits<Int32>::min();
            return static_cast<USize>(widened) << 32;
        };

        USize best = castUSize(moveScores_[idx_]) | (MoveList::MAX_MOVES - idx_);
        for (USize i = idx_ + 1; i < end_; i++) {
            const USize curr = castUSize(moveScores_[i]) | (MoveList::MAX_MOVES - i);
            best = std::max(best, curr);
        }

        const USize bestIdx = MoveList::MAX_MOVES - (best & 0xFFFFFFFF);
        if (bestIdx != idx_) {
            std::swap(moves_[idx_], moves_[bestIdx]);
            std::swap(moveScores_[idx_], moveScores_[bestIdx]);
        }

        return idx_++;
    }

    template<bool SORT>
    inline Move selectNext() noexcept {
        while (idx_ < end_) {
            const USize i = (SORT) ? findNext() : idx_++;
            const Move move = moves_[i];
            if (move != ttMove_) {
                return move;
            }
        }
        return Move::NULL_MOVE;
    }

    constexpr void scoreNoisies() noexcept {
        for (USize i = idx_; i < end_; i++) {
            const Move move = moves_[i];
            Int32 &score = moveScores_[i];

            score += history_.noisyScore(position_, move) / MOVE_ORDER_NOISY_SCORE_DIVISOR;
            score += SEE_PIECE_VALUES[static_cast<USize>(position_.captured(move).type())];
            if (move.type() == MoveType::PROMOTION) {
                score += SEE_PIECE_VALUES[static_cast<USize>(move.promotion())] - SEE_PIECE_VALUES[static_cast<USize>(PieceType::PAWN)];
            }
        }
    }

    constexpr void scoreQuiets() noexcept {
        for (USize i = idx_; i < end_; i++) {
            const Move move = moves_[i];
            Int32 &score = moveScores_[i];

            score += history_.quietScore(position_, historyStack_, move, rootPly_);
            score += MOVE_ORDER_DIRECT_CHECK_BONUS * (position_.directCheck(move) && position_.see(move, MOVE_ORDER_DIRECT_CHECK_SEE_MARGIN));
        }
    }

};

}
