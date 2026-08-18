#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstring>
#include <span>
#include <string>

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

    constexpr void update(Int32 bonus, Int32 base) noexcept { value_ = static_cast<Int16>(std::clamp(static_cast<Int32>(value_) + bonus - base * std::abs(bonus) / MAX, MIN, MAX)); }

    constexpr Int32 value() const noexcept { return static_cast<Int32>(value_); }

private:
    static constexpr Int32 MAX = 16384;
    static constexpr Int32 MIN = -MAX;

    Int16 value_;
};

using ContHistSubtable = MultiArray<HistEntry, 12, 64>;

class CorrHistEntry {
public:
    CorrHistEntry() noexcept : value_(0) {}
    CorrHistEntry(Int32 value) noexcept : value_(static_cast<Int16>(value)) {}

    constexpr void update(Int32 bonus) noexcept {
        Int32 v = static_cast<Int32>(value_);
        value_ = static_cast<Int16>(std::clamp(v + bonus - v * std::abs(bonus) / MAX, MIN, MAX));
    }

    constexpr Int32 value() const noexcept { return static_cast<Int32>(value_); }

private:
    static constexpr Int32 MAX = 1024;
    static constexpr Int32 MIN = -MAX;

    Int16 value_;
};

using ContCorrHistSubtable = MultiArray<CorrHistEntry, 12, 64>;

class SharedCorrHistEntry {
public:
    SharedCorrHistEntry() noexcept { (value_.store(0, std::memory_order_relaxed)); }
    SharedCorrHistEntry(Int32 value) noexcept { value_.store(static_cast<Int16>(value), std::memory_order_relaxed); }

    constexpr void update(Int32 bonus) noexcept {
        Int32 v = static_cast<Int32>(value_.load(std::memory_order_relaxed));
        value_.store(static_cast<Int16>(std::clamp(v + bonus - v * std::abs(bonus) / MAX, MIN, MAX)), std::memory_order_relaxed);
    }

    constexpr Int32 value() const noexcept { return static_cast<Int32>(value_.load(std::memory_order_relaxed)); }

private:
    static constexpr Int32 MAX = 1024;
    static constexpr Int32 MIN = -MAX;

    std::atomic<Int16> value_;
};

constexpr USize PAWN_HIST_SIZE = 8192;
constexpr USize CORR_HIST_SIZE = 16384;

using SharedCorrHistTable = MultiArray<SharedCorrHistEntry, 2, CORR_HIST_SIZE>;

using MainHistTable = MultiArray<HistEntry, 2, 4096, 2, 2>;
using PawnHistTable = MultiArray<HistEntry, PAWN_HIST_SIZE, 12, 64>;
using CaptureHistTable = MultiArray<HistEntry, 7, 12, 64, 2, 2>;
using ContHistTable = MultiArray<ContHistSubtable, 12, 64>;
using ContCorrHistTable = MultiArray<ContCorrHistSubtable, 12, 64>;

using PawnCorrHistTable = SharedCorrHistTable;
using NonPawnCorrHistTable = std::array<SharedCorrHistTable, 2>;
using MinorPieceCorrHistTable = SharedCorrHistTable;
using MajorPieceCorrHistTable = SharedCorrHistTable;

struct HistoryStackEntry {
    Move playedMove;
    Piece movedPiece;

    ContHistSubtable *contHistSubtable;
    ContCorrHistSubtable *contCorrHistSubtable;

    Int32 score;
};

class History {
public:
    History() noexcept { reset(); }

    void reset() noexcept {
        mainHistTable_.fill({});
        pawnHistTable_.fill({});
        captureHistTable_.fill({});
        contHistTable_.fill({});
        contCorrHistTable_.fill({});
    }

    constexpr Int32 quietScore(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 score = 0;
        score += MAIN_HIST_WEIGHT * mainHistScore(move, position.threats(), movedPiece.color());
        score += PAWN_HIST_WEIGHT * pawnHistScore(move, movedPiece, position.pawnHash());
        score += CONT1_HIST_WEIGHT * contHistScore(stack, rootPly, 1, move, movedPiece);
        score += CONT2_HIST_WEIGHT * contHistScore(stack, rootPly, 2, move, movedPiece);
        score += CONT4_HIST_WEIGHT * contHistScore(stack, rootPly, 4, move, movedPiece);
        score += CONT6_HIST_WEIGHT * contHistScore(stack, rootPly, 6, move, movedPiece);
        return score / HIST_DIVISOR;
    }

    constexpr Int32 noisyScore(const Position &position, Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return CAPTURE_HIST_WEIGHT * captureHistScore(move, position.threats(), position.moved(move), position.captured(move)) / HIST_DIVISOR;
    }

    constexpr void updateQuietHists(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateMainHist(move, position.threats(), position.sideToMove(), bonus);
        updatePawnHist(move, position.moved(move), position.pawnHash(), bonus);
        updateContHist(position, stack, move, rootPly, bonus);
    }

