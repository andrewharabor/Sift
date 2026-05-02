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
#include "utils.hpp"


namespace Syft {

class HistoryEntry {
public:
    HistoryEntry() noexcept : value_(0) {}
    HistoryEntry(Int32 value) noexcept : value_(value) {}

    constexpr void update(Int32 bonus) noexcept {
        update(bonus, value_);
    }

    constexpr void update(Int32 bonus, Int32 base) noexcept {
        Int32 newValue = value_ + bonus - base * std::abs(bonus) / MAX;
        value_ = std::clamp(newValue, MIN, MAX);
    }

    constexpr Int32 value() const noexcept {
        return value_;
    }

private:
    static constexpr Int32 MAX = 16384;
    static constexpr Int32 MIN = -MAX;

    Int32 value_;
};

using ContHistoryEntry = MultiArray<HistoryEntry, 12, 64>;

class CorrHistoryEntry {
public:
    static constexpr Int32 SCALE = 256;

    CorrHistoryEntry() noexcept : value_(0) {}
    CorrHistoryEntry(Int32 value) noexcept : value_(value) {}

    constexpr void update(Int32 target, Int32 weight) noexcept {
        Int32 newValue = (value_ * (SCALE - weight) + target * weight) / SCALE;
        newValue = std::clamp(newValue, value_ - MAX_UPDATE, value_ + MAX_UPDATE);
        value_ = std::clamp(newValue, MIN, MAX);
    }

    constexpr Int32 value() const noexcept {
        return value_;
    }

private:
    static constexpr Int32 MAX = 8091;
    static constexpr Int32 MIN = -MAX;
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

    Int32 correct(const Position &position, Int32 staticEval, USize rootPly) const noexcept {
        if (staticEval >= Score::KNOWN_WIN || staticEval <= Score::KNOWN_LOSS) {
            return staticEval;
        }

        const Color color = position.sideToMove();
        const UInt64 threatsHash = Utils::murmurHash3((position.threats() & position.friendly(color)).bits());

        const Int32 pawnEntry = pawnCorr_.entry(color, position.pawnHash()).value();
        const Int32 friendlyNonPawnEntry = nonPawnCorr_[static_cast<USize>(color)].entry(color, position.nonPawnHash(color)).value();
        const Int32 enemyNonPawnEntry = nonPawnCorr_[static_cast<USize>(color)].entry(~color, position.nonPawnHash(~color)).value();
        const Int32 threatsEntry = threatsCorr_.entry(color, threatsHash).value();
        const Int32 minorPieceEntry = minorPieceCorr_.entry(color, position.minorPieceHash()).value();
        const Int32 majorPieceEntry = majorPieceCorr_.entry(color, position.majorPieceHash()).value();

        Int32 correction = 0;
        correction += PAWN_CORR_WEIGHT * pawnEntry;
        correction += FRIENDLY_NONPAWN_CORR_WEIGHT * friendlyNonPawnEntry;
        correction += ENEMY_NONPAWN_CORR_WEIGHT * enemyNonPawnEntry;
        correction += THREATS_CORR_WEIGHT * threatsEntry;
        correction += MINOR_PIECE_CORR_WEIGHT * minorPieceEntry;
        correction += MAJOR_PIECE_CORR_WEIGHT * majorPieceEntry;

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        for (USize ply = MIN_CONT_CORR_PLY; ply <= MAX_CONT_CORR_PLY; ply++) {
            if (rootPly >= ply && stack[rootPly - ply].contCorrEntry != nullptr) {
                Int32 contCorrEntry = (*stack[rootPly - ply].contCorrEntry)[static_cast<USize>(prevPiece)][prevMove.to().index()].value();
                correction += CONT_CORR_WEIGHTS[ply] * contCorrEntry;
            }
        }

        Int32 correctedEval = staticEval + correction / (CorrHistoryEntry::SCALE * CorrHistoryEntry::SCALE);
        return std::clamp(correctedEval, Score::KNOWN_LOSS + 1, Score::KNOWN_WIN - 1);
    }

