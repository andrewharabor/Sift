#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <utility>

#include "arch.hpp"
#include "attacks.hpp"
#include "bitboard.hpp"
#include "coords.hpp"
#include "geometry.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "simd.hpp"
#include "types.hpp"

namespace Sift {

struct PSQFeature {
    Piece piece;
    Square square;

    constexpr PSQFeature() noexcept : piece(), square() {}

    constexpr PSQFeature(Piece piece, Square square) noexcept : piece(piece), square(square) {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
    }

    constexpr USize index(Color color, bool mirror) const noexcept {
        assert(color != Color::NONE);

        Square relativeSquare = (color == Color::WHITE) ? square : square.flipped();
        relativeSquare = (mirror) ? relativeSquare.mirrored() : relativeSquare;
        Piece relativePiece = (piece.color() == color) ? Piece(piece.type(), Color::WHITE) : Piece(piece.type(), Color::BLACK);
        relativePiece = (piece.type() == PieceType::KING) ? Piece::WHITE_KING : relativePiece;

        return static_cast<USize>(relativeSquare) + 64 * static_cast<USize>(relativePiece);
    }
};

struct TIFeature {
    Piece attacker;
    Square attackerSquare;
    Piece victim;
    Square victimSquare;

    constexpr TIFeature() noexcept : attacker(), attackerSquare(), victim(), victimSquare() {}

    constexpr TIFeature(Piece attacker, Square attackerSquare, Piece victim, Square victimSquare) noexcept : attacker(attacker), attackerSquare(attackerSquare), victim(victim), victimSquare(victimSquare) {
        assert(attacker != Piece::NONE);
        assert(attackerSquare != Square::NONE);
        assert(victim != Piece::NONE);
        assert(victimSquare != Square::NONE);
    }

    constexpr USize index(Color color, bool mirror) const noexcept {
        assert(color != Color::NONE);
        Piece relativeAttacker = (attacker.color() == color) ? Piece(attacker.type(), Color::WHITE) : Piece(attacker.type(), Color::BLACK);
        Piece relativeVictim = (victim.color() == color) ? Piece(victim.type(), Color::WHITE) : Piece(victim.type(), Color::BLACK);
        Square relativeAttackerSq = (color == Color::WHITE) ? attackerSquare : attackerSquare.flipped();
        relativeAttackerSq = (mirror) ? relativeAttackerSq.mirrored() : relativeAttackerSq;
        Square relativeVictimSq = (color == Color::WHITE) ? victimSquare : victimSquare.flipped();
        relativeVictimSq = (mirror) ? relativeVictimSq.mirrored() : relativeVictimSq;

        const USize forwards = (relativeAttackerSq < relativeVictimSq) ? 1 : 0;
        const USize attackIdx = ATTACK_INDICES[static_cast<USize>(relativeAttacker)][static_cast<USize>(relativeVictim)][forwards];
        const USize offset = OFFSETS.offsets[static_cast<USize>(relativeAttacker)][static_cast<USize>(relativeAttackerSq)];
        const USize pieceIdx = PIECE_INDICES[static_cast<USize>(relativeAttacker)][static_cast<USize>(relativeAttackerSq)][static_cast<USize>(relativeVictimSq)];
        return attackIdx + offset + pieceIdx;
    }

private:
    static constexpr MultiArray<Int8, 6, 6> PIECE_TARGET_MAP = {{
        {0,  1,  -1, 2,  -1, -1},
        {0,  1,  2,  3,  4,  -1},
        {0,  1,  2,  3,  -1, -1},
        {0,  1,  2,  3,  -1, -1},
        {0,  1,  2,  3,  4,  -1},
        {-1, -1, -1, -1, -1, -1}
    }};

    static constexpr std::array<USize, 6> PIECE_TARGET_COUNT = {6, 10, 8, 8, 10, 0};

    static inline MultiArray<USize, 64, 64> pieceIndices(Piece piece) noexcept {
        Attacks::init();

        MultiArray<USize, 64, 64> indices = {};

        for (UInt8 i = 0; i < 64; i++) {
            const Square from = Square(i);
            Bitboard attacks = Attacks::attacks(piece, from, Bitboard());
            for (UInt8 j = 0; j < 64; j++) {
                const Square to = Square(j);
                const Bitboard mask = attacks & (Bitboard(to).bits() - 1);
                indices[i][j] = mask.count();
            }
        }

        return indices;
    }

    static inline MultiArray<USize, 12, 64, 64> PIECE_INDICES = [] {
        MultiArray<USize, 12, 64, 64> indices = {};

        indices[static_cast<USize>(Piece::WHITE_PAWN)] = pieceIndices(Piece::WHITE_PAWN);
        indices[static_cast<USize>(Piece::BLACK_PAWN)] = pieceIndices(Piece::BLACK_PAWN);
        for (const PieceType pieceType : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            const MultiArray<USize, 64, 64> idx = pieceIndices(Piece(pieceType, Color::WHITE));
            indices[static_cast<USize>(Piece(pieceType, Color::WHITE))] = idx;
            indices[static_cast<USize>(Piece(pieceType, Color::BLACK))] = idx;
        }

        return indices;
    }();

