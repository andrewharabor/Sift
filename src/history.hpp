#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

#include "color.hpp"
#include "bitboard.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"


namespace Sift {

class HistEntry {
public:
    HistEntry() noexcept : value_(0) {}
    HistEntry(Int32 value) noexcept : value_(static_cast<Int16>(value)) {}

    constexpr void update(Int32 bonus) noexcept {
        update(bonus, value_);
    }

    constexpr void update(Int32 bonus, Int32 base) noexcept {
        Int32 newValue = static_cast<Int32>(value_) + bonus - base * std::abs(bonus) / MAX;
        value_ = static_cast<Int16>(std::clamp(newValue, MIN, MAX));
    }

    constexpr Int32 value() const noexcept { return static_cast<Int32>(value_); }

private:
    static constexpr Int32 MAX = 16384;
    static constexpr Int32 MIN = -MAX;

    Int16 value_;
};

using ContHistEntry = MultiArray<HistEntry, 12, 64>;

class CorrHistEntry {
public:
    static constexpr Int32 SCALE = 256;

    CorrHistEntry() noexcept : value_(0) {}
    CorrHistEntry(Int32 value) noexcept : value_(value) {}

    void update(Int32 target, Int32 weight) noexcept {
        Int32 newValue = (value_ * (SCALE - weight) + target * weight) / SCALE;
        newValue = std::clamp(newValue, value_ - CORR_HIST_UPDATE_MAX, value_ + CORR_HIST_UPDATE_MAX);
        value_ = std::clamp(newValue, -CORR_HIST_MAX, CORR_HIST_MAX);
    }

    constexpr Int32 value() const noexcept {
        return value_;
    }

private:
    Int32 value_;
};

using ContCorrHistEntry = MultiArray<CorrHistEntry, 12, 64>;

using MainHistTable = MultiArray<HistEntry, 2, 4096, 2, 2>;
using PawnHistTable = MultiArray<HistEntry, 8192, 12, 64>;
using ContHistTable = MultiArray<ContHistEntry, 12, 64>;
using CaptureHistTable = MultiArray<HistEntry, 7, 12, 64, 2, 2>;

class CorrHistTable {
public:
    CorrHistTable() noexcept : data() {}

    CorrHistEntry &entry(Color color, UInt64 key) noexcept {
        return data[static_cast<USize>(color)][key % SIZE];
    }

    const CorrHistEntry &entry(Color color, UInt64 key) const noexcept {
        return data[static_cast<USize>(color)][key % SIZE];
    }

private:
    static constexpr USize SIZE = 16384;

    MultiArray<CorrHistEntry, 2, SIZE> data;
};

using PawnCorrHistTable = CorrHistTable;
using NonPawnCorrHistTable = std::array<CorrHistTable, 2>;
using ThreatCorrHistTable = CorrHistTable;
using MinorPieceCorrHistTable = CorrHistTable;
using MajorPieceCorrHistTable = CorrHistTable;
using ContCorrHistTable = MultiArray<ContCorrHistEntry, 12, 64>;

struct HistoryStackEntry {
    Move playedMove;
    Piece movedPiece;

    ContCorrHistEntry *contCorrHistEntry;
    ContHistEntry *contHistEntry;

    Int32 score;
};

class History {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    History() noexcept : mainHistTable_(), pawnHistTable_(), contHistTable_(), captureHistTable_(), pawnCorrHistTable_(), nonPawnCorrHistTable_(), threatCorrHistTable_(), minorPieceCorrHistTable_(), majorPieceCorrHistTable_(), contCorrHistTable_() { reset(); }

    void reset() noexcept {
        mainHistTable_.fill({});
        pawnHistTable_.fill({});
        contHistTable_.fill({});
        captureHistTable_.fill({});
        pawnCorrHistTable_ = {};
        nonPawnCorrHistTable_.fill({});
        threatCorrHistTable_ = {};
        minorPieceCorrHistTable_ = {};
        majorPieceCorrHistTable_ = {};
        contCorrHistTable_.fill({});
    }

