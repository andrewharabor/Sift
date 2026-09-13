#pragma once

#include <algorithm>
#include <array>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "geometry.hpp"
#include "loader.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"
#include "types.hpp"

#define NET_PARAM(Type, SIZE, name) \
    std::span<const Type, SIZE> name = std::span<const Type, SIZE>{static_cast<const Type *>(nullptr), SIZE}


namespace Sift {

template<typename FeatureTransformer>
class Accumulator {
private:
    static constexpr USize INPUT_SIZE = FeatureTransformer::PSQ_INPUT_SIZE;
    static constexpr USize WEIGHT_SIZE = FeatureTransformer::PSQ_WEIGHT_SIZE;
    static constexpr USize OUTPUT_SIZE = FeatureTransformer::OUTPUT_SIZE;

    using OutputType = std::span<Int16, OUTPUT_SIZE>;
    using ConstOutputType = std::span<const Int16, OUTPUT_SIZE>;
    using WeightType = std::span<const Int16, WEIGHT_SIZE>;

    alignas(SIMD::ALIGNMENT) MultiArray<Int16, 2, OUTPUT_SIZE> output_;

public:
    Accumulator() noexcept : output_() {}

    inline ConstOutputType output(Color color) const noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline OutputType output(Color color) noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline void init(const FeatureTransformer &ft) noexcept {
        std::ranges::copy(ft.biases, output_[0].begin());
        std::ranges::copy(ft.biases, output_[1].begin());
    }

    inline void sub1Add1From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub < INPUT_SIZE);
        assert(add < INPUT_SIZE);


        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset = sub * OUTPUT_SIZE;
        USize addOffset = add * OUTPUT_SIZE;

        assert(subOffset + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i];
        }
    }

    inline void sub2Add1From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add < INPUT_SIZE);

        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = sub1 * OUTPUT_SIZE;
        USize subOffset2 = sub2 * OUTPUT_SIZE;
        USize addOffset = add * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
        }
    }

    inline void sub2Add2From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add1, USize add2) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add1 < INPUT_SIZE);
        assert(add2 < INPUT_SIZE);

        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = sub1 * OUTPUT_SIZE;
        USize subOffset2 = sub2 * OUTPUT_SIZE;
        USize addOffset1 = add1 * OUTPUT_SIZE;
        USize addOffset2 = add2 * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset1 + i] + delta[addOffset2 + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
        }
    }


    inline void add1(const FeatureTransformer &ft, Color color, USize feature) noexcept {
        assert(color != Color::NONE);
        assert(feature < INPUT_SIZE);

        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize addOffset = feature * OUTPUT_SIZE;

        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] += delta[addOffset + i];
        }
    }

    inline void sub1(const FeatureTransformer &ft, Color color, USize feature) noexcept {
        assert(color != Color::NONE);
        assert(feature < INPUT_SIZE);

        ConstOutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset = feature * OUTPUT_SIZE;

        assert(subOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] -= delta[subOffset + i];
        }
    }

    inline void add4(const FeatureTransformer &ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
        assert(color != Color::NONE);
        assert(feature1 < INPUT_SIZE);
        assert(feature2 < INPUT_SIZE);
        assert(feature3 < INPUT_SIZE);
        assert(feature4 < INPUT_SIZE);

        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize addOffset1 = feature1 * OUTPUT_SIZE;
        USize addOffset2 = feature2 * OUTPUT_SIZE;
        USize addOffset3 = feature3 * OUTPUT_SIZE;
        USize addOffset4 = feature4 * OUTPUT_SIZE;

        assert(addOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset3 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset4 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] += delta[addOffset1 + i] + delta[addOffset2 + i] + delta[addOffset3 + i] + delta[addOffset4 + i];
        }
    }

    inline void sub4(const FeatureTransformer &ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
        assert(color != Color::NONE);
        assert(feature1 < INPUT_SIZE);
        assert(feature2 < INPUT_SIZE);
        assert(feature3 < INPUT_SIZE);
        assert(feature4 < INPUT_SIZE);

        ConstOutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = feature1 * OUTPUT_SIZE;
        USize subOffset2 = feature2 * OUTPUT_SIZE;
        USize subOffset3 = feature3 * OUTPUT_SIZE;
        USize subOffset4 = feature4 * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset3 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset4 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] -= delta[subOffset1 + i] + delta[subOffset2 + i] + delta[subOffset3 + i] + delta[subOffset4 + i];
        }
    }
};