    static inline auto OFFSETS = [] {
        Attacks::init();

        struct {
            std::array<std::pair<USize, USize>, 12> indices = {};
            MultiArray<USize, 12, 64> offsets = {};
        } offsets;

        USize offset = 0;
        for (UInt8 i = 0; i < 12; i++) {
            const Piece piece = Piece(i);
            USize pieceOffset = 0;
            for (UInt8 sq = 0; sq < 64; sq++) {
                const Square square = Square(sq);
                offsets.offsets[static_cast<USize>(piece)][sq] = pieceOffset;
                if (piece.type() != PieceType::PAWN || !(square.backRank(Color::WHITE) || square.backRank(Color::BLACK))) {
                    const Bitboard attacks = Attacks::attacks(Piece(piece.type(), ~piece.color()), square, Bitboard());
                    pieceOffset += attacks.count();
                }
            }

            offsets.indices[static_cast<USize>(piece)] = {pieceOffset, offset};
            offset += PIECE_TARGET_COUNT[static_cast<USize>(piece.type())] * pieceOffset;
        }

        return offsets;
    }();

    static inline MultiArray<USize, 12, 12, 2> ATTACK_INDICES = [] {
        MultiArray<USize, 12, 12, 2> indices = {};

        for (UInt8 i = 0; i < 12; i++) {
            for (UInt8 j = 0; j < 12; j++) {
                const Piece attacker = Piece(i);
                const Piece victim = Piece(j);
                const bool enemies = attacker.color() != victim.color();
                const Int8 map = PIECE_TARGET_MAP[static_cast<USize>(attacker.type())][static_cast<USize>(victim.type())];
                const bool semiExcluded = ((attacker.type() == victim.type()) && (enemies || attacker.type() != PieceType::PAWN));
                const bool excluded = map < 0;
                const auto [pieceOffset, offset] = OFFSETS.indices[i];
                const USize featureIndex = offset + (static_cast<USize>(victim.color()) * PIECE_TARGET_COUNT[static_cast<USize>(attacker.type())] / 2 + static_cast<USize>(map)) * pieceOffset;
                indices[i][j][0] = (excluded) ? Arch::TI_SIZE : featureIndex;
                indices[i][j][1] = (excluded || semiExcluded) ? Arch::TI_SIZE : featureIndex;
            }
        }

        return indices;
    }();
};

struct RefreshEntry {
    alignas(64) std::array<Int16, Arch::L1_SIZE> data;

    constexpr RefreshEntry() noexcept : data(), piecesBitboards(), occupancyBitboards() {}

    constexpr void init(const std::array<Int16, Arch::L1_SIZE> &biases) noexcept { data = biases; }

    constexpr void update(const MultiArray<Int16, Arch::PSQ_SIZE, Arch::L1_SIZE> &weights, const Position &position, Color color, bool mirror) noexcept {
        std::array<USize, MAX_CHANGES> add;
        std::array<USize, MAX_CHANGES> sub;
        USize addSize = 0;
        USize subSize = 0;

        for (PieceType pieceType : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            for (Color pieceColor : {Color::WHITE, Color::BLACK}) {
                const Piece piece = Piece(pieceType, pieceColor);
                const Bitboard oldOccupancy = pieces(pieceType, pieceColor);
                const Bitboard newOccupancy = position.pieces(pieceType, pieceColor);

                Bitboard addOccupancy = newOccupancy & ~oldOccupancy;
                while (addOccupancy) {
                    const Square square = Square(addOccupancy.pop());

                    assert(addSize < MAX_CHANGES);
                    add[addSize++] = PSQFeature(piece, square).index(color, mirror);
                }

                Bitboard subOccupancy = oldOccupancy & ~newOccupancy;
                while (subOccupancy) {
                    const Square square = Square(subOccupancy.pop());

                    assert(subSize < MAX_CHANGES);
                    sub[subSize++] = PSQFeature(piece, square).index(color, mirror);
                }
            }
        }

        for (PieceType pieceType : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            piecesBitboards[static_cast<USize>(pieceType)] = position.pieces(pieceType);
        }

        for (Color pieceColor : {Color::WHITE, Color::BLACK}) {
            occupancyBitboards[static_cast<USize>(pieceColor)] = position.friendly(pieceColor);
        }

        while (addSize >= 2) {
            FusedUpdates::add2(data, weights[add[addSize - 1]], weights[add[addSize - 2]]);
            addSize -= 2;
        }

        while (addSize >= 1) {
            FusedUpdates::add1(data, weights[add[addSize - 1]]);
            addSize--;
        }

        while (subSize >= 2) {
            FusedUpdates::sub2(data, weights[sub[subSize - 1]], weights[sub[subSize - 2]]);
            subSize -= 2;
        }

        while (subSize >= 1) {
            FusedUpdates::sub1(data, weights[sub[subSize - 1]]);
            subSize--;
        }
    }

private:
    static constexpr USize MAX_CHANGES = 32;

    std::array<Bitboard, 6> piecesBitboards;
    std::array<Bitboard, 2> occupancyBitboards;

    constexpr Bitboard pieces(PieceType pieceType, Color color) const noexcept {
        assert(pieceType != PieceType::NONE && color != Color::NONE);
        return piecesBitboards[static_cast<USize>(pieceType)] & occupancyBitboards[static_cast<USize>(color)];
    }
};

enum class AccState : UInt8 {
    CLEAN,
    DIRTY,
    REFRESH
};

class Accumulator {
public:
    constexpr Accumulator() noexcept : psqData_(), psqStates_({AccState::REFRESH, AccState::REFRESH}), psqAdds_(), psqSubs_(), psqAddSize_(0), psqSubSize_(0), tiData_(), tiStates_({AccState::REFRESH, AccState::REFRESH}), tiAdds_(), tiSubs_(), tiAddSize_(0), tiSubSize_(0) {}

    constexpr const std::array<Int16, Arch::L1_SIZE> &psqData(Color color) const noexcept {
        assert(color != Color::NONE);
        return psqData_[static_cast<USize>(color)];
    }

