#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

#include "color.hpp"
#include "bitboard.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Syft {

class HistoryEntry {
public:
    HistoryEntry() noexcept : value_(0) {}
    HistoryEntry(Int32 value) noexcept : value_(value) {}

    constexpr operator Int32() const noexcept { return value_; }

    constexpr void update(Int32 bonus) noexcept {
        update(bonus, value_);
    }

    constexpr void update(Int32 bonus, Int32 base) noexcept {
        Int32 newValue = value_ + bonus - base * std::abs(bonus) / MAX;
        value_ = std::clamp(newValue, MIN, MAX);
    }

private:
    static constexpr Int32 MAX = 16384;
    static constexpr Int32 MIN = -MAX;

    Int32 value_;
};

using ContHistoryEntry = MultiArray<HistoryEntry, 12, 64>;

class CorrHistoryEntry {
public:
    CorrHistoryEntry() noexcept : value_(0) {}
    CorrHistoryEntry(Int32 value) noexcept : value_(value) {}

    constexpr operator Int32() const noexcept { return value_; }

    constexpr void update(Int32 target, Int32 weight) noexcept {
        Int32 newValue = (value_ * (SCALE - weight) + target * weight) / SCALE;
        newValue = std::clamp(newValue, value_ - MAX_UPDATE, value_ + MAX_UPDATE);
        value_ = std::clamp(newValue, MIN, MAX);
    }

private:
    static constexpr Int32 MAX = 8091;
    static constexpr Int32 MIN = -MAX;
    static constexpr Int32 SCALE = 256;
    static constexpr Int32 MAX_UPDATE = 2009;

    Int32 value_;
};

using ContCorrHistoryEntry = MultiArray<CorrHistoryEntry, 12, 64>;

using MainHistory = MultiArray<HistoryEntry, 2, 4096, 2, 2>;
using PawnHistory = MultiArray<HistoryEntry, 512, 12, 64>;
using ContHistory = MultiArray<ContHistoryEntry, 12, 64>;
using CaptureHistory = MultiArray<HistoryEntry, 7, 12, 64, 2, 2>;

class CorrHistory {
public:
    CorrHistory() noexcept : data() {}

    CorrHistoryEntry &entry(Color color, UInt64 key) noexcept {
        return data[static_cast<USize>(color)][key % SIZE];
    }

    const CorrHistoryEntry &entry(Color color, UInt64 key) const noexcept {
        return data[static_cast<USize>(color)][key % SIZE];
    }

private:
    static constexpr USize SIZE = 16384;

    MultiArray<CorrHistoryEntry, 2, SIZE> data;
};

using PawnCorrHistory = CorrHistory;
using NonPawnCorrHistory = std::array<CorrHistory, 2>;
using ThreatsCorrHistory = CorrHistory;
using MinorPieceCorrHistory = CorrHistory;
using MajorPieceCorrHistory = CorrHistory;
using ContCorrHistory = MultiArray<ContCorrHistoryEntry, 12, 64>;

struct HistoryStack {
    Move playedMove;
    Piece movedPiece;

    ContCorrHistoryEntry *contCorrEntry;
    ContHistoryEntry *contEntry;

    Int32 score;
};

class History {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    std::array<HistoryStack, MAX_PLY + 1> stack;

    History() noexcept : main_(), pawn_(), cont_(), capture_(), pawnCorr_(), nonPawnCorr_(), threatsCorr_(), minorPieceCorr_(), majorPieceCorr_(), contCorr_() { reset(); }

    void reset() noexcept {
        std::memset(&main_, 0, sizeof(main_));
        std::memset(&pawn_, 0, sizeof(pawn_));
        std::memset(&cont_, 0, sizeof(cont_));
        std::memset(&capture_, 0, sizeof(capture_));
        std::memset(&pawnCorr_, 0, sizeof(pawnCorr_));
        std::memset(&nonPawnCorr_, 0, sizeof(nonPawnCorr_));
        std::memset(&threatsCorr_, 0, sizeof(threatsCorr_));
        std::memset(&minorPieceCorr_, 0, sizeof(minorPieceCorr_));
        std::memset(&majorPieceCorr_, 0, sizeof(majorPieceCorr_));
        std::memset(&contCorr_, 0, sizeof(contCorr_));

        for (USize i = 0; i <= MAX_PLY; i++) {
            stack[i].playedMove = Move::NULL_MOVE;
            stack[i].movedPiece = Piece::NONE;
            stack[i].contCorrEntry = nullptr;
            stack[i].contEntry = nullptr;
            stack[i].score = 0;
        }
    }

