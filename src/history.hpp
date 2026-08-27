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
using CaptHistTable = MultiArray<HistEntry, 7, 12, 64, 2, 2>;
using ContHistTable = MultiArray<ContHistSubtable, 12, 64>;
using ContCorrHistTable = MultiArray<ContCorrHistSubtable, 12, 64>;

using PawnCorrHistTable = SharedCorrHistTable;
using NonPawnCorrHistTable = std::array<SharedCorrHistTable, 2>;
using MinorPieceCorrHistTable = SharedCorrHistTable;
using MajorPieceCorrHistTable = SharedCorrHistTable;

struct HistoryStackEntry {
    Move move;
    bool quietMove;
    Piece movedPiece;
    Piece capturedPiece;
    Bitboard threats;
    UInt64 pawnHash;

    ContHistSubtable *contHistSubtable;
    ContCorrHistSubtable *contCorrHistSubtable;
};

class History {
public:
    History() noexcept { reset(); }

    void reset() noexcept {
        mainHistTable_.fill({});
        pawnHistTable_.fill({});
        captHistTable_.fill({});
        contHistTable_.fill({});
        contCorrHistTable_.fill({});
    }

    Int32 quietScore(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.movedPiece(move);
        Int32 score = 0;
        score += MAIN_HIST_WEIGHT * mainHistScore(move, position.threats(), movedPiece.color());
        score += PAWN_HIST_WEIGHT * pawnHistScore(move, movedPiece, position.pawnHash());
        score += CONT1_HIST_WEIGHT * contHistScore(stack, rootPly, 1, move, movedPiece);
        score += CONT2_HIST_WEIGHT * contHistScore(stack, rootPly, 2, move, movedPiece);
        score += CONT4_HIST_WEIGHT * contHistScore(stack, rootPly, 4, move, movedPiece);
        score += CONT6_HIST_WEIGHT * contHistScore(stack, rootPly, 6, move, movedPiece);
        return score / 1024;
    }

    Int32 noisyScore(const Position &position, Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return CAPTURE_HIST_WEIGHT * captHistScore(move, position.threats(), position.movedPiece(move), position.capturedPiece(move)) / 1024;
    }

    void updateMainPawnHists(const HistoryStackEntry &entry, Int32 bonus) noexcept {
        assert(entry.move != Move::NULL_MOVE);
        updateMainHist(entry.move, entry.threats, entry.movedPiece.color(), bonus);
        updatePawnHist(entry.move, entry.movedPiece, entry.pawnHash, bonus);
    }

    void updateMainPawnHists(const Position &position, Move move, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateMainHist(move, position.threats(), position.sideToMove(), bonus);
        updatePawnHist(move, position.movedPiece(move), position.pawnHash(), bonus);
    }

    void updateContHist(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.movedPiece(move);
        Int32 base = 0;
        base += CONT_HIST_BASE_MAIN_HIST_WEIGHT * mainHistScore(move, position.threats(), movedPiece.color());
        base += CONT_HIST_BASE_PAWN_HIST_WEIGHT * pawnHistScore(move, movedPiece, position.pawnHash());
        base += CONT_HIST_BASE_CONT1_HIST_WEIGHT * contHistScore(stack, rootPly, 1, move, movedPiece);
        base += CONT_HIST_BASE_CONT2_HIST_WEIGHT * contHistScore(stack, rootPly, 2, move, movedPiece);
        base += CONT_HIST_BASE_CONT4_HIST_WEIGHT * contHistScore(stack, rootPly, 4, move, movedPiece);
        base += CONT_HIST_BASE_CONT6_HIST_WEIGHT * contHistScore(stack, rootPly, 6, move, movedPiece);
        base /= 1024;

        updateContHist(stack, rootPly, 1, move, movedPiece, bonus * CONT1_HIST_UPDATE_WEIGHT / 1024, base);
        updateContHist(stack, rootPly, 2, move, movedPiece, bonus * CONT2_HIST_UPDATE_WEIGHT / 1024, base);
        updateContHist(stack, rootPly, 4, move, movedPiece, bonus * CONT4_HIST_UPDATE_WEIGHT / 1024, base);
        updateContHist(stack, rootPly, 6, move, movedPiece, bonus * CONT6_HIST_UPDATE_WEIGHT / 1024, base);
    }

    void updateQuietHists(const Position &position, std::span<const HistoryStackEntry> stack, Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateMainPawnHists(position, move, bonus);
        updateContHist(position, stack, move, rootPly, bonus);
    }

    void updateNoisyHists(const HistoryStackEntry &entry, Int32 bonus) noexcept {
        assert(entry.move != Move::NULL_MOVE);
        updateCaptHist(entry.move, entry.threats, entry.movedPiece, entry.capturedPiece, bonus);
    }