    constexpr std::array<Int16, Arch::L1_SIZE> &psqData(Color color) noexcept {
        assert(color != Color::NONE);
        return psqData_[static_cast<USize>(color)];
    }

    constexpr const std::array<Int16, Arch::L1_SIZE> &tiData(Color color) const noexcept {
        assert(color != Color::NONE);
        return tiData_[static_cast<USize>(color)];
    }

    constexpr std::array<Int16, Arch::L1_SIZE> &tiData(Color color) noexcept {
        assert(color != Color::NONE);
        return tiData_[static_cast<USize>(color)];
    }

    template<typename Function>
    inline void writeTIAdds(Function f) { tiAddSize_ += f(&tiAdds_[tiAddSize_]); }

    template<typename Function>
    inline void writeTISubs(Function f) { tiSubSize_ += f(&tiSubs_[tiSubSize_]); }

    constexpr AccState psqState(Color color) const noexcept {
        assert(color != Color::NONE);
        return psqStates_[static_cast<USize>(color)];
    }

    constexpr AccState tiState(Color color) const noexcept {
        assert(color != Color::NONE);
        return tiStates_[static_cast<USize>(color)];
    }

    constexpr void setPSQState(Color color, AccState state) noexcept {
        assert(color != Color::NONE);
        psqStates_[static_cast<USize>(color)] = state;
    }

    constexpr void setTIState(Color color, AccState state) noexcept {
        assert(color != Color::NONE);
        tiStates_[static_cast<USize>(color)] = state;
    }

    constexpr void addPSQFeature(PSQFeature feature) noexcept {
        assert(psqAddSize_ < MAX_PSQ_CHANGES);
        psqAdds_[psqAddSize_++] = feature;
    }

    constexpr void subPSQFeature(PSQFeature feature) noexcept {
        assert(psqSubSize_ < MAX_PSQ_CHANGES);
        psqSubs_[psqSubSize_++] = feature;
    }

    constexpr void addTIFeature(TIFeature feature) noexcept {
        assert(tiAddSize_ < MAX_TI_CHANGES);
        tiAdds_[tiAddSize_++] = feature;
    }

    constexpr void subTIFeature(TIFeature feature) noexcept {
        assert(tiSubSize_ < MAX_TI_CHANGES);
        tiSubs_[tiSubSize_++] = feature;
    }

    constexpr void prepareUpdates() noexcept {
        psqAdds_.fill(PSQFeature());
        psqSubs_.fill(PSQFeature());
        psqAddSize_ = 0;
        psqSubSize_ = 0;
        tiAdds_.fill(TIFeature());
        tiSubs_.fill(TIFeature());
        tiAddSize_ = 0;
        tiSubSize_ = 0;
        setPSQState(Color::WHITE, AccState::DIRTY);
        setPSQState(Color::BLACK, AccState::DIRTY);
        setTIState(Color::WHITE, AccState::DIRTY);
        setTIState(Color::BLACK, AccState::DIRTY);
    }

    constexpr void updatePSQFeatures(const Accumulator &previous, const MultiArray<Int16, Arch::PSQ_SIZE, Arch::L1_SIZE> &weights, Color color, bool mirror) noexcept {
        assert(psqState(color) == AccState::DIRTY);
        assert(previous.psqState(color) == AccState::CLEAN);
        assert(psqAddSize_ >= 1);
        assert(psqSubSize_ >= 1);
        assert(color != Color::NONE);

        psqData(color) = previous.psqData(color);

        const USize add1 = psqAdds_[0].index(color, mirror);
        const USize add2 = (psqAddSize_ > 1) ? psqAdds_[1].index(color, mirror) : 0;
        const USize sub1 = psqSubs_[0].index(color, mirror);
        const USize sub2 = (psqSubSize_ > 1) ? psqSubs_[1].index(color, mirror) : 0;

        if (psqAddSize_ == 1 && psqSubSize_ == 1) {
            FusedUpdates::add1Sub1(psqData(color), weights[add1], weights[sub1]);
        } else if (psqAddSize_ == 1 && psqSubSize_ == 2) {
            FusedUpdates::add1Sub2(psqData(color), weights[add1], weights[sub1], weights[sub2]);
        } else if (psqAddSize_ == 2 && psqSubSize_ == 2) {
            FusedUpdates::add2Sub2(psqData(color), weights[add1], weights[add2], weights[sub1], weights[sub2]);
        }

        setPSQState(color, AccState::CLEAN);
    }

    constexpr void updateTIFeatures(const Accumulator &previous, const MultiArray<Int8, Arch::TI_SIZE, Arch::L1_SIZE> &weights, Color color, bool mirror) noexcept {
        assert(tiState(color) == AccState::DIRTY);
        assert(previous.tiState(color) == AccState::CLEAN);
        assert(color != Color::NONE);

        tiData(color) = previous.tiData(color);

        std::array<USize, MAX_TI_CHANGES> adds;
        std::array<USize, MAX_TI_CHANGES> subs;
        USize addSize = 0;
        USize subSize = 0;

        for (USize i = 0; i < tiAddSize_; i++) {
            const USize index = tiAdds_[i].index(color, mirror);
            if (index >= Arch::TI_SIZE) {
                continue;
            }
            assert(addSize < MAX_TI_CHANGES);
            adds[addSize++] = index;
        }

        for (USize i = 0; i < tiSubSize_; i++) {
            const USize index = tiSubs_[i].index(color, mirror);
            if (index >= Arch::TI_SIZE) {
                continue;
            }
            assert(subSize < MAX_TI_CHANGES);
            subs[subSize++] = index;
        }

        while (addSize >= 4) {
            FusedUpdates::castAdd4(tiData(color), weights[adds[addSize - 1]], weights[adds[addSize - 2]], weights[adds[addSize - 3]], weights[adds[addSize - 4]]);
            addSize -= 4;
        }

        while (addSize >= 1) {
            FusedUpdates::castAdd1(tiData(color), weights[adds[addSize - 1]]);
            addSize -= 1;
        }

        while (subSize >= 4) {
            FusedUpdates::castSub4(tiData(color), weights[subs[subSize - 1]], weights[subs[subSize - 2]], weights[subs[subSize - 3]], weights[subs[subSize - 4]]);
            subSize -= 4;
        }

        while (subSize >= 1) {
            FusedUpdates::castSub1(tiData(color), weights[subs[subSize - 1]]);
            subSize -= 1;
        }

        setTIState(color, AccState::CLEAN);
    }