    Int32 correctStaticEval(const Position &position, std::span<const HistoryStackEntry> stack, Int32 staticEval, USize rootPly) const noexcept {
        if (staticEval >= Score::KNOWN_WIN || staticEval <= Score::KNOWN_LOSS) {
            return staticEval;
        }

        const Color color = position.sideToMove();
        const UInt64 threatHash = Utils::murmurHash3((position.threats() & position.friendly(color)).bits());

        const Int32 pawnHistEntry = pawnCorrHistTable_.entry(color, position.pawnHash()).value();
        const Int32 friendlyNonPawnHistEntry = nonPawnCorrHistTable_[static_cast<USize>(color)].entry(color, position.nonPawnHash(color)).value();
        const Int32 enemyNonPawnHistEntry = nonPawnCorrHistTable_[static_cast<USize>(color)].entry(~color, position.nonPawnHash(~color)).value();
        const Int32 threatHistEntry = threatCorrHistTable_.entry(color, threatHash).value();
        const Int32 minorPieceHistEntry = minorPieceCorrHistTable_.entry(color, position.minorPieceHash()).value();
        const Int32 majorPieceHistEntry = majorPieceCorrHistTable_.entry(color, position.majorPieceHash()).value();

        Int32 correction = 0;
        correction += PAWN_CORR_HIST_WEIGHT * pawnHistEntry;
        correction += FRIENDLY_NONPAWN_CORR_HIST_WEIGHT * friendlyNonPawnHistEntry;
        correction += ENEMY_NONPAWN_CORR_HIST_WEIGHT * enemyNonPawnHistEntry;
        correction += THREAT_CORR_HIST_WEIGHT * threatHistEntry;
        correction += MINOR_PIECE_CORR_HIST_WEIGHT * minorPieceHistEntry;
        correction += MAJOR_PIECE_CORR_HIST_WEIGHT * majorPieceHistEntry;

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        for (USize ply = MIN_CONT_CORR_HIST_PLY; ply <= MAX_CONT_CORR_HIST_PLY; ply++) {
            if (rootPly >= ply && stack[rootPly - ply].contCorrHistEntry != nullptr) {
                Int32 contCorrHistEntry = (*stack[rootPly - ply].contCorrHistEntry)[static_cast<USize>(prevPiece)][prevMove.to().index()].value();
                correction += CONT_CORR_HISTORY_WEIGHTS[ply] * contCorrHistEntry;
            }
        }

        Int32 correctedStaticEval = staticEval + correction / (CorrHistEntry::SCALE * CorrHistEntry::SCALE);
        return std::clamp(correctedStaticEval, Score::KNOWN_LOSS + 1, Score::KNOWN_WIN - 1);
    }

    void updateCorrHist(const Position &position, std::span<const HistoryStackEntry> stack, Int32 depth, USize rootPly, Int32 bonus) noexcept {
        const Color color = position.sideToMove();
        const UInt64 threatHash = Utils::murmurHash3((position.threats() & position.friendly(color)).bits());
        const Int32 scaledBonus = bonus * CorrHistEntry::SCALE;
        const Int32 weight = CORR_HIST_DEPTH_WEIGHT_SCALE * std::min(1 + depth, CORR_HIST_DEPTH_WEIGHT_MAX);

        pawnCorrHistTable_.entry(color, position.pawnHash()).update(scaledBonus, weight);
        nonPawnCorrHistTable_[static_cast<USize>(color)].entry(color, position.nonPawnHash(color)).update(scaledBonus, weight);
        nonPawnCorrHistTable_[static_cast<USize>(color)].entry(~color, position.nonPawnHash(~color)).update(scaledBonus, weight);
        threatCorrHistTable_.entry(color, threatHash).update(scaledBonus, weight);
        minorPieceCorrHistTable_.entry(color, position.minorPieceHash()).update(scaledBonus, weight);
        majorPieceCorrHistTable_.entry(color, position.majorPieceHash()).update(scaledBonus, weight);

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        for (USize ply = MIN_CONT_CORR_HIST_PLY; ply <= MAX_CONT_CORR_HIST_PLY; ply++) {
            if (rootPly >= ply && stack[rootPly - ply].contCorrHistEntry != nullptr) {
                (*stack[rootPly - ply].contCorrHistEntry)[static_cast<USize>(prevPiece)][prevMove.to().index()].update(scaledBonus, weight);
            }
        }
    }

    Int32 quietScore(const Position &position, std::span<const HistoryStackEntry> stack, const Move move, USize rootPly) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 score = mainHist(move, position.threats(), movedPiece.color());
        score += pawnHist(move, movedPiece, position.pawnHash());
        if (rootPly > 0 && stack[rootPly - 1].contHistEntry != nullptr) {
            score += contHist(*stack[rootPly - 1].contHistEntry, move, movedPiece);
        }
        if (rootPly > 1 && stack[rootPly - 2].contHistEntry != nullptr) {
            score += contHist(*stack[rootPly - 2].contHistEntry, move, movedPiece);
        }
        if (rootPly > 3 && stack[rootPly - 4].contHistEntry != nullptr) {
            score += contHist(*stack[rootPly - 4].contHistEntry, move, movedPiece);
        }
        return score;
    }

    Int32 noisyScore(const Position &position, const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return captureHist(move, position.threats(), position.moved(move), position.captured(move));
    }

    void updateQuietHists(const Position &position, std::span<const HistoryStackEntry> stack, const Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateMainHist(move, position.threats(), position.sideToMove(), bonus);
        updatePawnHist(move, position.moved(move), position.pawnHash(), bonus);
        updateContHist(position, stack, move, rootPly, bonus);
    }