template<typename Accumulator>
struct RefreshTableEntry {
    Accumulator accumulator = Accumulator();
    MultiArray<Bitboard, 2, 2> pieceBitboardSet = {};
    MultiArray<Bitboard, 2, 6> occupancyBitboardSet = {};

    constexpr std::span<Bitboard, 2> pieceBitboards(Color color) noexcept {
        assert(color != Color::NONE);
        return pieceBitboardSet[color.index()];
    }

    constexpr std::span<Bitboard, 6> occupancyBitboards(Color color) noexcept {
        assert(color != Color::NONE);
        return occupancyBitboardSet[color.index()];
    }
};

template<typename FeatureTransformer, USize SIZE>
struct RefreshTable {
    std::array<RefreshTableEntry<Accumulator<FeatureTransformer>>, SIZE> entries = {};

    inline void init(const FeatureTransformer &ft) noexcept {
        for (auto &entry : entries) {
            entry.accumulator.init(ft);
            entry.pieceBitboardSet.fill(Bitboard());
            entry.occupancyBitboardSet.fill(Bitboard());
        }
    }
};

template<USize OUTPUTS, typename FeatureSet>
struct FeatureTransformer {
    using InputFeatureSet = FeatureSet;
    using Accumulator = Accumulator<FeatureTransformer>;
    using RefreshTable = RefreshTable<FeatureTransformer, FeatureSet::REFRESH_TABLE_SIZE>;

    static constexpr USize PSQ_INPUT_SIZE = FeatureSet::BUCKET_COUNT * FeatureSet::PSQ_FEATURES;
    static constexpr USize OUTPUT_SIZE = OUTPUTS;
    static constexpr USize PSQ_WEIGHT_SIZE = PSQ_INPUT_SIZE * OUTPUT_SIZE;
    static constexpr USize THREAT_WEIGHT_SIZE = FeatureSet::THREAT_FEATURES * OUTPUT_SIZE;
    static constexpr USize BIAS_SIZE = OUTPUT_SIZE;

    static_assert(PSQ_INPUT_SIZE > 0);
    static_assert(OUTPUT_SIZE > 0);

    NET_PARAM(Int16, PSQ_WEIGHT_SIZE, psqWeights);
    NET_PARAM(Int8, THREAT_WEIGHT_SIZE, threatWeights);
    NET_PARAM(Int16, BIAS_SIZE, biases);

    constexpr const Int16 *psqWeightPtr(USize featureIdx) const noexcept { return &psqWeights[featureIdx * OUTPUT_SIZE]; }
    constexpr const Int8 *threatWeightPtr(USize featureIdx) const noexcept { return &threatWeights[featureIdx * OUTPUT_SIZE]; }

    inline bool load(ByteLoader &loader) noexcept { return loader.load(psqWeights) && loader.load(threatWeights) && loader.load(biases); }

    static constexpr USize byteSize() noexcept { return sizeof(Int16) * PSQ_WEIGHT_SIZE + sizeof(Int8) * THREAT_WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE; }
};

struct ReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        const Vec<Int16> clamped = SIMD::max<Int16>(inputs, zero);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct CReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct SCReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        const Vec<Int16> crelu = SIMD::mulLo<Int16>(clamped, weights);
        return SIMD::mulAddAdjAcc<Int16>(sum, crelu, clamped);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value / MAX; }
};

struct SingleBucketOutput {
public:
    static constexpr USize BUCKET_COUNT = 1;

    static constexpr USize bucket(const Position &) noexcept { return 0; }
};

struct OppColoredBishopBucketOutput {
public:
    static constexpr USize BUCKET_COUNT = 2;

    static constexpr USize bucket(const Position &position) noexcept {
        const bool bishops = !position.pieces(Piece::WHITE_BISHOP).empty() && !position.pieces(Piece::BLACK_BISHOP).empty();
        const bool oppColored = (position.pieces(Piece::WHITE_BISHOP) & LIGHT_SQUARES).empty() != (position.pieces(Piece::BLACK_BISHOP) & LIGHT_SQUARES).empty();
        return (bishops && oppColored) ? 1 : 0;
    }

private:
    static constexpr Bitboard LIGHT_SQUARES = Bitboard(0x55AA55AA55AA55AAULL);
};

template<USize BUCKETS>
struct MaterialCountBucketOutput {
    static_assert(BUCKETS == 2 || BUCKETS == 4 || BUCKETS == 8 || BUCKETS == 16 || BUCKETS == 32);

public:
    static constexpr USize BUCKET_COUNT = BUCKETS;

    static constexpr USize bucket(const Position &position) noexcept { return (position.occupied().count() - 2) / DIV; }

private:
    static constexpr USize DIV = 32 / BUCKET_COUNT;
};