    constexpr void refreshPSQFeatures(const RefreshEntry &refreshEntry, Color color) noexcept {
        assert(psqState(color) == AccState::REFRESH);
        assert(color != Color::NONE);

        psqData(color) = refreshEntry.data;
        setPSQState(color, AccState::CLEAN);
    }

    constexpr void refreshTIFeatures(const MultiArray<Int8, Arch::TI_SIZE, Arch::L1_SIZE> &weights, const Position &position, Color color, bool mirror) {
        assert(tiState(color) == AccState::REFRESH);
        assert(color != Color::NONE);

        tiData(color).fill(0);

        std::array<USize, MAX_TI_CHANGES> features;
        USize size = 0;

        const Bitboard occupied = position.occupied();
        const Bitboard nonKings = occupied ^ position.pieces(PieceType::KING);
        Bitboard attackers = nonKings;
        while (attackers) {
            const Square from = Square(attackers.pop());
            const Piece attacker = position.pieceAt(from);
            Bitboard victims = nonKings & Attacks::attacks(attacker, from, occupied);
            while (victims) {
                const Square to = Square(victims.pop());
                const Piece victim = position.pieceAt(to);
                const USize index = TIFeature(attacker, from, victim, to).index(color, mirror);
                if (index < Arch::TI_SIZE) {
                    features[size++] = index;
                }
            }
        }

        while (size >= 4) {
            FusedUpdates::castAdd4(tiData(color), weights[features[size - 1]], weights[features[size - 2]], weights[features[size - 3]], weights[features[size - 4]]);
            size -= 4;
        }

        while (size >= 1) {
            FusedUpdates::castAdd1(tiData(color), weights[features[size - 1]]);
            size -= 1;
        }

        setTIState(color, AccState::CLEAN);
    }

private:
    static constexpr USize MAX_PSQ_CHANGES = 2;
    static constexpr USize MAX_TI_CHANGES = 128;

    alignas(64) MultiArray<Int16, 2, Arch::L1_SIZE> psqData_;
    std::array<AccState, 2> psqStates_;
    std::array<PSQFeature, MAX_PSQ_CHANGES> psqAdds_;
    std::array<PSQFeature, MAX_PSQ_CHANGES> psqSubs_;
    USize psqAddSize_;
    USize psqSubSize_;

    alignas(64) MultiArray<Int16, 2, Arch::L1_SIZE> tiData_;
    std::array<AccState, 2> tiStates_;
    std::array<TIFeature, MAX_TI_CHANGES> tiAdds_;
    std::array<TIFeature, MAX_TI_CHANGES> tiSubs_;
    USize tiAddSize_;
    USize tiSubSize_;
};


class NNUEState {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    NNUEState(const MultiArray<Int16, Arch::KING_BUCKETS, Arch::PSQ_SIZE, Arch::L1_SIZE> &psqWeights, const MultiArray<Int8, Arch::TI_SIZE, Arch::L1_SIZE> &tiWeights, const std::array<Int16, Arch::L1_SIZE> &l0Biases) :psqWeights_(psqWeights), tiWeights_(tiWeights), accumulators_(), ply_(0), refreshTable_() {
        for (Color color : {Color::WHITE, Color::BLACK}) {
            for (bool mirror : {false, true}) {
                for (USize kBucket = 0; kBucket < Arch::KING_BUCKETS; kBucket++) {
                    refreshTable_[static_cast<USize>(color)][mirror][kBucket].init(l0Biases);
                }
            }
        }
    }

    void set(const Position &position) noexcept {
        ply_ = 0;
        for (Color color : {Color::WHITE, Color::BLACK}) {
            const bool mirr = mirror(position.kingSquare(color));
            const USize kBucket = kingBucket(position.kingSquare(color), color);
            RefreshEntry &refreshEntry = refreshTable_[static_cast<USize>(color)][mirr][kBucket];
            refreshEntry.update(psqWeights_[kBucket], position, color, mirr);
            accumulators_[ply_].setPSQState(color, AccState::REFRESH);
            accumulators_[ply_].setTIState(color, AccState::REFRESH);
            accumulators_[ply_].refreshPSQFeatures(refreshEntry, color);
            accumulators_[ply_].refreshTIFeatures(tiWeights_, position, color, mirr);
        }
    }

    constexpr const Accumulator &topAccumulator(const Position &position) noexcept {
        update(position, Color::WHITE);
        update(position, Color::BLACK);

        assert(accumulators_[ply_].psqState(Color::WHITE) == AccState::CLEAN);
        assert(accumulators_[ply_].psqState(Color::BLACK) == AccState::CLEAN);
        assert(accumulators_[ply_].tiState(Color::WHITE) == AccState::CLEAN);
        assert(accumulators_[ply_].tiState(Color::BLACK) == AccState::CLEAN);

        return accumulators_[ply_];
    }