    void updateContHist(const Position &position, std::span<const HistoryStackEntry> stack, const Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 base = 0;
        base += mainHist(move, position.threats(), movedPiece.color()) / 2;
        if (rootPly > 0 && stack[rootPly - 1].contHistEntry != nullptr) {
            base += contHist(*stack[rootPly - 1].contHistEntry, move, movedPiece);
        }
        if (rootPly > 1 && stack[rootPly - 2].contHistEntry != nullptr) {
            base += contHist(*stack[rootPly - 2].contHistEntry, move, movedPiece);
        }
        if (rootPly > 3 && stack[rootPly - 4].contHistEntry != nullptr) {
            base += contHist(*stack[rootPly - 4].contHistEntry, move, movedPiece);
        }

        if (rootPly > 0 && stack[rootPly - 1].contHistEntry != nullptr) {
            updateContHist(*stack[rootPly - 1].contHistEntry, move, movedPiece, bonus, base);
        }
        if (rootPly > 1 && stack[rootPly - 2].contHistEntry != nullptr) {
            updateContHist(*stack[rootPly - 2].contHistEntry, move, movedPiece, bonus, base);
        }
        if (rootPly > 3 && stack[rootPly - 4].contHistEntry != nullptr) {
            updateContHist(*stack[rootPly - 4].contHistEntry, move, movedPiece, bonus, base);
        }
    }

    void updateNoisyHists(const Position &position, const Move move, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateCaptureHist(move, position.threats(), position.moved(move), position.captured(move), bonus);
    }

    constexpr ContHistEntry &contHistEntry(const Position &position, const Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContHistEntry &contHistEntry(const Position &position, const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr ContCorrHistEntry &contCorrHistEntry(const Position &position, const Move move) noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.moved(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContCorrHistEntry &contCorrHistEntry(const Position &position, const Move move) const noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.moved(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    static Int32 bonus(Int32 depth) noexcept {
        Int32 result = (HISTORY_BONUS_QUADRATIC * depth * depth / HISTORY_BONUS_SCALE) + (HISTORY_BONUS_LINEAR * depth) - HISTORY_BONUS_OFFSET;
        return std::min(result, HISTORY_BONUS_MAX);
    }

    static Int32 penalty(Int32 depth) noexcept {
        Int32 result = (HISTORY_PENALTY_QUADRATIC * depth * depth / HISTORY_PENALTY_SCALE) + (HISTORY_PENALTY_LINEAR * depth) - HISTORY_PENALTY_OFFSET;
        return -std::min(result, HISTORY_PENALTY_MAX);
    }

private:
    static constexpr Int32 CORR_HIST_DEPTH_WEIGHT_SCALE = 2;
    static constexpr Int32 CORR_HIST_DEPTH_WEIGHT_MAX = 16;

    MainHistTable mainHistTable_;
    PawnHistTable pawnHistTable_;
    ContHistTable contHistTable_;
    CaptureHistTable captureHistTable_;
    PawnCorrHistTable pawnCorrHistTable_;
    NonPawnCorrHistTable nonPawnCorrHistTable_;
    ThreatCorrHistTable threatCorrHistTable_;
    MinorPieceCorrHistTable minorPieceCorrHistTable_;
    MajorPieceCorrHistTable majorPieceCorrHistTable_;
    ContCorrHistTable contCorrHistTable_;

    constexpr Int32 mainHist(const Move move, Bitboard threats, Color color) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return mainHistTable_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].value();
    }

    constexpr Int32 pawnHist(const Move move, Piece movedPiece, UInt64 hash) const noexcept {
        assert(move != Move::NULL_MOVE);
        return pawnHistTable_[hash % pawnHistTable_.size()][static_cast<USize>(movedPiece)][move.to().index()].value();
    }

    constexpr Int32 contHist(const ContHistEntry &contHistEntry, const Move move, Piece movedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        return contHistEntry[static_cast<USize>(movedPiece)][move.to().index()].value();
    }

    constexpr Int32 captureHist(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return captureHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].value();
    }

    constexpr void updateMainHist(const Move move, Bitboard threats, Color color, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        mainHistTable_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].update(bonus);
    }

    constexpr void updatePawnHist(const Move move, Piece movedPiece, UInt64 hash, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        pawnHistTable_[hash % pawnHistTable_.size()][static_cast<USize>(movedPiece)][move.to().index()].update(bonus);
    }

    void updateContHist(ContHistEntry &contHistEntry, const Move move, Piece movedPiece, Int32 bonus, Int32 base) noexcept {
        assert(move != Move::NULL_MOVE);
        contHistEntry[static_cast<USize>(movedPiece)][move.to().index()].update(bonus, base);
    }

    void updateCaptureHist(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        captureHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].update(bonus);
    }
};

}
