#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstring>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "geometry.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "types.hpp"

namespace Sift {
    struct Updates {
        std::array<bool, 2> psqRefresh = {};

        std::array<PSQFeature, 2> psqAdds = {};
        USize psqAddSize = 0;

        std::array<PSQFeature, 2> psqSubs = {};
        USize psqSubSize = 0;

        std::array<bool, 2> tiRefresh = {};

        std::array<TIFeature, 128> tiAdds = {};
        USize tiAddSize = 0;

        std::array<TIFeature, 128> tiSubs = {};
        USize tiSubSize = 0;

        std::array<Bitboard, 2> pawnsBefore = {};
        std::array<Bitboard, 2> pawnsAfter = {};

        constexpr void setPSQRefresh(Color color) noexcept { psqRefresh[color.index()] = true; }

        constexpr bool needsPSQRefresh(Color color) const noexcept { return psqRefresh[color.index()]; }

        constexpr void addPSQFeature(PSQFeature feature) noexcept {
            assert(psqAddSize < 2);
            psqAdds[psqAddSize++] = feature;
        }

        constexpr void subPSQFeature(PSQFeature feature) noexcept {
            assert(psqSubSize < 2);
            psqSubs[psqSubSize++] = feature;
        }

        constexpr void setTIRefresh(Color color) noexcept { tiRefresh[color.index()] = true; };

        constexpr bool needsTIRefresh(Color color) const noexcept { return tiRefresh[color.index()]; }

        constexpr void addTIFeature(TIFeature feature) noexcept {
            assert(tiAddSize < 128);
            tiAdds[tiAddSize++] = feature;
        }

        constexpr void subTIFeature(TIFeature feature) noexcept {
            assert(tiSubSize < 128);
            tiSubs[tiSubSize++] = feature;
        }

        template<typename Function>
        inline void writeAddTIFeatures(Function func) noexcept {
            assert(tiAddSize + 16 <= 128);
            tiAddSize += func(&tiAdds[tiAddSize]);
        }

        template<typename Function>
        inline void writeSubTIFeatures(Function func) noexcept {
            assert(tiSubSize + 16 <= 128);
            tiSubSize += func(&tiSubs[tiSubSize]);
        }

        constexpr void setPawns(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
            pawnsBefore[0] = whiteBefore;
            pawnsBefore[1] = blackBefore;
            pawnsAfter[0] = whiteAfter;
            pawnsAfter[1] = blackAfter;
        }
    };

    struct BoardObserver {
    public:
        Updates& updates;

        inline void kingMove(Color color, Square from, Square to) noexcept {
            if (Arch::needsRefresh(color, from, to)) { updates.setPSQRefresh(color); }
            if (Arch::needsMirror(from) != Arch::needsMirror(to)) { updates.setTIRefresh(color); }
        }

        inline void addPiece(const Position& position, Piece piece, Square square) noexcept {
            updates.addPSQFeature(PSQFeature(piece, square));
            updateTIFeaturesOnChange<true>(position, piece, square);
        }

        inline void removePiece(const Position& position, Piece piece, Square square) noexcept {
            updates.subPSQFeature(PSQFeature(piece, square));
            updateTIFeaturesOnChange<false>(position, piece, square);
        }

        inline void transmutePiece(const Position& position, Piece fromPiece, Piece toPiece, Square square) noexcept {
            updates.subPSQFeature(PSQFeature(fromPiece, square));
            updates.addPSQFeature(PSQFeature(toPiece, square));
            updateTIFeaturesOnTransmute(position, fromPiece, toPiece, square);
        }

        inline void movePiece(const Position& position, Piece fromPiece, Piece toPiece, Square fromSquare, Square toSquare) noexcept {
            updates.subPSQFeature(PSQFeature(fromPiece, fromSquare));
            updates.addPSQFeature(PSQFeature(toPiece, toSquare));
            updateTIFeaturesOnMove(position, fromPiece, toPiece, fromSquare, toSquare);
        }

        inline void pawnChanges(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
            updates.setPawns(whiteBefore, blackBefore, whiteAfter, blackAfter);
        }

    private:
#if defined(USE_VBMI2)