    constexpr void prepareUpdates() noexcept {
        assert(ply_ < MAX_PLY - 1);
        ply_++;
        accumulators_[ply_].prepareUpdates();
    }

    void addPiece(const Position &position, Piece piece, Square square) noexcept {
        accumulators_[ply_].addPSQFeature(PSQFeature(piece, square));
        changePiece<true>(position, piece, square);
    }

    void removePiece(const Position &position, Piece piece, Square square) noexcept {
        accumulators_[ply_].subPSQFeature(PSQFeature(piece, square));
        changePiece<false>(position, piece, square);
    }

    void movePiece(const Position &position, Piece fromPiece, Piece toPiece, Square fromSquare, Square toSquare) noexcept {
        accumulators_[ply_].subPSQFeature(PSQFeature(fromPiece, fromSquare));
        accumulators_[ply_].addPSQFeature(PSQFeature(toPiece, toSquare));

#if defined(USE_SIMD)
        const Permutation fromPerm = Geometry::permutation(fromSquare);
        const Permutation toPerm = Geometry::permutation(toSquare);
        const auto [fromRays, fromBits] = Geometry::permuteMailbox(fromPerm, position.mailbox(), toSquare);
        const auto [toRays, toBits] = Geometry::permuteMailbox(toPerm, position.mailbox());
        const BitRays fromClosest = Geometry::closestOccupied(fromBits);
        const BitRays toClosest = Geometry::closestOccupied(toBits);
        const BitRays fromOutgoing = Geometry::outgoingThreats(fromPiece, fromClosest);
        const BitRays toOutgoing = Geometry::outgoingThreats(toPiece, toClosest);
        const BitRays fromIncomingAttackers = Geometry::incomingAttackers(fromBits, fromClosest);
        const BitRays toIncomingAttackers = Geometry::incomingAttackers(toBits, toClosest);
        const BitRays fromIncomingSliders = Geometry::incomingSliders(fromBits, fromClosest);
        const BitRays toIncomingSliders = Geometry::incomingSliders(toBits, toClosest);
        const BitRays fromVictimMask = std::rotr(fromClosest & 0xFEFEFEFEFEFEFEFE, 32);
        const BitRays toVictimMask = std::rotr(toClosest & 0xFEFEFEFEFEFEFEFE, 32);
        const BitRays fromValid = Geometry::rayFill(fromVictimMask) & Geometry::rayFill(fromIncomingSliders);
        const BitRays toValid = Geometry::rayFill(toVictimMask) & Geometry::rayFill(toIncomingSliders);

        pushDirectTIFeatures<false, true>(fromPerm.indices, fromRays, fromOutgoing, fromPiece, fromSquare);
        pushDirectTIFeatures<false, false>(fromPerm.indices, fromRays, fromIncomingAttackers, fromPiece, fromSquare);
        pushDirectTIFeatures<true, true>(toPerm.indices, toRays, toOutgoing, toPiece, toSquare);
        pushDirectTIFeatures<true, false>(toPerm.indices, toRays, toIncomingAttackers, toPiece, toSquare);
        pushXRayTIFeatures<false>(fromPerm.indices, fromRays, fromIncomingSliders & fromValid, fromVictimMask & fromValid);
        pushXRayTIFeatures<true>(toPerm.indices, toRays, toIncomingSliders & toValid, toVictimMask & toValid);
#else
        const Bitboard occupied = position.occupied();
        const Bitboard xRayOccupied = occupied ^ Bitboard(toSquare);

        Bitboard fromOutgoing = Attacks::attacks(fromPiece, fromSquare, xRayOccupied) & xRayOccupied;
        Bitboard toOutgoing = Attacks::attacks(toPiece, toSquare, occupied) & occupied;
        while (fromOutgoing) {
            const Square otherSquare = Square(fromOutgoing.pop());
            const Piece other = position.pieceAt(otherSquare);
            const TIFeature feature = TIFeature(fromPiece, fromSquare, other, otherSquare);
            accumulators_[ply_].subTIFeature(feature);
        }
        while (toOutgoing) {
            const Square otherSquare = Square(toOutgoing.pop());
            const Piece other = position.pieceAt(otherSquare);
            const TIFeature feature = TIFeature(toPiece, toSquare, other, otherSquare);
            accumulators_[ply_].addTIFeature(feature);
        }

        for (const Piece pc : {Piece::WHITE_PAWN, Piece::BLACK_PAWN, Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK}) {
            bool slider = false;
            Bitboard incoming = Attacks::attacks(Piece(pc.type(), ~pc.color()), fromSquare, xRayOccupied) & xRayOccupied;
            const Bitboard potentialVictims = incoming;
            if (pc.type() == PieceType::PAWN) {
                incoming &= position.pieces(pc);
            } else if (pc.type() == PieceType::KNIGHT) {
                incoming &= position.pieces(pc.type());
            } else {
                incoming &= position.pieces(pc.type()) | position.pieces(PieceType::QUEEN);
                slider = true;
            }

            while (incoming) {
                const Square otherSquare = Square(incoming.pop());
                const Piece other = position.pieceAt(otherSquare);
                const TIFeature feature = TIFeature(other, otherSquare, fromPiece, fromSquare);
                accumulators_[ply_].subTIFeature(feature);

                if (!slider) {
                    continue;
                }

                Bitboard victims = Attacks::attacks(pc, otherSquare, Bitboard()) & (potentialVictims ^ Bitboard(otherSquare));
                if (victims) {
                    const Square victimSquare = Square(victims.pop());
                    const Piece victim = position.pieceAt(victimSquare);
                    const TIFeature xRayFeature = TIFeature(other, otherSquare, victim, victimSquare);
                    accumulators_[ply_].addTIFeature(xRayFeature);
                }
            }
        }

        for (const Piece pc : {Piece::WHITE_PAWN, Piece::BLACK_PAWN, Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK}) {
            bool slider = false;
            Bitboard incoming = Attacks::attacks(Piece(pc.type(), ~pc.color()), toSquare, occupied) & occupied;
            const Bitboard potentialVictims = incoming;
            if (pc.type() == PieceType::PAWN) {
                incoming &= position.pieces(pc);
            } else if (pc.type() == PieceType::KNIGHT) {
                incoming &= position.pieces(pc.type());
            } else {
                incoming &= position.pieces(pc.type()) | position.pieces(PieceType::QUEEN);
                slider = true;
            }

            while (incoming) {
                const Square otherSquare = Square(incoming.pop());
                const Piece other = position.pieceAt(otherSquare);
                const TIFeature feature = TIFeature(other, otherSquare, toPiece, toSquare);
                accumulators_[ply_].addTIFeature(feature);

                if (!slider) {
                    continue;
                }

                Bitboard victims = Attacks::attacks(pc, otherSquare, Bitboard()) & (potentialVictims ^ Bitboard(otherSquare));
                if (victims) {
                    const Square victimSquare = Square(victims.pop());
                    const Piece victim = position.pieceAt(victimSquare);
                    const TIFeature xRayFeature = TIFeature(other, otherSquare, victim, victimSquare);
                    accumulators_[ply_].subTIFeature(xRayFeature);
                }
            }
        }
#endif
    }

