#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "accumulator.hpp"
#include "activation.hpp"
#include "arch.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "geometry.hpp"
#include "input.hpp"
#include "network.hpp"
#include "output.hpp"
#include "piece.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace Sift {
    template<typename FeatureSet>
    struct BoardObserver {
    public:
        FeatureSet::Updates& updates;

        inline void kingMove(Color color, Square from, Square to) noexcept {
            if (FeatureSet::needsRefresh(color, from, to)) { updates.setPSQRefresh(color); }

            if constexpr (FeatureSet::THREAT_INPUTS) {
                if (FeatureSet::needsMirror(from) != FeatureSet::needsMirror(to)) { updates.setTIRefresh(color); }
            }
        }

        inline void addPiece(const Position& position, Piece piece, Square square) noexcept {
            updates.addPSQFeature(PSQFeature(piece, square));
            if constexpr (FeatureSet::THREAT_INPUTS) { updateTIFeaturesOnChange<true>(position, piece, square); }
        }

        inline void removePiece(const Position& position, Piece piece, Square square) noexcept {
            updates.subPSQFeature(PSQFeature(piece, square));
            if constexpr (FeatureSet::THREAT_INPUTS) { updateTIFeaturesOnChange<false>(position, piece, square); }
        }

        inline void transmutePiece(const Position& position, Piece fromPiece, Piece toPiece, Square square) noexcept {
            updates.subPSQFeature(PSQFeature(fromPiece, square));
            updates.addPSQFeature(PSQFeature(toPiece, square));
            if constexpr (FeatureSet::THREAT_INPUTS) { updateTIFeaturesOnTransmute(position, fromPiece, toPiece, square); }
        }

        inline void movePiece(const Position& position, Piece fromPiece, Piece toPiece, Square fromSquare, Square toSquare) noexcept {
            updates.subPSQFeature(PSQFeature(fromPiece, fromSquare));
            updates.addPSQFeature(PSQFeature(toPiece, toSquare));
            if constexpr (FeatureSet::THREAT_INPUTS) { updateTIFeaturesOnMove(position, fromPiece, toPiece, fromSquare, toSquare); }
        }

        inline void pawnChanges(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
            if constexpr (FeatureSet::PAWN_PAWN_INPUTS) { updates.setPawns(whiteBefore, blackBefore, whiteAfter, blackAfter); }
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

            if constexpr (FeatureSet::THREAT_INPUTS) {
                if constexpr (ADD) {
                    updates.writeAddTIFeatures(writeFeatures);
                } else {
                    updates.writeSubTIFeatures(writeFeatures);
                }
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

            if constexpr (FeatureSet::THREAT_INPUTS) {
                if constexpr (ADD) {
                    updates.writeSubTIFeatures(writeFeatures);
                } else {
                    updates.writeAddTIFeatures(writeFeatures);
                }
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

                if constexpr (FeatureSet::THREAT_INPUTS) {
                    if constexpr (ADD) {
                        updates.addTIFeature(feature);
                    } else {
                        updates.subTIFeature(feature);
                    }
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

                if constexpr (FeatureSet::THREAT_INPUTS) {
                    if constexpr (ADD) {
                        updates.subTIFeature(feature);
                    } else {
                        updates.addTIFeature(feature);
                    }
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

            if constexpr (ADD) {
                std::cout << "updateTIFeaturesOnChange<true>:" << std::endl;
            } else {
                std::cout << "updateTIFeaturesOnChange<false>:" << std::endl;
            }

            std::cout << "\tclosest: " << closest << std::endl;
            std::cout << "\toutgoing: " << outgoing << std::endl;
            std::cout << "\tincomingAttackers: " << incomingAttackers << std::endl;
            std::cout << "\tincomingSliders: " << incomingSliders << std::endl;
            std::cout << "\tvictimMask: " << victimMask << std::endl;
            std::cout << "\tvalid: " << valid << std::endl;

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

            std::cout << "updateTIFeaturesOnTransmute:" << std::endl;
            std::cout << "\tclosest: " << closest << std::endl;
            std::cout << "\toldOutgoing: " << oldOutgoing << std::endl;
            std::cout << "\tnewOutgoing: " << newOutgoing << std::endl;
            std::cout << "\tincomingAttackers: " << incomingAttackers << std::endl;

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

            std::cout << "updateTIFeaturesOnMove:" << std::endl;
            std::cout << "\tfromClosest: " << fromClosest << std::endl;
            std::cout << "\ttoClosest: " << toClosest << std::endl;
            std::cout << "\tfromOutgoing: " << fromOutgoing << std::endl;
            std::cout << "\ttoOutgoing: " << toOutgoing << std::endl;
            std::cout << "\tfromIncomingAttackers: " << fromIncomingAttackers << std::endl;
            std::cout << "\ttoIncomingAttackers: " << toIncomingAttackers << std::endl;
            std::cout << "\tfromIncomingSliders: " << fromIncomingSliders << std::endl;
            std::cout << "\ttoIncomingSliders: " << toIncomingSliders << std::endl;
            std::cout << "\tfromVictimMask: " << fromVictimMask << std::endl;
            std::cout << "\ttoVictimMask: " << toVictimMask << std::endl;
            std::cout << "\tfromValid: " << fromValid << std::endl;
            std::cout << "\ttoValid: " << toValid << std::endl;

            pushDirectTIFeatures<false, true>(fromPerm.indices, fromRays, fromOutgoing, fromPiece, fromSquare);
            pushDirectTIFeatures<false, false>(fromPerm.indices, fromRays, fromIncomingAttackers, fromPiece, fromSquare);
            pushDirectTIFeatures<true, true>(toPerm.indices, toRays, toOutgoing, toPiece, toSquare);
            pushDirectTIFeatures<true, false>(toPerm.indices, toRays, toIncomingAttackers, toPiece, toSquare);
            pushXRayTIFeatures<false>(fromPerm.indices, fromRays, fromIncomingSliders & fromValid, fromVictimMask & fromValid);
            pushXRayTIFeatures<true>(toPerm.indices, toRays, toIncomingSliders & toValid, toVictimMask & toValid);
        }
    };

    class NNUE {
    public:
        static constexpr USize L1_SIZE = 1024;
        static constexpr USize L2_SIZE = 32;
        static constexpr USize L3_SIZE = 64;

        static constexpr Int32 FT_SCALE_BITS = 7;
        static constexpr Int32 FT_QUANT_BITS = 8;
        static constexpr Int32 L1_QUANT_BITS = 7;

        static constexpr Int32 SCALE = 400;

        static constexpr bool DUAL_ACTIVATION = true;
        static constexpr bool SKIP_L2 = true;

        using PSQFeatureSet = MergedMirroredKingBucketInputs<
            // clang-format off
            MirroredKingSide::ABCD,
            0, 1, 2, 3,
            4, 5, 6, 7,
            8, 9, 10, 11,
            8, 9, 10, 11,
            12, 12, 13, 13,
            12, 12, 13, 13,
            14, 14, 15, 15,
            14, 14, 15, 15
            // clang-format on
            >;

        using InputFeatureSet = PawnPawnThreatInputs<PSQFeatureSet>;

        using Updates = InputFeatureSet::Updates;
        using BoardObserver = BoardObserver<InputFeatureSet>;

        using FeatureTransformer = FeatureTransformer<L1_SIZE, InputFeatureSet>;
        using Accumulator = FeatureTransformer::Accumulator;
        using RefreshTable = FeatureTransformer::RefreshTable;
        using RefreshTableEntry = RefreshTableEntry<Accumulator>;

        using UpdatableAccumulator = UpdatableAccumulator<InputFeatureSet, FeatureTransformer>;

        using Output = MaterialCountBucketOutput<8>;

        using Arch = PairwiseMultilayerArch<InputFeatureSet, L1_SIZE, L2_SIZE, L3_SIZE, FT_SCALE_BITS, FT_QUANT_BITS, L1_QUANT_BITS,
            DUAL_ACTIVATION, SKIP_L2, Output, SCALE>;

        using Network = PerspectiveNetwork<FeatureTransformer, Output, Arch>;

        NNUE() noexcept : curr_(), refreshTable_(), network_() { accStack_.resize(RESERVED_STATES); }

        constexpr void load(const Network* network) noexcept {
            assert(network != nullptr);
            network_ = network;
        }

        inline void set(const Position& position) noexcept {
            assert(network_ != nullptr);

            curr_ = &accStack_[0];

            refreshTable_.init(network_->ft());

            for (const Color color : {Color::WHITE, Color::BLACK}) {
                const Square kingSquare = position.kingSquare(color);
                const USize tableIdx = InputFeatureSet::refreshTableIdx(color, kingSquare);
                RefreshTableEntry& entry = refreshTable_.entries[tableIdx];

                resetPSQAcc(entry.acc, color, position);
                curr_->psqAcc.copy(color, entry.acc);
                entry.updateBitboards(color, position);

                if constexpr (InputFeatureSet::THREAT_INPUTS) { resetThreatAcc(curr_->threatAcc, color, position); }
            }
        }

        inline BoardObserver makeMove() noexcept {
            curr_++;
            curr_->updates = Updates();
            curr_->setPSQDirty();
            curr_->setThreatDirty();
            return BoardObserver(curr_->updates);
        }

        inline void unmakeMove() noexcept {
            assert(curr_ > &accStack_[0]);
            curr_--;
        }

        inline Int32 forward(const Position& position) noexcept {
            assert(network_ != nullptr);
            assert(curr_ >= &accStack_[0] && curr_ <= &accStack_.back());

            const Color color = position.sideToMove();

            const auto accChecksum = [&](Accumulator& acc, Color color) {
                Int32 sum = 0;
                for (USize i = 0; i < L1_SIZE; i++) { sum += acc.data(color)[i]; }
                return sum;
            };

            update(position);

            Accumulator psqAcc = Accumulator();
            psqAcc.init(network_->ft());
            resetPSQAcc(psqAcc, Color::WHITE, position);
            resetPSQAcc(psqAcc, Color::BLACK, position);

            Int32 whitePSQActual = accChecksum(curr_->psqAcc, Color::WHITE);
            Int32 blackPSQActual = accChecksum(curr_->psqAcc, Color::BLACK);
            Int32 whitePSQExpected = accChecksum(psqAcc, Color::WHITE);
            Int32 blackPSQExpected = accChecksum(psqAcc, Color::BLACK);

            if (whitePSQActual != whitePSQExpected) {
                std::cout << position.fen() << std::endl;
                std::cout << "white psq mismatch: expected " << whitePSQExpected << ", got " << whitePSQActual << std::endl;
                std::terminate();
            }

            if (blackPSQActual != blackPSQExpected) {
                std::cout << position.fen() << std::endl;
                std::cout << "black psq mismatch: expected " << blackPSQExpected << ", got " << blackPSQActual << std::endl;
                std::terminate();
            }

            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                Accumulator threatAcc = Accumulator();
                threatAcc.init(network_->ft());
                resetThreatAcc(threatAcc, Color::WHITE, position);
                resetThreatAcc(threatAcc, Color::BLACK, position);

                Int32 whiteThreatActual = accChecksum(curr_->threatAcc, Color::WHITE);
                Int32 blackThreatActual = accChecksum(curr_->threatAcc, Color::BLACK);
                Int32 whiteThreatExpected = accChecksum(threatAcc, Color::WHITE);
                Int32 blackThreatExpected = accChecksum(threatAcc, Color::BLACK);

                if (whiteThreatActual != whiteThreatExpected) {
                    std::cout << position.fen() << std::endl;
                    std::cout << "white threat mismatch: expected " << whiteThreatExpected << ", got " << whiteThreatActual << std::endl;
                    std::terminate();
                }

                if (blackThreatActual != blackThreatExpected) {
                    std::cout << position.fen() << std::endl;
                    std::cout << "black threat mismatch: expected " << blackThreatExpected << ", got " << blackThreatActual << std::endl;
                    std::terminate();
                }
            }

            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                return forwardNetwork(curr_->psqAcc, curr_->threatAcc, position, color);
            } else {
                return forwardNetwork(curr_->psqAcc, Accumulator(), position, color);
            }
        }

        //        inline Int32 forwardOnce(const Position &position) noexcept {
        //     assert(network_ != nullptr);
        //     assert(curr_ >= &accStack_[0] && curr_ <= &accStack_.back());

        //     const Color color = position.sideToMove();

        //     Accumulator psqAcc = Accumulator();
        //     psqAcc.init(network_->ft());
        //     resetPSQAcc(psqAcc, Color::WHITE, position);
        //     resetPSQAcc(psqAcc, Color::BLACK, position);

        //     if constexpr (InputFeatureSet::THREAT_INPUTS) {
        //         Accumulator threatAcc = Accumulator();
        //         resetThreatAcc(threatAcc, Color::WHITE, position);
        //         resetThreatAcc(threatAcc, Color::BLACK, position);
        //         return forwardNetwork(psqAcc, threatAcc, position, color);
        //     } else {
        //         return forwardNetwork(psqAcc, Accumulator(), position, color);
        //     }
        // }

    private:
        static constexpr USize RESERVED_STATES = 256;

        std::vector<UpdatableAccumulator> accStack_;
        UpdatableAccumulator* curr_;

        RefreshTable refreshTable_;

        const Network* network_;

        void updatePSQFeatures(const Accumulator& prev, UpdatableAccumulator& curr, const Updates& updates, Color color,
            Square kingSquare) noexcept {
            assert(!updates.needsPSQRefresh(color));

            if (updates.psqAddSize == 0 && updates.psqSubSize == 0) { return; }

            if (updates.psqAddSize == 1 && updates.psqSubSize == 1) {
                const USize sub = updates.psqSubs[0].index<InputFeatureSet>(color, kingSquare);
                const USize add = updates.psqAdds[0].index<InputFeatureSet>(color, kingSquare);
                curr.psqAcc.sub1Add1From(prev, network_->ft(), color, sub, add);
            } else if (updates.psqAddSize == 1 && updates.psqSubSize == 2) {
                const USize sub1 = updates.psqSubs[0].index<InputFeatureSet>(color, kingSquare);
                const USize sub2 = updates.psqSubs[1].index<InputFeatureSet>(color, kingSquare);
                const USize add = updates.psqAdds[0].index<InputFeatureSet>(color, kingSquare);
                curr.psqAcc.sub2Add1From(prev, network_->ft(), color, sub1, sub2, add);
            } else if (updates.psqAddSize == 2 && updates.psqSubSize == 2) {
                const USize sub1 = updates.psqSubs[0].index<InputFeatureSet>(color, kingSquare);
                const USize sub2 = updates.psqSubs[1].index<InputFeatureSet>(color, kingSquare);
                const USize add1 = updates.psqAdds[0].index<InputFeatureSet>(color, kingSquare);
                const USize add2 = updates.psqAdds[1].index<InputFeatureSet>(color, kingSquare);
                curr.psqAcc.sub2Add2From(prev, network_->ft(), color, sub1, sub2, add1, add2);
            } else {
                assert(false);
            }

            curr.setPSQClean(color);
        }

        template<bool ZERO_INIT>
        FORCE_INLINE void accumulateThreatChanges(std::span<Int16, L1_SIZE> acc, const FeatureTransformer& ft, std::span<const UInt16> adds,
            std::span<const UInt16> subs) noexcept {
            static constexpr USize CHUNK_SIZE = SIMD::CHUNK_SIZE<Int16>;
            static constexpr USize CHUNKS = L1_SIZE / CHUNK_SIZE;

            static_assert(L1_SIZE % CHUNK_SIZE == 0);

#if defined(USE_AVX512)
            static constexpr USize TILE_TARGET = 32;
#else
            static constexpr USize TILE_TARGET = 8;
#endif

            static constexpr USize TILE = std::min(CHUNKS, TILE_TARGET);

            static_assert(CHUNKS % TILE == 0);

            for (USize i = 0; i < CHUNKS; i += TILE) {
                std::array<Vec<Int16>, TILE> tmp;
                for (USize j = 0; j < TILE; j++) {
                    if constexpr (ZERO_INIT) {
                        tmp[j] = SIMD::zero<Int16>();
                    } else {
                        tmp[j] = SIMD::load<Int16>(&acc[(i + j) * CHUNK_SIZE]);
                    }
                }

                for (const UInt16 idx : subs) {
                    const Int8* sub = ft.threatWeightPtr(idx);
                    for (USize j = 0; j < TILE; j++) { tmp[j] = SIMD::sub<Int16>(tmp[j], SIMD::widenLoadInt8(&sub[(i + j) * CHUNK_SIZE])); }
                }

                for (const UInt16 idx : adds) {
                    const Int8* add = ft.threatWeightPtr(idx);
                    for (USize j = 0; j < TILE; j++) { tmp[j] = SIMD::add<Int16>(tmp[j], SIMD::widenLoadInt8(&add[(i + j) * CHUNK_SIZE])); }
                }

                for (USize j = 0; j < TILE; j++) { SIMD::store<Int16>(&acc[(i + j) * CHUNK_SIZE], tmp[j]); }
            }
        }

#if defined(USE_VBMI2)
        static FORCE_INLINE __m512i ppIdxEpi16(__m512i a, __m512i b) {
            const auto hi = _mm512_max_epu16(a, b);
            const auto lo = _mm512_min_epu16(a, b);
            const auto prod = _mm512_mullo_epi16(hi, _mm512_sub_epi16(hi, _mm512_set1_epi16(1)));
            return _mm512_add_epi16(_mm512_srli_epi16(prod, 1), lo);
        }

        static FORCE_INLINE __m256i ppIdxEpi16(__m256i a, __m256i b) {
            const auto hi = _mm256_max_epu16(a, b);
            const auto lo = _mm256_min_epu16(a, b);
            const auto prod = _mm256_mullo_epi16(hi, _mm256_sub_epi16(hi, _mm256_set1_epi16(1)));
            return _mm256_add_epi16(_mm256_srli_epi16(prod, 1), lo);
        }
#endif

        FORCE_INLINE void writePPChanges(Color color, Square kingSquare, Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter,
            Bitboard blackAfter, std::span<UInt16> adds, std::span<UInt16> subs, USize& addOffset, USize& subOffset) noexcept {
#if defined(USE_VBMI2)

            const UInt8 squareMask = ((color == Color::BLACK) ? 0b111000 : 0) ^ ((InputFeatureSet::needsMirror(kingSquare)) ? 0b000111 : 0);
            const Bitboard friendlyBefore = (color == Color::WHITE) ? whiteBefore : blackBefore;
            const Bitboard friendlyAfter = (color == Color::WHITE) ? whiteAfter : blackAfter;
            const Bitboard after = whiteAfter | blackAfter;
            const Bitboard added = (whiteAfter & ~whiteBefore) | (blackAfter & ~blackBefore);
            const Bitboard removed = (whiteBefore & ~whiteAfter) | (blackBefore & ~blackAfter);
            const Bitboard same = after & ~added;

            static constexpr std::array<UInt8, 64> IOTA = [] {
                std::array<UInt8, 64> table{};
                std::iota(table.begin(), table.end(), 0);
                return table;
            }();

            const auto iota = _mm512_loadu_si512(IOTA.data());
            const auto adjusted =
                _mm512_sub_epi8(_mm512_xor_si512(iota, _mm512_set1_epi8(static_cast<Int8>(squareMask))), _mm512_set1_epi8(8));
            const auto ids = _mm512_mask_blend_epi8(friendlyAfter.bits(), _mm512_add_epi8(adjusted, _mm512_set1_epi8(48)), adjusted);
            const auto compressed = _mm512_maskz_compress_epi8(same.bits(), ids);
            const auto ids16 = _mm256_cvtepu8_epi16(_mm512_castsi512_si128(compressed));
            const auto sameDoubled = _mm512_broadcast_i64x4(ids16);
            const UInt16 sameCount = same.count();
            const UInt16 sameMask = static_cast<UInt16>((1 << sameCount) - 1);

            const auto pawnID = [&](Square square, bool enemy) -> UInt16 { return (square.index() ^ squareMask) - 8 + (enemy ? 48 : 0); };

    #if defined(USE_PEXT)
            const auto bandMask = [&](Square square) -> UInt16 {
                return static_cast<UInt16>(_pext_u64((PPFeature::MASKS[square.index()] & same).bits(), same.bits()));
            };
    #else
            const auto slowPEXT = [](UInt64 val, UInt64 mask) -> UInt64 {
                UInt64 res = 0;
                UInt64 bb = 1;
                while (mask) {
                    if (val & mask & (-mask)) { res |= bb; }
                    mask &= (mask - 1);
                    bb <<= 1;
                }
                return res;
            };

            const auto bandMask = [&](Square square) -> UInt16 {
                return static_cast<UInt16>(slowPEXT(PPFeature::MASKS[square.index()] & same, same));
            };
    #endif

            const auto ppIdx = [](UInt16 a, UInt16 b) -> UInt16 {
                const auto hi = std::max(a, b);
                const auto lo = std::min(a, b);
                return hi * (hi - 1) / 2 + lo;
            };

            const UInt16 removedSize = removed.count();
            assert(removedSize > 0);

            Bitboard remaining = removed;

            const Square remSq1 = Square(remaining.pop());
            const Square remSq2 = (removedSize >= 2) ? Square(remaining.pop()) : Square::NONE;
            const UInt16 remID1 = pawnID(remSq1, !(friendlyBefore & Bitboard(remSq1)));
            const UInt16 remID2 = (removedSize >= 2) ? pawnID(remSq2, !(friendlyBefore & Bitboard(remSq2))) : 0;
            const UInt16 remMask1 = sameMask & bandMask(remSq1);
            const UInt16 remMask2 = (removedSize >= 2) ? sameMask & bandMask(remSq2) : 0;
            const UInt32 remMask = static_cast<UInt32>(remMask1 | (remMask2 << 16));

            const auto remVec = _mm512_insertf64x4(_mm512_castsi256_si512(_mm256_set1_epi16(static_cast<Int16>(remID1))),
                _mm256_set1_epi16(static_cast<Int16>(remID2)), 1);
            _mm512_storeu_epi16(&subs[subOffset], _mm512_maskz_compress_epi16(remMask, ppIdxEpi16(remVec, sameDoubled)));
            subOffset += static_cast<USize>(std::popcount(remMask));

            if (removedSize >= 2) {
                assert(PPFeature::MASKS[remSq1.index()] & Bitboard(remSq2));
                subs[subOffset++] = ppIdx(remID1, remID2);
            }

            if (!added.empty()) {
                const Square sq = Square(added.lsb());
                const UInt16 id = pawnID(sq, !(friendlyAfter & Bitboard(sq)));
                const UInt16 mask = sameMask & bandMask(sq);
                const auto idx = ppIdxEpi16(_mm256_set1_epi16(static_cast<Int16>(id)), ids16);
                _mm256_storeu_epi16(&adds[addOffset], _mm256_maskz_compress_epi16(mask, idx));
                addOffset += static_cast<USize>(std::popcount(mask));
            }

#else

            Bitboard before = whiteBefore | blackBefore;
            Bitboard after = whiteAfter | blackAfter;
            std::array<Bitboard, 2> added = {whiteAfter & ~whiteBefore, blackAfter & ~blackBefore};
            std::array<Bitboard, 2> removed = {whiteBefore & ~whiteAfter, blackBefore & ~blackAfter};
            for (const Color pawnColor : {Color::WHITE, Color::BLACK}) {
                Bitboard& addedPawns = added[pawnColor.index()];
                while (addedPawns) {
                    const UInt8 sq1 = addedPawns.pop();
                    after &= ~Bitboard(Square(sq1));
                    const Bitboard mask = PPFeature::MASKS[sq1] & after;

                    Bitboard whiteMasked = whiteAfter & mask;
                    while (whiteMasked) {
                        const UInt8 sq2 = whiteMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::WHITE);
                        adds[addOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
                        std::cout << "\t+ " << std::string(feature) << std::endl;
                    }

                    Bitboard blackMasked = blackAfter & mask;
                    while (blackMasked) {
                        const UInt8 sq2 = blackMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::BLACK);
                        adds[addOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
                        std::cout << "\t+ " << std::string(feature) << std::endl;
                    }
                }

                Bitboard& removedPawns = removed[pawnColor.index()];
                while (removedPawns) {
                    const UInt8 sq1 = removedPawns.pop();
                    before &= ~Bitboard(Square(sq1));
                    const Bitboard mask = PPFeature::MASKS[sq1] & before;

                    Bitboard whiteMasked = whiteBefore & mask;
                    while (whiteMasked) {
                        const UInt8 sq2 = whiteMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::WHITE);
                        subs[subOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
                        std::cout << "\t- " << std::string(feature) << std::endl;
                    }

                    Bitboard blackMasked = blackBefore & mask;
                    while (blackMasked) {
                        const UInt8 sq2 = blackMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::BLACK);
                        subs[subOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
                        std::cout << "\t- " << std::string(feature) << std::endl;
                    }
                }
            }

#endif
        }

        inline void addThreatFeatures(std::span<Int16, L1_SIZE> acc, Color color, const Position& position) noexcept {
            const Square kingSquare = position.kingSquare(color);
            const Bitboard occupied = position.occupied();
            const Bitboard kings = position.pieces(PieceType::KING);

            std::array<UInt16, 256> indices;
            USize size = 0;

            Bitboard nonKings = occupied & ~kings;
            while (nonKings) {
                const Square from = Square(nonKings.pop());
                const Piece piece = position.pieceAt(from);
                Bitboard attacks = occupied & Attacks::attacks(piece, from, occupied) & ~kings;
                while (attacks) {
                    const Square to = Square(attacks.pop());
                    const Piece victim = position.pieceAt(to);
                    const TIFeature feature = TIFeature(piece, from, victim, to);
                    const Int64 idx = feature.index<InputFeatureSet>(color, kingSquare);
                    if (idx >= 0) {
                        indices[size++] = static_cast<UInt16>(idx);
                        assert(size <= indices.size());
                    }
                }
            }

            if constexpr (InputFeatureSet::PAWN_PAWN_INPUTS) {
                Bitboard friendlyPawns = position.pieces(PieceType::PAWN, color);
                Bitboard enemyPawns = position.pieces(PieceType::PAWN, ~color);

                while (friendlyPawns) {
                    const Square square1 = Square(friendlyPawns.pop());

                    Bitboard friendlyMasked = PPFeature::MASKS[square1.index()] & friendlyPawns;
                    while (friendlyMasked) {
                        const Square square2 = Square(friendlyMasked.pop());
                        const PPFeature feature = PPFeature(square1, color, square2, color);
                        indices[size++] = feature.index<InputFeatureSet>(color, kingSquare);
                        assert(size <= indices.size());
                    }

                    Bitboard enemyMasked = PPFeature::MASKS[square1.index()] & enemyPawns;
                    while (enemyMasked) {
                        const Square square2 = Square(enemyMasked.pop());
                        const PPFeature feature = PPFeature(square1, color, square2, ~color);
                        indices[size++] = feature.index<InputFeatureSet>(color, kingSquare);
                        assert(size <= indices.size());
                    }
                }

                while (enemyPawns) {
                    const Square square1 = Square(enemyPawns.pop());
                    Bitboard enemyMasked = PPFeature::MASKS[square1.index()] & enemyPawns;
                    while (enemyMasked) {
                        const Square square2 = Square(enemyMasked.pop());
                        const PPFeature feature = PPFeature(square1, ~color, square2, ~color);
                        indices[size++] = feature.index<InputFeatureSet>(color, kingSquare);
                        assert(size <= indices.size());
                    }
                }
            }

            accumulateThreatChanges<true>(acc, network_->ft(), std::span<const UInt16>{indices.data(), size}, std::span<const UInt16>{});
        }

        inline void updateThreatFeatures(UpdatableAccumulator& curr, const Updates& updates, Color color, Square kingSquare) noexcept {
            assert(!updates.needsTIRefresh(color));

            std::array<UInt16, 192> adds;
            std::array<UInt16, 192> subs;
            USize addSize = 0;
            USize subSize = 0;

            std::cout << "updating threat features:" << std::endl;
            std::cout << "addSize: " << updates.tiAddSize << std::endl;
            std::cout << "subSize: " << updates.tiSubSize << std::endl;

            for (USize i = 0; i < updates.tiAddSize; i++) {
                const TIFeature& feature = updates.tiAdds[i];
                const Int64 idx = feature.index<InputFeatureSet>(color, kingSquare);
                if (idx >= 0) {
                    adds[addSize++] = static_cast<UInt16>(idx);
                    assert(addSize <= adds.size());
                    std::cout << "\t+ " << std::string(feature) << std::endl;
                }
            }

            for (USize i = 0; i < updates.tiSubSize; i++) {
                const TIFeature& feature = updates.tiSubs[i];
                const Int64 idx = feature.index<InputFeatureSet>(color, kingSquare);
                if (idx >= 0) {
                    subs[subSize++] = static_cast<UInt16>(idx);
                    assert(subSize <= subs.size());
                    std::cout << "\t- " << std::string(feature) << std::endl;
                }
            }

            if constexpr (InputFeatureSet::PAWN_PAWN_INPUTS) {
                const Bitboard whiteBefore = updates.pawnsBefore[0];
                const Bitboard blackBefore = updates.pawnsBefore[1];
                const Bitboard whiteAfter = updates.pawnsAfter[0];
                const Bitboard blackAfter = updates.pawnsAfter[1];
                if (whiteBefore != whiteAfter || blackBefore != blackAfter) {
                    writePPChanges(color, kingSquare, whiteBefore, blackBefore, whiteAfter, blackAfter, adds, subs, addSize, subSize);
                    assert(addSize <= adds.size());
                    assert(subSize <= subs.size());
                }
            }

            accumulateThreatChanges<false>(curr.threatAcc.data(color), network_->ft(), std::span<const UInt16>{adds.data(), addSize},
                std::span<const UInt16>{subs.data(), subSize});

            curr.setThreatClean(color);
        }

        inline void resetPSQAcc(Accumulator& acc, Color color, const Position& position) noexcept {
            const Square kingSquare = position.kingSquare(color);
            for (UInt8 sq = 0; sq < 64; sq++) {
                const Square square = Square(sq);
                const Piece piece = position.pieceAt(square);
                if (piece != Piece::NONE) {
                    const PSQFeature feature = PSQFeature(piece, square);
                    const USize idx = feature.index<InputFeatureSet>(color, kingSquare);
                    acc.add1(network_->ft(), color, idx);
                }
            }
        }

        inline void resetThreatAcc(Accumulator& acc, Color color, const Position& position) noexcept {
            if constexpr (InputFeatureSet::THREAT_INPUTS) { addThreatFeatures(acc.data(color), color, position); }
        }

        inline void refreshPSQAcc(UpdatableAccumulator& curr, Color color, const Position& position) noexcept {
            const Square kingSquare = position.kingSquare(color);
            const USize tableIdx = InputFeatureSet::refreshTableIdx(color, kingSquare);
            RefreshTableEntry& entry = refreshTable_.entries[tableIdx];

            std::array<USize, 32> adds;
            std::array<USize, 32> subs;
            USize addSize = 0;
            USize subSize = 0;

            for (UInt8 pc = 0; pc < 12; pc++) {
                const Piece piece = Piece(pc);
                const Bitboard before = entry.pieces(color, piece);
                const Bitboard after = position.pieces(piece);

                Bitboard added = after & ~before;
                while (added) {
                    const Square square = Square(added.pop());
                    const PSQFeature feature = PSQFeature(piece, square);
                    adds[addSize++] = feature.index<InputFeatureSet>(color, kingSquare);
                    assert(addSize <= adds.size());
                }

                Bitboard removed = before & ~after;
                while (removed) {
                    const Square square = Square(removed.pop());
                    const PSQFeature feature = PSQFeature(piece, square);
                    subs[subSize++] = feature.index<InputFeatureSet>(color, kingSquare);
                    assert(subSize <= subs.size());
                }
            }

            while (addSize >= 4) {
                const USize add1 = adds[addSize - 1];
                const USize add2 = adds[addSize - 2];
                const USize add3 = adds[addSize - 3];
                const USize add4 = adds[addSize - 4];
                entry.acc.add4(network_->ft(), color, add1, add2, add3, add4);
                addSize -= 4;
            }

            while (addSize >= 1) {
                const USize add = adds[addSize - 1];
                entry.acc.add1(network_->ft(), color, add);
                addSize -= 1;
            }

            while (subSize >= 4) {
                const USize sub1 = subs[subSize - 1];
                const USize sub2 = subs[subSize - 2];
                const USize sub3 = subs[subSize - 3];
                const USize sub4 = subs[subSize - 4];
                entry.acc.sub4(network_->ft(), color, sub1, sub2, sub3, sub4);
                subSize -= 4;
            }

            while (subSize >= 1) {
                const USize sub = subs[subSize - 1];
                entry.acc.sub1(network_->ft(), color, sub);
                subSize -= 1;
            }

            entry.updateBitboards(color, position);

            curr.psqAcc.copy(color, entry.acc);

            curr.setPSQClean(color);
        }

        inline void refreshThreatAcc(UpdatableAccumulator& curr, Color color, const Position& position) noexcept {
            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                resetThreatAcc(curr.threatAcc, color, position);
                curr.setThreatClean(color);
            }
        }

        inline void update(const Position& position) noexcept {
            assert(network_ != nullptr);

            for (const Color color : {Color::WHITE, Color::BLACK}) {
                if (!curr_->isPSQDirty(color)) { continue; }

                if (curr_->updates.needsPSQRefresh(color)) {
                    refreshPSQAcc(*curr_, color, position);
                    continue;
                }

                UpdatableAccumulator* prev = curr_ - 1;
                for (; prev->isPSQDirty(color) && !prev->updates.needsPSQRefresh(color); prev--) {}

                assert(prev != &accStack_[0] || !prev->updates.needsPSQRefresh(color));

                if (prev->updates.needsPSQRefresh(color)) {
                    refreshPSQAcc(*curr_, color, position);
                } else {
                    do {
                        updatePSQFeatures(prev->psqAcc, *(prev + 1), (prev + 1)->updates, color, position.kingSquare(color));
                        prev++;
                    } while (prev != curr_);
                }
            }

            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                for (const Color color : {Color::WHITE, Color::BLACK}) {
                    if (!curr_->isThreatDirty(color)) { continue; }

                    if (curr_->updates.needsTIRefresh(color)) {
                        refreshThreatAcc(*curr_, color, position);
                        continue;
                    }

                    UpdatableAccumulator* prev = curr_ - 1;
                    for (; prev->isThreatDirty(color) && !prev->updates.needsTIRefresh(color); prev--) {}

                    assert(prev != &accStack_[0] || !prev->updates.needsTIRefresh(color));

                    if (prev->updates.needsTIRefresh(color)) {
                        refreshThreatAcc(*curr_, color, position);
                    } else {
                        do {
                            (prev + 1)->threatAcc.copy(color, prev->threatAcc);
                            updateThreatFeatures(*(prev + 1), (prev + 1)->updates, color, position.kingSquare(color));
                            prev++;
                        } while (prev != curr_);
                    }
                }
            }
        }

        inline Int32 forwardNetwork(const Accumulator& psqAcc, const Accumulator& threatAcc, const Position& position,
            Color color) noexcept {
            assert(network_ != nullptr);

            const auto& friendlyPSQAcc = psqAcc.data(color);
            const auto& enemyPSQAcc = psqAcc.data(~color);
            const auto& friendlyThreatAcc = threatAcc.data(color);
            const auto& enemyThreatAcc = threatAcc.data(~color);

            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                return network_->forward(position, friendlyPSQAcc, enemyPSQAcc, friendlyThreatAcc, enemyThreatAcc);
            } else {
                return network_->forward(position, friendlyPSQAcc, enemyPSQAcc, friendlyPSQAcc, enemyPSQAcc);
            }
        }
    };
};