    constexpr void updateContHist(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 base = 0;
        base += CONT_HIST_BASE_MAIN_HIST_WEIGHT * mainHistScore(move, position.threats(), movedPiece.color());
        base += CONT_HIST_BASE_PAWN_HIST_WEIGHT * pawnHistScore(move, movedPiece, position.pawnHash());
        base += CONT_HIST_BASE_CONT1_HIST_WEIGHT * contHistScore(stack, rootPly, 1, move, movedPiece);
        base += CONT_HIST_BASE_CONT2_HIST_WEIGHT * contHistScore(stack, rootPly, 2, move, movedPiece);
        base += CONT_HIST_BASE_CONT4_HIST_WEIGHT * contHistScore(stack, rootPly, 4, move, movedPiece);
        base += CONT_HIST_BASE_CONT6_HIST_WEIGHT * contHistScore(stack, rootPly, 6, move, movedPiece);
        base /= HIST_DIVISOR;

        updateContHist(stack, rootPly, 1, move, movedPiece, bonus * CONT1_HIST_UPDATE_WEIGHT / HIST_DIVISOR, base);
        updateContHist(stack, rootPly, 2, move, movedPiece, bonus * CONT2_HIST_UPDATE_WEIGHT / HIST_DIVISOR, base);
        updateContHist(stack, rootPly, 4, move, movedPiece, bonus * CONT4_HIST_UPDATE_WEIGHT / HIST_DIVISOR, base);
        updateContHist(stack, rootPly, 6, move, movedPiece, bonus * CONT6_HIST_UPDATE_WEIGHT / HIST_DIVISOR, base);
    }

    constexpr void updateNoisyHists(const Position &position, Move move, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateCaptureHist(move, position.threats(), position.moved(move), position.captured(move), bonus);
    }

    constexpr ContHistSubtable &contHistSubtable(const Position &position, Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContHistSubtable &contHistSubtable(const Position &position, Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr ContCorrHistSubtable &contCorrHistSubtable(const Position &position, Move move) noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.moved(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContCorrHistSubtable &contCorrHistSubtable(const Position &position, Move move) const noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.moved(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    template<Int32 DEPTH_SCALE, Int32 OFFSET, Int32 MAX>
    static constexpr Int32 bonus(Int32 depth) noexcept { return std::clamp(depth * DEPTH_SCALE - OFFSET, 0, MAX); }

private:
    MainHistTable mainHistTable_;
    PawnHistTable pawnHistTable_;
    CaptureHistTable captureHistTable_;
    ContHistTable contHistTable_;
    ContCorrHistTable contCorrHistTable_;

    constexpr Int32 mainHistScore(Move move, Bitboard threats, Color color) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return mainHistTable_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].value();
    }

    constexpr Int32 pawnHistScore(Move move, Piece movedPiece, UInt64 hash) const noexcept {
        assert(move != Move::NULL_MOVE);
        return pawnHistTable_[hash % PAWN_HIST_SIZE][static_cast<USize>(movedPiece)][move.to().index()].value();
    }

    constexpr Int32 contHistScore(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece) const noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contHistSubtable != nullptr) {
            return (*stack[rootPly - ply].contHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].value();
        }
        return 0;
    }

    constexpr Int32 captureHistScore(Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return captureHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].value();
    }

    constexpr void updateMainHist(Move move, Bitboard threats, Color color, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        mainHistTable_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].update(bonus * MAIN_HIST_UPDATE_WEIGHT / HIST_DIVISOR);
    }

    constexpr void updatePawnHist(Move move, Piece movedPiece, UInt64 hash, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        pawnHistTable_[hash % PAWN_HIST_SIZE][static_cast<USize>(movedPiece)][move.to().index()].update(bonus * PAWN_HIST_UPDATE_WEIGHT / HIST_DIVISOR);
    }

    constexpr void updateContHist(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece, Int32 bonus, Int32 base) noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contHistSubtable != nullptr) {
            (*stack[rootPly - ply].contHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].update(bonus, base);
        }
    }

    constexpr void updateCaptureHist(Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        captureHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].update(bonus * CAPTURE_HIST_UPDATE_WEIGHT / HIST_DIVISOR);
    }
};

class SharedHistory {
public:
    SharedHistory() noexcept { reset(); }

    void reset() noexcept {
        std::memset(&pawnCorrHistTable_, 0, sizeof(pawnCorrHistTable_));
        std::memset(&nonPawnCorrHistTable_, 0, sizeof(nonPawnCorrHistTable_));
        std::memset(&minorPieceCorrHistTable_, 0, sizeof(minorPieceCorrHistTable_));
        std::memset(&majorPieceCorrHistTable_, 0, sizeof(majorPieceCorrHistTable_));
    }