    void transmutePiece(const Position &position, Piece oldPiece, Piece newPiece, Square square) noexcept {
        accumulators_[ply_].subPSQFeature(PSQFeature(oldPiece, square));
        accumulators_[ply_].addPSQFeature(PSQFeature(newPiece, square));

#if defined(USE_SIMD)
        const Permutation perm = Geometry::permutation(square);
        const auto [rays, bits] = Geometry::permuteMailbox(perm, position.mailbox());
        const BitRays closest = Geometry::closestOccupied(bits);
        const BitRays oldOutgoing = Geometry::outgoingThreats(oldPiece, closest);
        const BitRays newOutgoing = Geometry::outgoingThreats(newPiece, closest);
        const BitRays incomingAttackers = Geometry::incomingAttackers(bits, closest);

        pushDirectTIFeatures<false, true>(perm.indices, rays, oldOutgoing, oldPiece, square);
        pushDirectTIFeatures<false, false>(perm.indices, rays, incomingAttackers, oldPiece, square);
        pushDirectTIFeatures<true, true>(perm.indices, rays, newOutgoing, newPiece, square);
        pushDirectTIFeatures<true, false>(perm.indices, rays, incomingAttackers, newPiece, square);
#else
        const Bitboard occupied = position.occupied();

        Bitboard oldOutgoing = Attacks::attacks(oldPiece, square, occupied) & occupied;
        Bitboard newOutgoing = Attacks::attacks(newPiece, square, occupied) & occupied;
        while (oldOutgoing) {
            const Square otherSquare = Square(oldOutgoing.pop());
            const Piece other = position.pieceAt(otherSquare);
            const TIFeature feature = TIFeature(oldPiece, square, other, otherSquare);
            accumulators_[ply_].subTIFeature(feature);
        }
        while (newOutgoing) {
            const Square otherSquare = Square(newOutgoing.pop());
            const Piece other = position.pieceAt(otherSquare);
            const TIFeature feature = TIFeature(newPiece, square, other, otherSquare);
            accumulators_[ply_].addTIFeature(feature);
        }

        for (const Piece pc : {Piece::WHITE_PAWN, Piece::BLACK_PAWN, Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK}) {
            Bitboard incoming = Attacks::attacks(Piece(pc.type(), ~pc.color()), square, occupied);
            if (pc.type() == PieceType::PAWN) {
                incoming &= position.pieces(pc);
            } else if (pc.type() == PieceType::KNIGHT) {
                incoming &= position.pieces(pc.type());
            } else {
                incoming &= position.pieces(pc.type()) | position.pieces(PieceType::QUEEN);
            }

            while (incoming) {
                const Square otherSquare = Square(incoming.pop());
                const Piece other = position.pieceAt(otherSquare);
                const TIFeature oldFeature = TIFeature(other, otherSquare, oldPiece, square);
                const TIFeature newFeature = TIFeature(other, otherSquare, newPiece, square);
                accumulators_[ply_].subTIFeature(oldFeature);
                accumulators_[ply_].addTIFeature(newFeature);
            }
        }
#endif
    }

    constexpr void checkRefresh(Square kingFrom, Square kingTo, Color color) noexcept {
        if (accumulators_[ply_ - 1].psqState(color) == AccState::REFRESH || mirror(kingFrom) != mirror(kingTo) || kingBucket(kingFrom, color) != kingBucket(kingTo, color)) {
            accumulators_[ply_].setPSQState(color, AccState::REFRESH);
        }

        if (accumulators_[ply_ - 1].tiState(color) == AccState::REFRESH || mirror(kingFrom) != mirror(kingTo)) {
            accumulators_[ply_].setTIState(color, AccState::REFRESH);
        }
    }