        static_assert(sizeof(TIFeature) == sizeof(UInt32));
        static_assert(offsetof(TIFeature, attacker) == 0);
        static_assert(offsetof(TIFeature, attackerSq) == 1);
        static_assert(offsetof(TIFeature, victim) == 2);
        static_assert(offsetof(TIFeature, victimSq) == 3);

        template<bool ADD, bool OUTGOING>
        FORCE_INLINE void pushDirectTIFeatures(const Vector& indices, const Vector& rays, BitRays bits, Piece piece,
            Square square) noexcept {
            const auto pair2Shuffle = _mm512_set_epi8(
                // clang-format off
                79, 15, 79, 15, 78, 14, 78, 14, 77, 13, 77, 13, 76, 12, 76, 12, 75, 11, 75, 11,
                74, 10, 74, 10, 73, 9, 73, 9, 72, 8, 72, 8, 71, 7, 71, 7, 70, 6, 70, 6, 69, 5,
                69, 5, 68, 4, 68, 4, 67, 3, 67, 3, 66, 2, 66, 2, 65, 1, 65, 1, 64, 0, 64, 0
                // clang-format on
            );

            const auto pair1 = _mm512_set1_epi16(static_cast<Int16>(piece.index() | (square.index() << 8)));
            const auto pair2Square = _mm512_maskz_compress_epi8(bits, indices.raw);
            const auto pair2Piece = _mm512_maskz_compress_epi8(bits, rays.raw);
            const auto pair2 = _mm512_permutex2var_epi8(pair2Piece, pair2Shuffle, pair2Square);

            constexpr UInt64 MASK = (OUTGOING) ? 0xCCCCCCCCCCCCCCCC : 0x3333333333333333;
            const auto vector = _mm512_mask_mov_epi8(pair1, MASK, pair2);

            const auto writeFeatures = [&](TIFeature* ptr) {
                _mm512_storeu_si512(ptr, vector);
                return static_cast<USize>(std::popcount(bits));
            };

            if constexpr (ADD) {
                updates.writeAddTIFeatures(writeFeatures);
            } else {
                updates.writeSubTIFeatures(writeFeatures);
            }
        }

        template<bool ADD>
        FORCE_INLINE void pushXRayTIFeatures(const Vector& indices, const Vector& rays, BitRays sliders, BitRays victims) noexcept {
            assert(std::popcount(sliders) == std::popcount(victims));

            const USize count = static_cast<USize>(std::popcount(victims));

            const auto piece1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, rays.raw));
            const auto square1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, indices.raw));
            const auto piece2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, rays.flipped().raw));
            const auto square2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, indices.flipped().raw));

            const auto pair1 = _mm_unpacklo_epi8(piece1, square1);
            const auto pair2 = _mm_unpacklo_epi8(piece2, square2);
            const auto tuple1 = _mm_unpacklo_epi16(pair1, pair2);
            const auto tuple2 = _mm_unpackhi_epi16(pair1, pair2);

            const auto writeFeatures = [&](TIFeature* ptr) {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 0, tuple1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 1, tuple2);
                return count;
            };

            if constexpr (ADD) {
                updates.writeSubTIFeatures(writeFeatures);
            } else {
                updates.writeAddTIFeatures(writeFeatures);
            }
        }

#else

        template<bool ADD, bool OUTGOING>
        inline void pushDirectTIFeatures(const Vector& indices, const Vector& rays, BitRays bits, Piece piece, Square square) noexcept {
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
                    updates.addTIFeature(feature);
                } else {
                    updates.subTIFeature(feature);
                }
            }
        }

        template<bool ADD>
        inline void pushXRayTIFeatures(const Vector& indices, const Vector& rays, BitRays sliders, BitRays victims) noexcept {
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
                    updates.subTIFeature(feature);
                } else {
                    updates.addTIFeature(feature);
                }
            }

            assert(!sliders && !victims);
        }

#endif

        template<bool ADD>
        inline void updateTIFeaturesOnChange(const Position& position, Piece piece, Square square) noexcept {
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
        }

        inline void updateTIFeaturesOnTransmute(const Position& position, Piece oldPiece, Piece newPiece, Square square) noexcept {
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
        }

        inline void updateTIFeaturesOnMove(const Position& position, Piece fromPiece, Piece toPiece, Square fromSquare,
            Square toSquare) noexcept {
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
        }
    };
}