    void updateCorr(const Position &position, Int32 depth, USize rootPly, Int32 bonus) noexcept {
        const Color color = position.sideToMove();
        const UInt64 threatsHash = Utils::murmurHash3((position.threats() & position.friendly(color)).bits());
        const Int32 scaledBonus = bonus * CorrHistoryEntry::SCALE;
        const Int32 weight = CORR_DEPTH_WEIGHT_SCALE * std::min(1 + depth, CORR_DEPTH_WEIGHT_MAX);

        pawnCorr_.entry(color, position.pawnHash()).update(scaledBonus, weight);
        nonPawnCorr_[static_cast<USize>(color)].entry(color, position.nonPawnHash(color)).update(scaledBonus, weight);
        nonPawnCorr_[static_cast<USize>(color)].entry(~color, position.nonPawnHash(~color)).update(scaledBonus, weight);
        threatsCorr_.entry(color, threatsHash).update(scaledBonus, weight);
        minorPieceCorr_.entry(color, position.minorPieceHash()).update(scaledBonus, weight);
        majorPieceCorr_.entry(color, position.majorPieceHash()).update(scaledBonus, weight);

        const Move prevMove = (rootPly > 0) ? stack[rootPly - 1].playedMove : Move::NULL_MOVE;
        Piece prevPiece = (rootPly > 0) ? stack[rootPly - 1].movedPiece : Piece::NONE;
        if (prevPiece == Piece::NONE) {
            prevPiece = Piece(PieceType::PAWN, ~color);
        }

        for (USize ply = MIN_CONT_CORR_PLY; ply <= MAX_CONT_CORR_PLY; ply++) {
            if (rootPly >= ply && stack[rootPly - ply].contCorrEntry != nullptr) {
                (*stack[rootPly - ply].contCorrEntry)[static_cast<USize>(prevPiece)][prevMove.to().index()].update(scaledBonus, weight);
            }
        }
    }

    Int32 quietStats(const Position &position, const Move move, USize rootPly) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 score = main(move, position.threats(), movedPiece.color());
        score += pawn(move, movedPiece, position.pawnHash());
        if (rootPly > 0 && stack[rootPly - 1].contEntry != nullptr) {
            score += cont(*stack[rootPly - 1].contEntry, move, movedPiece);
        }
        if (rootPly > 1 && stack[rootPly - 2].contEntry != nullptr) {
            score += cont(*stack[rootPly - 2].contEntry, move, movedPiece);
        }
        if (rootPly > 3 && stack[rootPly - 4].contEntry != nullptr) {
            score += cont(*stack[rootPly - 4].contEntry, move, movedPiece);
        }
        return score;
    }

    Int32 noisyStats(const Position &position, const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return capture(move, position.threats(), position.moved(move), position.captured(move));
    }

    void updateQuietStats(const Position &position, const Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateMain(move, position.threats(), position.sideToMove(), bonus);
        updatePawn(move, position.moved(move), position.pawnHash(), bonus);
        updateCont(position, move, rootPly, bonus);
    }

    void updateCont(const Position &position, const Move move, USize rootPly, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        Int32 base = 0;
        base += main(move, position.threats(), movedPiece.color()) / 2;
        if (rootPly > 0 && stack[rootPly - 1].contEntry != nullptr) {
            base += cont(*stack[rootPly - 1].contEntry, move, movedPiece);
        }
        if (rootPly > 1 && stack[rootPly - 2].contEntry != nullptr) {
            base += cont(*stack[rootPly - 2].contEntry, move, movedPiece);
        }
        if (rootPly > 3 && stack[rootPly - 4].contEntry != nullptr) {
            base += cont(*stack[rootPly - 4].contEntry, move, movedPiece);
        }

        if (rootPly > 0 && stack[rootPly - 1].contEntry != nullptr) {
            updateCont(*stack[rootPly - 1].contEntry, move, movedPiece, bonus, base);
        }
        if (rootPly > 1 && stack[rootPly - 2].contEntry != nullptr) {
            updateCont(*stack[rootPly - 2].contEntry, move, movedPiece, bonus, base);
        }
        if (rootPly > 3 && stack[rootPly - 4].contEntry != nullptr) {
            updateCont(*stack[rootPly - 4].contEntry, move, movedPiece, bonus, base);
        }
    }