    constexpr void unmakeMove() noexcept {
        assert(ply_ > 0);
        ply_--;
    }

private:
    const MultiArray<Int16, Arch::KING_BUCKETS, Arch::PSQ_SIZE, Arch::L1_SIZE> &psqWeights_;
    const MultiArray<Int8, Arch::TI_SIZE, Arch::L1_SIZE> &tiWeights_;

    Accumulator accumulators_[MAX_PLY + 1];
    USize ply_;

    MultiArray<RefreshEntry, 2, 2, Arch::KING_BUCKETS> refreshTable_;

    void update(const Position &position, Color color) noexcept {
        const bool mirr = mirror(position.kingSquare(color));
        const USize kBucket = kingBucket(position.kingSquare(color), color);

        USize cleanIdx = ply_;
        while (accumulators_[cleanIdx].psqState(color) == AccState::DIRTY) {
            cleanIdx--;
        }

        if (accumulators_[cleanIdx].psqState(color) == AccState::REFRESH) {
            RefreshEntry &refreshEntry = refreshTable_[static_cast<USize>(color)][mirr][kBucket];
            refreshEntry.update(psqWeights_[kBucket], position, color, mirr);
            accumulators_[ply_].setPSQState(color, AccState::REFRESH);
            accumulators_[ply_].refreshPSQFeatures(refreshEntry, color);
        } else {
            while (cleanIdx++ < ply_) {
                accumulators_[cleanIdx].updatePSQFeatures(accumulators_[cleanIdx - 1], psqWeights_[kBucket], color, mirr);
            }
        }

        cleanIdx = ply_;
        while (accumulators_[cleanIdx].tiState(color) == AccState::DIRTY) {
            cleanIdx--;
        }

        if (accumulators_[cleanIdx].tiState(color) == AccState::REFRESH) {
            accumulators_[ply_].setTIState(color, AccState::REFRESH);
            accumulators_[ply_].refreshTIFeatures(tiWeights_, position, color, mirr);
        } else {
            while (cleanIdx++ < ply_) {
                accumulators_[cleanIdx].updateTIFeatures(accumulators_[cleanIdx - 1], tiWeights_, color, mirr);
            }
        }
    }

    template<bool ADD>
    void changePiece(const Position &position, Piece piece, Square square) noexcept {
#if defined(USE_SIMD)
        const Permutation perm = Geometry::permutation(square);
        const auto [rays, bits] = Geometry::permuteMailbox(perm, position.mailbox());
        const BitRays closest = Geometry::closestOccupied(bits);
        const BitRays outgoing = Geometry::outgoingThreats(piece, closest);
        const BitRays incomingAttackers = Geometry::incomingAttackers(bits, closest);
        const BitRays incomingSliders = Geometry::incomingSliders(bits, closest);
        const BitRays victimMask = std::rotr(closest & 0xFEFEFEFEFEFEFEFE, 32);
        const BitRays valid = Geometry::rayFill(victimMask) & Geometry::rayFill(incomingSliders);

        pushDirectTIFeatures<ADD, true>(perm.indices, rays, outgoing, piece, square);
        pushDirectTIFeatures<ADD, false>(perm.indices, rays, incomingAttackers, piece, square);
        pushXRayTIFeatures<ADD>(perm.indices, rays, incomingSliders & valid, victimMask & valid);
#else
        const Bitboard occupied = position.occupied();

        Bitboard outgoing = Attacks::attacks(piece, square, occupied) & occupied;
        while (outgoing) {
            const Square otherSquare = Square(outgoing.pop());
            const Piece other = position.pieceAt(otherSquare);
            const TIFeature feature = TIFeature(piece, square, other, otherSquare);
            if constexpr (ADD) {
                accumulators_[ply_].addTIFeature(feature);
            } else {
                accumulators_[ply_].subTIFeature(feature);
            }
        }

        for (const Piece pc : {Piece::WHITE_PAWN, Piece::BLACK_PAWN, Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK}) {
            bool slider = false;
            Bitboard incoming = Attacks::attacks(Piece(pc.type(), ~pc.color()), square, occupied) & occupied;
            const Bitboard potentialVictims = incoming;
            if (pc.type() == PieceType::PAWN) {
                incoming &= position.pieces(pc);
            } else if (pc.type() == PieceType::KNIGHT) {
                incoming &= position.pieces(pc.type());
            } else {
                incoming &= position.pieces(pc.type()) | position.pieces(PieceType::QUEEN);
                slider = true;
            }

            while (incoming) {
                const Square otherSquare = Square(incoming.pop());
                const Piece other = position.pieceAt(otherSquare);
                const TIFeature feature = TIFeature(other, otherSquare, piece, square);
                if constexpr (ADD) {
                    accumulators_[ply_].addTIFeature(feature);
                } else {
                    accumulators_[ply_].subTIFeature(feature);
                }

                if (!slider) {
                    continue;
                }

                Bitboard victims = Attacks::attacks(pc, otherSquare, Bitboard()) & (potentialVictims ^ Bitboard(otherSquare));
                if (victims) {
                    const Square victimSquare = Square(victims.pop());
                    const Piece victim = position.pieceAt(victimSquare);
                    const TIFeature xRayFeature = TIFeature(other, otherSquare, victim, victimSquare);
                    if constexpr (ADD) {
                        accumulators_[ply_].subTIFeature(xRayFeature);
                    } else {
                        accumulators_[ply_].addTIFeature(xRayFeature);
                    }
                }
            }
        }
#endif
    }

#if defined(USE_SIMD)

#if defined(USE_AVX512)