    Int32 correction(const Position &position, std::span<const HistoryStackEntry> stack, USize rootPly) const noexcept {
        const Color color = position.sideToMove();

        Int32 correction = 0;
        correction += PAWN_CORR_HIST_WEIGHT * pawnCorrHistScore(position, color);
        correction += FRIENDLY_NONPAWN_CORR_HIST_WEIGHT * friendlyNonPawnCorrHistScore(position, color);
        correction += ENEMY_NONPAWN_CORR_HIST_WEIGHT * enemyNonPawnCorrHistScore(position, color);
        correction += MINOR_PIECE_CORR_HIST_WEIGHT * minorPieceCorrHistScore(position, color);
        correction += MAJOR_PIECE_CORR_HIST_WEIGHT * majorPieceCorrHistScore(position, color);

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        correction += CONT1_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 1, prevMove, prevPiece);
        correction += CONT2_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 2, prevMove, prevPiece);
        correction += CONT4_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 4, prevMove, prevPiece);
        correction += CONT6_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 6, prevMove, prevPiece);

        return correction / CORR_HIST_DIVISOR;
    }

    void updateCorrHist(const Position &position, std::span<const HistoryStackEntry> stack, USize rootPly, Int32 depth, Int32 searchScore, Int32 staticEval) noexcept {
        const Color color = position.sideToMove();

        const Int32 bonus = std::clamp((searchScore - staticEval) * depth / BONUS_DIVISOR, -CORR_HIST_PENALTY_MAX, CORR_HIST_BONUS_MAX);

        updatePawnCorrHist(position, color, bonus);
        updateFriendlyNonPawnCorrHist(position, color, bonus);
        updateEnemyNonPawnCorrHist(position, color, bonus);
        updateMinorPieceCorrHist(position, color, bonus);
        updateMajorPieceCorrHist(position, color, bonus);

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        updateContCorrHist(stack, rootPly, 1, prevMove, prevPiece, bonus);
        updateContCorrHist(stack, rootPly, 2, prevMove, prevPiece, bonus);
        updateContCorrHist(stack, rootPly, 4, prevMove, prevPiece, bonus);
        updateContCorrHist(stack, rootPly, 6, prevMove, prevPiece, bonus);
    }

private:
    static constexpr Int32 BONUS_DIVISOR = 8;

    PawnCorrHistTable pawnCorrHistTable_;
    NonPawnCorrHistTable nonPawnCorrHistTable_;
    MinorPieceCorrHistTable minorPieceCorrHistTable_;
    MajorPieceCorrHistTable majorPieceCorrHistTable_;

    constexpr Int32 pawnCorrHistScore(const Position &position, Color color) const noexcept {
        return pawnCorrHistTable_[static_cast<USize>(color)][position.pawnHash() % CORR_HIST_SIZE].value();
    }

    constexpr Int32 friendlyNonPawnCorrHistScore(const Position &position, Color color) const noexcept {
        return nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(color)][position.nonPawnHash(color) % CORR_HIST_SIZE].value();
    }

    constexpr Int32 enemyNonPawnCorrHistScore(const Position &position, Color color) const noexcept {
        return nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(~color)][position.nonPawnHash(~color) % CORR_HIST_SIZE].value();
    }

    constexpr Int32 minorPieceCorrHistScore(const Position &position, Color color) const noexcept {
        return minorPieceCorrHistTable_[static_cast<USize>(color)][position.minorPieceHash() % CORR_HIST_SIZE].value();
    }

    constexpr Int32 majorPieceCorrHistScore(const Position &position, Color color) const noexcept {
        return majorPieceCorrHistTable_[static_cast<USize>(color)][position.majorPieceHash() % CORR_HIST_SIZE].value();
    }

    constexpr Int32 contCorrHistScore(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece) const noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contCorrHistSubtable != nullptr) {
            return (*stack[rootPly - ply].contCorrHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].value();
        }
        return 0;
    }

    constexpr void updatePawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        pawnCorrHistTable_[static_cast<USize>(color)][position.pawnHash() % CORR_HIST_SIZE].update(bonus);
    }

    constexpr void updateFriendlyNonPawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(color)][position.nonPawnHash(color) % CORR_HIST_SIZE].update(bonus);
    }

    constexpr void updateEnemyNonPawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(~color)][position.nonPawnHash(~color) % CORR_HIST_SIZE].update(bonus);
    }

    constexpr void updateMinorPieceCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        minorPieceCorrHistTable_[static_cast<USize>(color)][position.minorPieceHash() % CORR_HIST_SIZE].update(bonus);
    }

    constexpr void updateMajorPieceCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        majorPieceCorrHistTable_[static_cast<USize>(color)][position.majorPieceHash() % CORR_HIST_SIZE].update(bonus);
    }

    constexpr void updateContCorrHist(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece, Int32 bonus) noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contCorrHistSubtable != nullptr) {
            (*stack[rootPly - ply].contCorrHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].update(bonus);
        }
    }
};

}