template<typename FeatureTransformer, typename Output, typename Arch>
class PerspectiveNetwork {
private:
    using InputType = std::span<const Int16, FeatureTransformer::OUTPUT_SIZE>;

public:
    constexpr const FeatureTransformer &ft() const noexcept { return ft_; }

    inline Int32 forward(const Position &position, InputType friendlyPSQInputs, InputType enemyPSQInputs, InputType friendlyThreatInputs, InputType enemyThreatInputs) const noexcept { return arch_.forward(Output::bucket(position), friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs); }

    inline bool load(ByteLoader &loader) noexcept {
        if (!ft_.load(loader) || !arch_.load(loader)) {
            return false;
        }

        if (Arch::NEEDS_FT_PERMUTE) {
            Arch::permuteFTParams(ft_.psqWeights, ft_.threatWeights, ft_.biases);
        }
        return true;
    }

    static constexpr USize byteSize() noexcept { return FeatureTransformer::byteSize() + Arch::byteSize(); }

private:
    FeatureTransformer ft_;
    Arch arch_;
};

template<typename FeatureSet>
class BoardObserver {
public:
    typename FeatureSet::Updates updates;

    inline void kingMove(Color color, Square from, Square to) noexcept {
        if (FeatureSet::needsRefresh(color, from, to)) {
            updates.setPSQRefresh(color);
        }

        if constexpr (FeatureSet::THREAT_INPUTS) {
            if (FeatureSet::needsMirror(from) != FeatureSet::needsMirror(to)) {
                updates.setThreatRefresh(color);
            }
        }
    }

    inline void addPiece(const Position &position, Piece piece, Square square) noexcept {
        updates.addPSQFeature(PSQFeature(piece, square));
        if constexpr (FeatureSet::THREAT_INPUTS) {
            updateTIFeaturesOnChange<true>(position, piece, square);
        }
    }

    inline void removePiece(const Position &position, Piece piece, Square square) noexcept {
        updates.subPSQFeature(PSQFeature(piece, square));
        if constexpr (FeatureSet::THREAT_INPUTS) {
            updateTIFeaturesOnChange<false>(position, piece, square);
        }
    }

    inline void transmutePiece(const Position &position, Piece fromPiece, Piece toPiece, Square square) noexcept {
        updates.subPSQFeature(PSQFeature(fromPiece, square));
        updates.addPSQFeature(PSQFeature(toPiece, square));
        if constexpr (FeatureSet::THREAT_INPUTS) {
            updateTIFeaturesOnTransmute(position, fromPiece, toPiece, square);
        }
    }

    inline void movePiece(const Position &position, Piece fromPiece, Piece toPiece, Square fromSquare, Square toSquare) noexcept {
        updates.subPSQFeature(PSQFeature(fromPiece, fromSquare));
        updates.addPSQFeature(PSQFeature(toPiece, toSquare));
        if constexpr (FeatureSet::THREAT_INPUTS) {
            updateTIFeaturesOnMove(position, fromPiece, toPiece, fromSquare, toSquare);
        }
    }

    inline void pawnChanges(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
        if constexpr (FeatureSet::PAWN_PAWN_INPUTS) {
            updates.setPawns(whiteBefore, blackBefore, whiteAfter, blackAfter);
        }
    }

private:

#if defined(USE_VBMI2)

    template<bool ADD, bool OUTGOING>
    inline void pushDirectTIFeatures(const Vector &indices, const Vector &rays, BitRays bits, Piece piece, Square square) noexcept {
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
    inline void pushXRayTIFeatures(const Vector &indices, const Vector &rays, BitRays sliders, BitRays victims) noexcept {
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

        const auto writeFeatures = [&](TIFeature *ptr) {
            _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr) + 0, tuple1);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr) + 1, tuple2);
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
    inline void pushDirectTIFeatures(const Vector &indices, const Vector &rays, BitRays bits, Piece piece, Square square) noexcept {
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
    inline void pushXRayTIFeatures(const Vector &indices, const Vector &rays, BitRays sliders, BitRays victims) noexcept {
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
    inline void updateTIFeaturesOnChange(const Position &position, Piece piece, Square square) noexcept {
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

    inline void updateTIFeaturesOnTransmute(const Position &position, Piece oldPiece, Piece newPiece, Square square) noexcept {
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

    inline void updateTIFeaturesOnMove(const Position &position, Piece fromPiece, Piece toPiece, Square fromSquare, Square toSquare) noexcept {
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