    template<bool ADD, bool OUTGOING>
    void pushDirectTIFeatures(const Vec &indices, const Vec &rays, BitRays bits, Piece piece, Square square) noexcept {
        const auto pair2Shuffle = _mm512_set_epi8(
            79, 15, 79, 15, 78, 14, 78, 14, 77, 13, 77, 13, 76, 12, 76, 12, 75, 11, 75, 11,
            74, 10, 74, 10, 73, 9, 73, 9, 72, 8, 72, 8, 71, 7, 71, 7, 70, 6, 70, 6, 69, 5,
            69, 5, 68, 4, 68, 4, 67, 3, 67, 3, 66, 2, 66, 2, 65, 1, 65, 1, 64, 0, 64, 0
        );

        const auto pair1 = _mm512_set1_epi16(static_cast<Int16>(piece.index() | (square.index() << 8)));
        const auto pair2Square = _mm512_maskz_compress_epi8(bits, indices.raw);
        const auto pair2Piece = _mm512_maskz_compress_epi8(bits, rays.raw);
        const auto pair2 = _mm512_permutex2var_epi8(pair2Piece, pair2Shuffle, pair2Square);

        constexpr UInt64 MASK = (OUTGOING) ? 0xCCCCCCCCCCCCCCCC : 0x3333333333333333;
        const auto vector = _mm512_mask_mov_epi8(pair1, MASK, pair2);

        const auto writeFeatures = [&](TIFeature *ptr) {
            _mm512_storeu_si512(ptr, vector);
            return std::popcount(bits);
        };

        if constexpr (ADD) {
            accumulators_[ply_].writeTIAdds(writeFeatures);
        } else {
            accumulators_[ply_].writeTISubs(writeFeatures);
        }
    }

    template<bool ADD>
    void pushXRayTIFeatures(const Vec &indices, const Vec &rays, BitRays sliders, BitRays victims) noexcept {
        assert(std::popcount(sliders) == std::popcount(victims));

        const auto piece1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, rays.raw));
        const auto square1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, indices.raw));
        const auto piece2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, rays.flipped().raw));
        const auto square2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, indices.flipped().raw));

        const auto pair1 = _mm_unpacklo_epi8(piece1, square1);
        const auto pair2 = _mm_unpacklo_epi8(piece2, square2);
        const auto tuple1 = _mm_unpacklo_epi16(pair1, pair2);
        const auto tuple2 = _mm_unpackhi_epi16(pair1, pair2);

        const auto writeFeatures = [&](TIFeature *ptr) {
            _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), tuple1);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr) + 1, tuple2);
            return static_cast<USize>(std::popcount(victims));
        };

        if constexpr (ADD) {
            accumulators_[ply_].writeTISubs(writeFeatures);
        } else {
            accumulators_[ply_].writeTIAdds(writeFeatures);
        }
    }

#else

    template<bool ADD, bool OUTGOING>
    void pushDirectTIFeatures(const Vec &indices, const Vec &rays, BitRays bits, Piece piece, Square square) noexcept {
        std::array<Piece, 64> pieces;
        std::array<Square, 64> squares;
        std::memcpy(pieces.data(), &rays, sizeof(pieces));
        std::memcpy(squares.data(), &indices, sizeof(squares));

        for (; bits; bits &= (bits - 1)) {
            const USize i = static_cast<USize>(std::countr_zero(bits));
            const Piece other = pieces[i];
            const Square otherSquare = squares[i];
            const Piece attacker = (OUTGOING) ? piece : other;
            const Square attackerSquare = (OUTGOING) ? square : otherSquare;
            const Piece victim = (OUTGOING) ? other : piece;
            const Square victimSquare = (OUTGOING) ? otherSquare : square;
            const TIFeature feature = TIFeature(attacker, attackerSquare, victim, victimSquare);

            if constexpr (ADD) {
                accumulators_[ply_].addTIFeature(feature);
            } else {
                accumulators_[ply_].subTIFeature(feature);
            }
        }
    }

    template<bool ADD>
    void pushXRayTIFeatures(const Vec &indices, const Vec &rays, BitRays sliders, BitRays victims) noexcept {
        std::array<Piece, 64> pieces;
        std::array<Square, 64> squares;
        std::memcpy(pieces.data(), &rays, sizeof(pieces));
        std::memcpy(squares.data(), &indices, sizeof(squares));

        for (; sliders; sliders &= (sliders - 1), victims &= (victims - 1)) {
            const USize i = static_cast<USize>(std::countr_zero(sliders));
            const USize j = static_cast<USize>((std::countr_zero(victims) + 32) % 64);
            const Piece attacker = pieces[i];
            const Square attackerSquare = squares[i];
            const Piece victim = pieces[j];
            const Square victimSquare = squares[j];
            const TIFeature feature = TIFeature(attacker, attackerSquare, victim, victimSquare);

            if constexpr (ADD) {
                accumulators_[ply_].subTIFeature(feature);
            } else {
                accumulators_[ply_].addTIFeature(feature);
            }
        }

        assert(!sliders && !victims);
    }

#endif

#endif

    constexpr bool mirror(Square kingSquare) const noexcept { return kingSquare.file() > File::D; }

    constexpr USize kingBucket(Square kingSquare, Color color) const noexcept {
        const bool mirr = mirror(kingSquare);
        Square relativeSquare = (mirr) ? kingSquare.mirrored() : kingSquare;
        relativeSquare = (color == Color::WHITE) ? relativeSquare : relativeSquare.flipped();
        return Arch::KING_BUCKET_LAYOUT[4 * static_cast<USize>(relativeSquare.rank()) + static_cast<USize>(relativeSquare.file())];
    }
};

}
