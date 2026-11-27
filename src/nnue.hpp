#pragma once

#include <array>
#include <cassert>
#include <fstream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "accumulator.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "network.hpp"
#include "piece.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "updates.hpp"
#include "utils.hpp"

namespace Sift {
    class NNUE {
    public:
        using Arch = PairwiseMultilayerArch;

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

            update(position);

            if constexpr (InputFeatureSet::THREAT_INPUTS) {
                return forwardNetwork(curr_->psqAcc, curr_->threatAcc, position, color);
            } else {
                return forwardNetwork(curr_->psqAcc, Accumulator(), position, color);
            }
        }

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
                curr.psqAcc.sub1Add1From(prev, network_.ftPSQWeights, color, sub, add);
            } else if (updates.psqAddSize == 1 && updates.psqSubSize == 2) {
                const USize sub1 = updates.psqSubs[0].index<InputFeatureSet>(color, kingSquare);
                const USize sub2 = updates.psqSubs[1].index<InputFeatureSet>(color, kingSquare);
                const USize add = updates.psqAdds[0].index<InputFeatureSet>(color, kingSquare);
                curr.psqAcc.sub2Add1From(prev, network_.ftPSQWeights, color, sub1, sub2, add);
            } else if (updates.psqAddSize == 2 && updates.psqSubSize == 2) {
                const USize sub1 = updates.psqSubs[0].index<InputFeatureSet>(color, kingSquare);
                const USize sub2 = updates.psqSubs[1].index<InputFeatureSet>(color, kingSquare);
                const USize add1 = updates.psqAdds[0].index<InputFeatureSet>(color, kingSquare);
                const USize add2 = updates.psqAdds[1].index<InputFeatureSet>(color, kingSquare);
                curr.psqAcc.sub2Add2From(prev, network_.ftPSQWeights, color, sub1, sub2, add1, add2);
            } else {
                assert(false);
            }

            curr.setPSQClean(color);
        }

        template<bool ZERO_INIT>
        FORCE_INLINE void accumulateThreatChanges(std::span<Int16, L1_SIZE> acc, const Network& network, std::span<const UInt16> adds,
            std::span<const UInt16> subs) noexcept {
            static constexpr USize CHUNK_SIZE = SIMD::CHUNK_SIZE_16;
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
                std::array<VecInt16, TILE> tmp;
                for (USize j = 0; j < TILE; j++) {
                    if constexpr (ZERO_INIT) {
                        tmp[j] = SIMD::zeroInt16();
                    } else {
                        tmp[j] = SIMD::loadInt16(&acc[(i + j) * CHUNK_SIZE]);
                    }
                }

                for (const UInt16 idx : subs) {
                    const Int8* sub = network.ftThreatWeightPtr(idx);
                    for (USize j = 0; j < TILE; j++) { tmp[j] = SIMD::subInt16(tmp[j], SIMD::widenLoadInt8(&sub[(i + j) * CHUNK_SIZE])); }
                }

                for (const UInt16 idx : adds) {
                    const Int8* add = network.ftThreatWeightPtr(idx);
                    for (USize j = 0; j < TILE; j++) { tmp[j] = SIMD::addInt16(tmp[j], SIMD::widenLoadInt8(&add[(i + j) * CHUNK_SIZE])); }
                }

                for (USize j = 0; j < TILE; j++) { SIMD::storeInt16(&acc[(i + j) * CHUNK_SIZE], tmp[j]); }
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
                    }

                    Bitboard blackMasked = blackAfter & mask;
                    while (blackMasked) {
                        const UInt8 sq2 = blackMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::BLACK);
                        adds[addOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
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
                    }

                    Bitboard blackMasked = blackBefore & mask;
                    while (blackMasked) {
                        const UInt8 sq2 = blackMasked.pop();
                        PPFeature feature = PPFeature(Square(sq1), pawnColor, Square(sq2), Color::BLACK);
                        subs[subOffset++] = feature.index<InputFeatureSet>(color, kingSquare);
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

            accumulateThreatChanges<true>(acc, network_, std::span<const UInt16>{indices.data(), size}, std::span<const UInt16>{});
        }

        inline void updateThreatFeatures(UpdatableAccumulator& curr, const Updates& updates, Color color, Square kingSquare) noexcept {
            assert(!updates.needsTIRefresh(color));

            std::array<UInt16, 192> adds;
            std::array<UInt16, 192> subs;
            USize addSize = 0;
            USize subSize = 0;

            for (USize i = 0; i < updates.tiAddSize; i++) {
                const TIFeature& feature = updates.tiAdds[i];
                const Int64 idx = feature.index<InputFeatureSet>(color, kingSquare);
                if (idx >= 0) {
                    adds[addSize++] = static_cast<UInt16>(idx);
                    assert(addSize <= adds.size());
                }
            }

            for (USize i = 0; i < updates.tiSubSize; i++) {
                const TIFeature& feature = updates.tiSubs[i];
                const Int64 idx = feature.index<InputFeatureSet>(color, kingSquare);
                if (idx >= 0) {
                    subs[subSize++] = static_cast<UInt16>(idx);
                    assert(subSize <= subs.size());
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

            accumulateThreatChanges<false>(curr.threatAcc.data(color), network_, std::span<const UInt16>{adds.data(), addSize},
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
                entry.acc.add4(network_.ftPSQWeights, color, add1, add2, add3, add4);
                addSize -= 4;
            }

            while (addSize >= 1) {
                const USize add = adds[addSize - 1];
                entry.acc.add1(network_.ftPSQWeights, color, add);
                addSize -= 1;
            }

            while (subSize >= 4) {
                const USize sub1 = subs[subSize - 1];
                const USize sub2 = subs[subSize - 2];
                const USize sub3 = subs[subSize - 3];
                const USize sub4 = subs[subSize - 4];
                entry.acc.sub4(network_.ftPSQWeights, color, sub1, sub2, sub3, sub4);
                subSize -= 4;
            }

            while (subSize >= 1) {
                const USize sub = subs[subSize - 1];
                entry.acc.sub1(network_.ftPSQWeights, color, sub);
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
