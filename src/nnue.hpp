#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>

#include "arch.hpp"
#include "features.hpp"
#include "loader.hpp"
#include "network.hpp"
#include "numa.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "utils.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_SILENCE_BITCODE_WARNING

#include "incbin/incbin.h"


INCBIN(std::byte, EMBEDDED_NETWORK, TOSTRING(NETWORK_FILE));

namespace Sift {

namespace NetConfig {

// constexpr USize L1_SIZE = 1024;
// constexpr USize L2_SIZE = 32;
// constexpr USize L3_SIZE = 64;

// constexpr Int32 FT_SCALE_BITS = 7;
// constexpr Int32 FT_QUANT_BITS = 8;
// constexpr Int32 L1_QUANT_BITS = 7;

// constexpr Int32 SCALE = 400;

// constexpr bool DUAL_ACTIVATION = true;
// constexpr bool SKIP_L2 = true;

// using PSQFeatureSet = MergedMirroredKingBucketInputs<
//     MirroredKingSide::ABCD,
//     0, 1, 2, 3,
//     4, 5, 6, 7,
//     8, 9, 10, 11,
//     8, 9, 10, 11,
//     12, 12, 13, 13,
//     12, 12, 13, 13,
//     14, 14, 15, 15,
//     14, 14, 15, 15
// >;

// using InputFeatureSet = PawnPawnThreatInputs<PSQFeatureSet>;

// using Updates = InputFeatureSet::Updates;

// using FeatureTransformer = Sift::FeatureTransformer<L1_SIZE, InputFeatureSet>;
// using Accumulator = FeatureTransformer::Accumulator;
// using RefreshTable = FeatureTransformer::RefreshTable;

// using Output = MaterialCountBucketOutput<8>;

// using Arch = PairwiseMultilayerArch<
//     InputFeatureSet,
//     L1_SIZE,
//     L2_SIZE,
//     L3_SIZE,
//     FT_SCALE_BITS,
//     FT_QUANT_BITS,
//     L1_QUANT_BITS,
//     DUAL_ACTIVATION,
//     SKIP_L2,
//     Output,
//     SCALE
// >;

constexpr USize L1_SIZE = 1024;
constexpr USize L2_SIZE = 32;
constexpr USize L3_SIZE = 32;

constexpr Int32 FT_SCALE_BITS = 7;
constexpr Int32 FT_QUANT_BITS = 8;
constexpr Int32 L1_QUANT_BITS = 3;

constexpr Int32 SCALE = 253;

constexpr bool DUAL_ACTIVATION = false;
constexpr bool SKIP_L2 = false;

using PSQFeatureSet = MergedMirroredKingBucketInputs<
    MirroredKingSide::ABCD,
    0, 1, 2, 3,
    4, 5, 6, 7,
    8, 8, 9, 9,
    10, 10, 11, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15
>;

using InputFeatureSet = ThreatInputs<PSQFeatureSet>;

using Updates = InputFeatureSet::Updates;

using FeatureTransformer = Sift::FeatureTransformer<L1_SIZE, InputFeatureSet>;
using Accumulator = FeatureTransformer::Accumulator;
using RefreshTable = FeatureTransformer::RefreshTable;

using Output = MaterialCountBucketOutput<8>;

using Arch = PairwiseMultilayerArch<
    InputFeatureSet,
    L1_SIZE,
    L2_SIZE,
    L3_SIZE,
    FT_SCALE_BITS,
    FT_QUANT_BITS,
    L1_QUANT_BITS,
    DUAL_ACTIVATION,
    SKIP_L2,
    Output,
    SCALE
>;

}

using Network = PerspectiveNetwork<NetConfig::FeatureTransformer, NetConfig::Output, NetConfig::Arch>;

namespace NetLoader {

std::byte *loadedData = nullptr;

#if defined(USE_NUMA)
std::unique_ptr<NUMAUniqueAllocation<std::byte>> networkData = nullptr;
std::unique_ptr<NUMAUniqueAllocation<Network>> networks = nullptr;
#else
Network network;
#endif

bool loaded = false;

void init() noexcept {
    const USize networkSize = Network::byteSize();

    assert(EMBEDDED_NETWORK_size >= networkSize);

    loaded = false;
    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }

    const std::byte *ptr = EMBEDDED_NETWORK_data;

#if defined(USE_NUMA)
    networkData = std::make_unique<NUMAUniqueAllocation<std::byte>>(networkSize);
    networks = std::make_unique<NUMAUniqueAllocation<Network>>();

    for (USize node = 0; node < NUMA::nodeCount(); node++) {
        std::byte *target = networkData->getForNode(node);
        std::memcpy(target, ptr, networkSize);
        ByteLoader byteLoader = ByteLoader(target, networkSize);
        if (!networks->getForNode(node)->load(byteLoader)) {
            assert(false);
            return;
        }
    }

    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }
#else
    ByteLoader byteLoader = ByteLoader(ptr, networkSize);
    if (!network.load(byteLoader)) {
        assert(false);
        return;
    }
#endif

    loaded = true;
}

void cleanup() noexcept {
    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }

#if defined(USE_NUMA)
    networkData = nullptr;
    networks = nullptr;
#endif

    loaded = false;
}

const Network *get([[maybe_unused]] USize threadID) noexcept {
#if defined(USE_NUMA)
    return networks->get(threadID);
#else
    return &network;
#endif
}

}

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

};