    void updateNoisyStats(const Position &position, const Move move, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        updateCapture(move, position.threats(), position.moved(move), position.captured(move), bonus);
    }

    constexpr ContHistoryEntry &contEntry(const Position &position, const Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return cont_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContHistoryEntry &contEntry(const Position &position, const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        const Piece movedPiece = position.moved(move);
        return cont_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr ContCorrHistoryEntry &contCorrEntry(const Position &position, const Move move) noexcept {
        if (move == Move::NULL_MOVE) {
            return contCorr_[static_cast<USize>(Piece(PieceType::PAWN, position.sideToMove()))][static_cast<USize>(move.to())];
        }
        const Piece movedPiece = position.moved(move);
        return contCorr_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

    constexpr const ContCorrHistoryEntry &contCorrEntry(const Position &position, const Move move) const noexcept {
        if (move == Move::NULL_MOVE) {
            return contCorr_[static_cast<USize>(Piece(PieceType::PAWN, position.sideToMove()))][static_cast<USize>(move.to())];
        }
        const Piece movedPiece = position.moved(move);
        return contCorr_[static_cast<USize>(movedPiece)][static_cast<USize>(move.to())];
    }

private:
    static constexpr Int32 CORR_DEPTH_WEIGHT_SCALE = 2;
    static constexpr Int32 CORR_DEPTH_WEIGHT_MAX = 16;

    static constexpr Int32 PAWN_CORR_WEIGHT = 418;
    static constexpr Int32 FRIENDLY_NONPAWN_CORR_WEIGHT = 387;
    static constexpr Int32 ENEMY_NONPAWN_CORR_WEIGHT = 273;
    static constexpr Int32 THREATS_CORR_WEIGHT = 228;
    static constexpr Int32 MINOR_PIECE_CORR_WEIGHT = 266;
    static constexpr Int32 MAJOR_PIECE_CORR_WEIGHT = 404;

    static constexpr Int32 MIN_CONT_CORR_PLY = 2;
    static constexpr Int32 MAX_CONT_CORR_PLY = 7;
    static constexpr Int32 CONT_CORR_WEIGHTS[MAX_CONT_CORR_PLY + 1] = {0, 0, 349, 175, 230, 219, 163, 142};


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
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return main_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].value();
    }

    constexpr Int32 pawn(const Move move, Piece movedPiece, UInt64 hash) const noexcept {
        assert(move != Move::NULL_MOVE);
        return pawn_[hash % pawn_.size()][static_cast<USize>(movedPiece)][move.to().index()].value();
    }

    constexpr Int32 cont(const ContHistoryEntry &contEntry, const Move move, Piece movedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        return contEntry[static_cast<USize>(movedPiece)][move.to().index()].value();
    }

    constexpr Int32 capture(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece) const noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        return capture_[static_cast<USize>(capturedPiece.type())][static_cast<USize>(movedPiece)][move.to().index()][fromThreat][toThreat].value();
    }

    constexpr void updateMain(const Move move, Bitboard threats, Color color, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
        main_[static_cast<USize>(color)][move.fromTo()][fromThreat][toThreat].update(bonus);
    }

    constexpr void updatePawn(const Move move, Piece movedPiece, UInt64 hash, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        pawn_[hash % pawn_.size()][static_cast<USize>(movedPiece)][move.to().index()].update(bonus);
    }

    void updateCont(ContHistoryEntry &contEntry, const Move move, Piece movedPiece, Int32 bonus, Int32 base) noexcept {
        assert(move != Move::NULL_MOVE);
        contEntry[static_cast<USize>(movedPiece)][move.to().index()].update(bonus, base);
    }

    void updateCapture(const Move move, Bitboard threats, Piece movedPiece, Piece capturedPiece, Int32 bonus) noexcept {
        assert(move != Move::NULL_MOVE);
        const bool fromThreat = threats.get(move.from().index());
        const bool toThreat = threats.get(move.to().index());
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