    void updateNoisyHists(const Position &position, Move move, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateCaptHist(move, position.threats(), position.movedPiece(move), position.capturedPiece(move), bonus);
    }

    constexpr ContHistSubtable &contHistSubtable(const Position &position, Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.movedPiece(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContHistSubtable &contHistSubtable(const Position &position, Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.movedPiece(move);
        return contHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr ContCorrHistSubtable &contCorrHistSubtable(const Position &position, Move move) noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.movedPiece(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContCorrHistSubtable &contCorrHistSubtable(const Position &position, Move move) const noexcept {
        const Piece movedPiece = (move == Move::NULL_MOVE) ? Piece(PieceType::PAWN, position.sideToMove()) : position.movedPiece(move);
        return contCorrHistTable_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    static constexpr Int32 bonus(Int32 fdepth, Int32 base, Int32 depthScale, Int32 max) noexcept { return std::clamp(base + depthScale * fdepth / FDEPTH_SCALE, 0, max); }

private:
    MainHistTable mainHistTable_;
    PawnHistTable pawnHistTable_;
    CaptHistTable captHistTable_;
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

    constexpr Int32 captHistScore(Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return captHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].value();
    }

    void updateMainHist(Move move, Bitboard threats, Color color, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        mainHistTable_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].update(bonus * MAIN_HIST_UPDATE_WEIGHT / 1024);
    }

    void updatePawnHist(Move move, Piece movedPiece, UInt64 hash, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        pawnHistTable_[hash % PAWN_HIST_SIZE][static_cast<USize>(movedPiece)][move.to().index()].update(bonus * PAWN_HIST_UPDATE_WEIGHT / 1024);
    }

    void updateContHist(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece, Int32 bonus, Int32 base) noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contHistSubtable != nullptr) {
            (*stack[rootPly - ply].contHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].update(bonus, base);
        }
    }

    void updateCaptHist(Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        captHistTable_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].update(bonus * CAPTURE_HIST_UPDATE_WEIGHT / 1024);
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

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].move : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        correction += CONT1_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 1, prevMove, prevPiece);
        correction += CONT2_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 2, prevMove, prevPiece);
        correction += CONT4_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 4, prevMove, prevPiece);
        correction += CONT6_CORR_HIST_WEIGHT * contCorrHistScore(stack, rootPly, 6, prevMove, prevPiece);

        return correction / 1024;
    }

    void updateCorrHist(const Position &position, std::span<const HistoryStackEntry> stack, USize rootPly, Int32 fdepth, Int32 searchScore, Int32 staticEval) noexcept {
        const Color color = position.sideToMove();

        const Int32 bonus = std::clamp((searchScore - staticEval) * fdepth / CORR_HIST_BONUS_FDEPTH_DIVISOR, -CORR_HIST_PENALTY_MAX, CORR_HIST_BONUS_MAX);

        updatePawnCorrHist(position, color, bonus);
        updateFriendlyNonPawnCorrHist(position, color, bonus);
        updateEnemyNonPawnCorrHist(position, color, bonus);
        updateMinorPieceCorrHist(position, color, bonus);
        updateMajorPieceCorrHist(position, color, bonus);

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].move : Move::NULL_MOVE;
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

    void updatePawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        pawnCorrHistTable_[static_cast<USize>(color)][position.pawnHash() % CORR_HIST_SIZE].update(bonus);
    }

    void updateFriendlyNonPawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(color)][position.nonPawnHash(color) % CORR_HIST_SIZE].update(bonus);
    }

    void updateEnemyNonPawnCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        nonPawnCorrHistTable_[static_cast<USize>(color)][static_cast<USize>(~color)][position.nonPawnHash(~color) % CORR_HIST_SIZE].update(bonus);
    }

    void updateMinorPieceCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        minorPieceCorrHistTable_[static_cast<USize>(color)][position.minorPieceHash() % CORR_HIST_SIZE].update(bonus);
    }

    void updateMajorPieceCorrHist(const Position &position, Color color, Int32 bonus) noexcept {
        majorPieceCorrHistTable_[static_cast<USize>(color)][position.majorPieceHash() % CORR_HIST_SIZE].update(bonus);
    }

    void updateContCorrHist(std::span<const HistoryStackEntry> stack, USize rootPly, USize ply, Move move, Piece movedPiece, Int32 bonus) noexcept {
        if (rootPly >= ply && stack[rootPly - ply].contCorrHistSubtable != nullptr) {
            (*stack[rootPly - ply].contCorrHistSubtable)[static_cast<USize>(movedPiece)][move.to().index()].update(bonus);
        }
    }
};

}