    constexpr ContHistoryEntry &contEntry(const Move move, Piece movedPiece) noexcept {
        assert(move != Move::NULL_MOVE);
        return cont_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContHistoryEntry &contEntry(const Move move, Piece movedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        return cont_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr ContCorrHistoryEntry &contCorrEntry(const Move move, Piece movedPiece, Color color) noexcept {
        if (move == Move::NULL_MOVE) {
            return contCorr_[static_cast<USize>(Piece(PieceType::PAWN, color))][static_cast<USize>(move.to())];
        }
        return contCorr_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContCorrHistoryEntry &contCorrEntry(const Move move, Piece movedPiece, Color color) const noexcept {
        if (move == Move::NULL_MOVE) {
            return contCorr_[static_cast<USize>(Piece(PieceType::PAWN, color))][static_cast<USize>(move.to())];
        }
        return contCorr_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

private:
    static constexpr Int32 BONUS_MAX = 2036;
    static constexpr Int32 BONUS_SCALE = 64;
    static constexpr Int32 BONUS_QUADRATIC = 441;
    static constexpr Int32 BONUS_LINEAR = 219;
    static constexpr Int32 BONUS_OFFSET = 98;

    static constexpr Int32 PENALTY_MAX = 1093;
    static constexpr Int32 PENALTY_SCALE = 64;
    static constexpr Int32 PENALTY_QUADRATIC = 292;
    static constexpr Int32 PENALTY_LINEAR = 302;
    static constexpr Int32 PENALTY_OFFSET = 27;

    MainHistory main_;
    PawnHistory pawn_;
    ContHistory cont_;
    CaptureHistory capture_;
    PawnCorrHistory pawnCorr_;
    NonPawnCorrHistory nonPawnCorr_;
    ThreatsCorrHistory threatsCorr_;
    MinorPieceCorrHistory minorPieceCorr_;
    MajorPieceCorrHistory majorPieceCorr_;
    ContCorrHistory contCorr_;

    constexpr Int32 main(const Move move, Bitboard threats, Color color) const noexcept {
        assert(move != Move::NULL_MOVE);
        bool fromThreat = threats.get(move.from().index());
        bool toThreat = threats.get(move.to().index());
        return main_[static_cast<USize>(color)][move.internal() & 4095][fromThreat][toThreat];
    }

    constexpr Int32 pawn(const Move move, Piece movedPiece, UInt64 hash) const noexcept {
        assert(move != Move::NULL_MOVE);
        return pawn_[hash % pawn_.size()][static_cast<USize>(movedPiece)][move.to().index()];
    }

    constexpr Int32 cont(const ContHistoryEntry &contEntry, const Move move, Piece movedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        return contEntry[static_cast<USize>(movedPiece)][move.to().index()];
    }

    constexpr Int32 capture(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        bool fromThreat = threats.get(move.from().index());
        bool toThreat = threats.get(move.to().index());
        return capture_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat];
    }

    constexpr void updateMain(const Move move, Bitboard threats, Color color, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        bool fromThreat = threats.get(move.from().index());
        bool toThreat = threats.get(move.to().index());
        main_[static_cast<USize>(color)][move.internal() & 4095][fromThreat][toThreat].update(bonus);
    }

    constexpr void updatePawn(const Move move, Piece movedPiece, UInt64 key, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        pawn_[key % pawn_.size()][static_cast<USize>(movedPiece)][move.to().index()].update(bonus);
    }

    void updateCont(ContHistoryEntry &contEntry, const Move move, Piece movedPiece, Int32 bonus, Int32 base) noexcept {
        assert(move != Move::NULL_MOVE);
        contEntry[static_cast<USize>(movedPiece)][move.to().index()].update(bonus, base);
    }

    void updateCapture(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        bool fromThreat = threats.get(move.from().index());
        bool toThreat = threats.get(move.to().index());
        capture_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].update(bonus);
    }

    static constexpr Int32 bonus(Int32 depth) noexcept {
        Int32 result = (BONUS_QUADRATIC * depth * depth / BONUS_SCALE) + (BONUS_LINEAR * depth) - BONUS_OFFSET;
        return std::min(result, BONUS_MAX);
    }

    static constexpr Int32 penalty(Int32 depth) noexcept {
        Int32 result = (PENALTY_QUADRATIC * depth * depth / PENALTY_SCALE) + (PENALTY_LINEAR * depth) - PENALTY_OFFSET;
        return std::min(result, PENALTY_MAX);
    }

};

}
