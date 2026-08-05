#pragma once

#include <array>
#include <span>
#include <utility>

#include "coords.hpp"
#include "piece.hpp"
#include "simd.hpp"
#include "types.hpp"


namespace Sift {

using Bit = UInt8;
using BitRays = UInt64;

class Geometry {
public:
    struct Bits {
        static constexpr Bit WHITE_PAWN = 1 << 0;
        static constexpr Bit BLACK_PAWN = 1 << 1;
        static constexpr Bit KNIGHT = 1 << 2;
        static constexpr Bit BISHOP = 1 << 3;
        static constexpr Bit ROOK = 1 << 4;
        static constexpr Bit QUEEN = 1 << 5;
        static constexpr Bit KING = 1 << 6;
    };

    static constexpr MultiArray<UInt8, 64, 64> PERMUTATIONS = [] {
        constexpr std::array<UInt8, 64> OFFSETS = {
            0x1F, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
            0x21, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
            0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0xF2, 0xF1, 0xE2, 0xD3, 0xC4, 0xB5, 0xA6, 0x97,
            0xE1, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
            0xDF, 0xEF, 0xDE, 0xCD, 0xBC, 0xAB, 0x9A, 0x89,
            0xEE, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9,
            0x0E, 0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69,
        };

        MultiArray<UInt8, 64, 64> perm = {};
        for (UInt8 square = 0; square < 64; square++) {
            for (UInt8 i = 0; i < 64; i++) {
                const UInt8 wideSquare = square + (square & 0x38);
                const UInt8 wideResult = wideSquare + OFFSETS[i];
                const UInt8 result = ((wideResult & 0x70) >> 1) | (wideResult & 0x07);
                const bool valid = (wideResult & 0x88) == 0;
                perm[square][i] = (valid) ? result : 0x80;
            }
        }
        return perm;
    }();

    static constexpr std::array<Bit, 16> PIECE_BITS = [] {
        std::array<Bit, 16> bits = {};
        bits[static_cast<USize>(Piece::WHITE_PAWN)] = Bits::WHITE_PAWN;
        bits[static_cast<USize>(Piece::BLACK_PAWN)] = Bits::BLACK_PAWN;
        bits[static_cast<USize>(Piece::WHITE_KNIGHT)] = Bits::KNIGHT;
        bits[static_cast<USize>(Piece::BLACK_KNIGHT)] = Bits::KNIGHT;
        bits[static_cast<USize>(Piece::WHITE_BISHOP)] = Bits::BISHOP;
        bits[static_cast<USize>(Piece::BLACK_BISHOP)] = Bits::BISHOP;
        bits[static_cast<USize>(Piece::WHITE_ROOK)] = Bits::ROOK;
        bits[static_cast<USize>(Piece::BLACK_ROOK)] = Bits::ROOK;
        bits[static_cast<USize>(Piece::WHITE_QUEEN)] = Bits::QUEEN;
        bits[static_cast<USize>(Piece::BLACK_QUEEN)] = Bits::QUEEN;
        bits[static_cast<USize>(Piece::WHITE_KING)] = Bits::KING;
        bits[static_cast<USize>(Piece::BLACK_KING)] = Bits::KING;
        bits[static_cast<USize>(Piece::NONE)] = 0;
        return bits;
    }();

    static constexpr std::array<BitRays, 12> OUTGOING_THREATS = [] {
        std::array<BitRays, 12> threats = {};
        threats[static_cast<USize>(Piece::WHITE_PAWN)] = 0x0200000000000200;
        threats[static_cast<USize>(Piece::BLACK_PAWN)] = 0x0000020002000000;
        threats[static_cast<USize>(Piece::WHITE_KNIGHT)] = 0x0101010101010101;
        threats[static_cast<USize>(Piece::BLACK_KNIGHT)] = 0x0101010101010101;
        threats[static_cast<USize>(Piece::WHITE_BISHOP)] = 0xFE00FE00FE00FE00;
        threats[static_cast<USize>(Piece::BLACK_BISHOP)] = 0xFE00FE00FE00FE00;
        threats[static_cast<USize>(Piece::WHITE_ROOK)] = 0x00FE00FE00FE00FE;
        threats[static_cast<USize>(Piece::BLACK_ROOK)] = 0x00FE00FE00FE00FE;
        threats[static_cast<USize>(Piece::WHITE_QUEEN)] = 0xFEFEFEFEFEFEFEFE;
        threats[static_cast<USize>(Piece::BLACK_QUEEN)] = 0xFEFEFEFEFEFEFEFE;
        threats[static_cast<USize>(Piece::WHITE_KING)] = 0;
        threats[static_cast<USize>(Piece::BLACK_KING)] = 0;
        return threats;
    }();

    static constexpr std::array<Bit, 64> INCOMING_THREAT_MASK = [] {
        constexpr Bit KNIGHT = Bits::KNIGHT;
        constexpr Bit ORTHOGONAL = Bits::ROOK | Bits::QUEEN;
        constexpr Bit DIAGONAL = Bits::BISHOP | Bits::QUEEN;
        constexpr Bit ORTHOGONAL_NEAR = ORTHOGONAL;
        constexpr Bit WHITE_PAWN_NEAR = Bits::WHITE_PAWN | DIAGONAL;
        constexpr Bit BLACK_PAWN_NEAR = Bits::BLACK_PAWN | DIAGONAL;

        std::array<Bit, 64> threats = {
            KNIGHT, ORTHOGONAL_NEAR, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            KNIGHT, BLACK_PAWN_NEAR, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            KNIGHT, ORTHOGONAL_NEAR, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            KNIGHT, WHITE_PAWN_NEAR, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            KNIGHT, ORTHOGONAL_NEAR, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            KNIGHT, WHITE_PAWN_NEAR, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            KNIGHT, ORTHOGONAL_NEAR, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            KNIGHT, BLACK_PAWN_NEAR, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL
        };
        return threats;
    }();

    static constexpr std::array<Bit, 64> INCOMING_SLIDER_MASK = [] {
        constexpr Bit ORTHOGONAL = Bits::ROOK | Bits::QUEEN;
        constexpr Bit DIAGONAL = Bits::BISHOP | Bits::QUEEN;
        constexpr Bit NONE = 0x80;
        std::array<Bit, 64> sliders = {
            NONE, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            NONE, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            NONE, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            NONE, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            NONE, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            NONE, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL,
            NONE, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL, ORTHOGONAL,
            NONE, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL
        };
        return sliders;
    }();

#if defined(USE_AVX512)

    struct Vector {
        __m512i raw;

        inline Vector flipped() const noexcept { return Vector{_mm512_shuffle_i64x2(raw, raw, 0b01001110)}; }
    };

    struct Permutation {
        Vector indices;
        BitRays valid;
    };

    static inline Permutation permutation(Square square) noexcept {
        const auto indices = _mm512_loadu_si512(PERMUTATIONS[static_cast<USize>(square.index())].data());
        const auto valid = _mm512_testn_epi8_mask(indices, _mm512_set1_epi8(0x80));
        return Permutation{Vector{indices}, valid};
    };

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox) noexcept {
        const auto pieceBits = _mm512_broadcast_i32x4(_mm_loadu_si128(reinterpret_cast<const __m128i *>(PIECE_BITS.data())));
        const auto maskedMailbox = _mm512_loadu_si512(mailbox.data());
        const auto permuted = _mm512_permutexvar_epi8(permutation.indices.raw, maskedMailbox);
        const auto bits = _mm512_maskz_shuffle_epi8(permutation.valid, pieceBits, permuted);
        return {Vector{permuted}, Vector{bits}};
    }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox, Square ignore) noexcept {
        const auto pieceBits = _mm512_broadcast_i32x4(_mm_loadu_si128(reinterpret_cast<const __m128i *>(PIECE_BITS.data())));
        const auto maskedMailbox = _mm512_mask_blend_epi8(static_cast<UInt64>(ignore.index()), _mm512_loadu_si512(mailbox.data()), _mm512_set1_epi8(static_cast<UInt8>(Piece::NONE)));
        const auto permuted = _mm512_permutexvar_epi8(permutation.indices.raw, maskedMailbox);
        const auto bits = _mm512_maskz_shuffle_epi8(permutation.valid, pieceBits, permuted);
        return {Vector{permuted}, Vector{bits}};
    }

    static inline BitRays closestOccupied(Vector bits) noexcept {
        const BitRays occupied = _mm512_test_epi8_mask(bits.raw, bits.raw);
        const BitRays o = occupied | 0x8181818181818181;
        return (o ^ (o - 0x0303030303030303)) & occupied;
    }

    static inline BitRays rayFill(BitRays rays) noexcept {
        rays = (rays + 0x7E7E7E7E7E7E7E7E) & 0x8080808080808080;
        return rays - (rays >> 7);
    }

    static inline BitRays outgoingThreats(Piece piece, BitRays closest) noexcept { return OUTGOING_THREATS[static_cast<USize>(piece.internal())] & closest; }

    static inline BitRays incomingAttackers(Vector bits, BitRays closest) noexcept {
        const auto mask = _mm512_loadu_si512(INCOMING_THREAT_MASK.data());
        return _mm512_test_epi8_mask(bits.raw, mask) & closest;
    }

    static inline BitRays incomingSliders(Vector bits, BitRays closest) noexcept {
        const auto mask = _mm512_loadu_si512(INCOMING_SLIDER_MASK.data());
        return _mm512_test_epi8_mask(bits.raw, mask) & closest & 0xFEFEFEFEFEFEFEFE;
    }

#elif defined(USE_AVX2)

    struct Vector {
        std::array<__m256i, 2> raw;

        inline Vector flipped() const noexcept { return Vector{{ raw[1], raw[0] }}; }
        inline BitRays mask() const noexcept { return static_cast<BitRays>(_mm256_movemask_epi8(raw[0])) | (static_cast<BitRays>(_mm256_movemask_epi8(raw[1])) << 32); }
        static inline Vector load(const void *ptr) noexcept { return Vector{{ _mm256_loadu_si256(static_cast<const __m256i *>(ptr)), _mm256_loadu_si256(static_cast<const __m256i *>(ptr) + 1) }}; }

        template<typename TYPE>
        static inline Vector cast(const TYPE &value) noexcept requires(sizeof(TYPE) == sizeof(__m256i) * 2) { return load(&value); }
    };

    struct Permutation {
        Vector indices;
        Vector invalid;
    };

    static inline Permutation permuation(Square square) noexcept {
        const auto indices = Vector::cast(PERMUTATIONS[static_cast<USize>(square.index())]);
        const Vector valid{{ _mm256_cmpeq_epi8(indices.raw[0], _mm256_set1_epi8(0x80)), _mm256_cmpeq_epi8(indices.raw[1], _mm256_set1_epi8(0x80)) }};
        return Permutation{indices, valid};
    }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, Vector maskedMailbox) noexcept {
        const auto pieceBits = _mm256_broadcastsi128_si256(_mm_loadu_si128(reinterpret_cast<const __m128i *>(PIECE_BITS.data())));

        const auto halfSwizzler = [](__m256i bytes0, __m256i bytes1, __m256i indices) {
            const auto mask0 = _mm256_slli_epi64(indices, 2);
            const auto mask1 = _mm256_slli_epi64(indices, 3);
            const auto lolo0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x00), indices);
            const auto hihi0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x11), indices);
            const auto x = _mm256_blendv_epi8(lolo0, hihi0, mask1);
            const auto lolo1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x00), indices);
            const auto hihi1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x11), indices);
            const auto y = _mm256_blendv_epi8(lolo1, hihi1, mask1);
            return _mm256_blendv_epi8(x, y, mask0);
        };

        const Vector permuted{{ halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indices.raw[0]), halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indices.raw[1]) }};
        const Vector bits{{ _mm256_andnot_si256(permutation.invalid.raw[0], _mm256_shuffle_epi8(pieceBits, permuted.raw[0])), _mm256_andnot_si256(permutation.invalid.raw[1], _mm256_shuffle_epi8(pieceBits, permuted.raw[1])) }};
        return {permuted, bits};
    }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox) noexcept { return permuteMailbox(permutation, Vector::load(mailbox.data())); }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox, Square ignore) noexcept {
        const auto iota = Vector::cast(
            std::array<UInt8, 64>{{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
            22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
            44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 }}
        );
        const auto ignoreVec = _mm256_set1_epi8(static_cast<Int8>(ignore.index()));
        const auto noneVec = _mm256_set1_epi8(static_cast<Int8>(Piece::NONE));
        const auto mailboxVec = Vector::load(mailbox.data());
        const Vector maskedMailbox{{ _mm256_blendv_epi8(mailboxVec.raw[0], noneVec, _mm256_cmpeq_epi8(iota.raw[0], ignoreVec)), _mm256_blendv_epi8(mailboxVec.raw[1], noneVec, _mm256_cmpeq_epi8(iota.raw[1], ignoreVec)) }};
        return permuteMailbox(permutation, maskedMailbox);
    }

    static inline BitRays closestOccupied(Vector bits) noexcept {
        const Vector unoccupied{{ _mm256_cmpeq_epi8(bits.raw[0], _mm256_setzero_si256()), _mm256_cmpeq_epi8(bits.raw[1], _mm256_setzero_si256()) }};
        const BitRays occupied = ~unoccupied.mask();
        const BitRays o = occupied | 0x8181818181818181;
        return (o ^ (o - 0x0303030303030303)) & occupied;
    }

    static inline BitRays rayFill(BitRays rays) noexcept {
        rays = (rays + 0x7E7E7E7E7E7E7E7E) & 0x8080808080808080;
        return rays - (rays >> 7);
    }

    static inline BitRays outgoingThreats(Piece piece, BitRays closest) noexcept { return OUTGOING_THREATS[static_cast<USize>(piece.internal())] & closest; }

    static inline BitRays incomingAttackers(Vector bits, BitRays closest) noexcept {
        const auto mask = Vector::cast(INCOMING_THREAT_MASK);
        const Vector vec{{ _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()), _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()) }};
        return ~vec.mask() & closest;
    }

    static inline BitRays incomingSliders(Vector bits, BitRays closest) noexcept {
        const auto mask = Vector::cast(INCOMING_SLIDER_MASK);
        const Vector vec{{ _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()), _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()) }};
        return ~vec.mask() & closest & 0xFEFEFEFEFEFEFEFE;
    }

#elif defined(USE_NEON)

    struct Vector {
        uint8x16x4_t raw;

        explicit inline Vector(uint8x16x4_t raw) noexcept : raw(raw) {}
        explicit inline Vector(uint8x16_t value0, uint8x16_t value1, uint8x16_t value2, uint8x16_t value3) noexcept : raw({value0, value1, value2, value3}) {}

        inline Vector flipped() const noexcept { return Vector{raw.val[2], raw.val[3], raw.val[0], raw.val[1]}; }

        inline BitRays mask() const noexcept {
            const uint8x16_t mask{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
            const auto vec = vpaddq_u8(vpaddq_u8(vandq_u8(raw.val[0], mask), vandq_u8(raw.val[1], mask)), vpaddq_u8(vandq_u8(raw.val[2], mask), vandq_u8(raw.val[3], mask)));
            return vgetq_lane_u64(vreinterpretq_u64_u8(vpaddq_u8(vec, vec)), 0);
        }

        static inline Vector load(const void *ptr) { return Vector(vld1q_u8_x4(static_cast<const UInt8 *>(ptr))); }

        template<typename TYPE>
        static inline Vector cast(const TYPE &value) requires(sizeof(TYPE) == sizeof(uint8x16x4_t)) { return load(&value); }

        inline uint8x16_t &operator[](USize index) { return raw.val[index]; }
        inline  uint8x16_t operator[](USize index) const { return raw.val[index]; }
    };

    struct Permutation {
        Vector indexes;
        Vector valid;
    };


    static inline Permutation permutation(Square square) {
        const auto indices = Vector::load(PERMUTATIONS[static_cast<USize>(square)].data());
        const auto valid = Vector{vmvnq_u8(vshrq_n_s8(indices[0], 7)), vmvnq_u8(vshrq_n_s8(indices[1], 7)), vmvnq_u8(vshrq_n_s8(indices[2], 7)), vmvnq_u8(vshrq_n_s8(indices[3], 7))};
        return Permutation{indices, valid};
    }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, Vector mailbox) {
        const auto pieceBits = vld1q_u8(reinterpret_cast<const UInt8 *>(PIECE_BITS.data()));
        const Vector permuted = Vector{vqtbl4q_u8(mailbox.raw, permutation.indexes[0]), vqtbl4q_u8(mailbox.raw, permutation.indexes[1]), vqtbl4q_u8(mailbox.raw, permutation.indexes[2]), vqtbl4q_u8(mailbox.raw, permutation.indexes[3])};
        const Vector bits = Vector{vandq_u8(vqtbl1q_u8(pieceBits, permuted[0]), permutation.valid[0]), vandq_u8(vqtbl1q_u8(pieceBits, permuted[1]), permutation.valid[1]), vandq_u8(vqtbl1q_u8(pieceBits, permuted[2]), permutation.valid[2]), vandq_u8(vqtbl1q_u8(pieceBits, permuted[3]), permutation.valid[3])};
        return {permuted, bits};
    }

    static inline std::pair<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox) { return permuteMailbox(permutation, Vector::load(mailbox.data())); }

    static inline std::tuple<Vector, Vector> permuteMailbox(const Permutation &permutation, const std::span<const Piece, 64> mailbox, Square ignore) {
        const auto iota = Vector::cast(
            std::array<UInt8, 64>{{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
            22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
            44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 }}
        );
        const auto ignoreVec = vdupq_n_u8(static_cast<UInt8>(ignore.index()));
        const auto noneVec = vdupq_n_u8(static_cast<UInt8>(Piece::NONE));
        const auto mailboxVec = Vector::load(mailbox.data());
        const Vector maskedMailbox{vbslq_u8(vceqq_u8(iota[0], ignoreVec), noneVec, mailboxVec[0]), vbslq_u8(vceqq_u8(iota[1], ignoreVec), noneVec, mailboxVec[1]), vbslq_u8(vceqq_u8(iota[2], ignoreVec), noneVec, mailboxVec[2]), vbslq_u8(vceqq_u8(iota[3], ignoreVec), noneVec, mailboxVec[3])};
        return permuteMailbox(permutation, maskedMailbox);
    }

    static inline BitRays closestOccupied(Vector bits) {
        const Vector occupiedVec{vtstq_u8(bits[0], bits[0]), vtstq_u8(bits[1], bits[1]), vtstq_u8(bits[2], bits[2]), vtstq_u8(bits[3], bits[3])};
        const auto occupied = occupiedVec.mask();
        const BitRays o = occupied | 0x8181818181818181;
        return (o ^ (o - 0x0303030303030303)) & occupied;
    }

    static inline BitRays rayFill(BitRays rays) {
        rays = (rays + 0x7E7E7E7E7E7E7E7E) & 0x8080808080808080;
        return rays - (rays >> 7);
    }

    static inline BitRays outgoingThreats(Piece piece, BitRays closest) { return OUTGOING_THREATS[static_cast<USize>(piece.internal())] & closest; }

    static inline BitRays incomingAttackers(Vector bits, BitRays closest) {
        const auto mask = Vector::load(INCOMING_THREAT_MASK.data());
        const Vector vec{vtstq_u8(bits[0], mask[0]), vtstq_u8(bits[1], mask[1]), vtstq_u8(bits[2], mask[2]), vtstq_u8(bits[3], mask[3])};
        return vec.mask() & closest;
    }

    static inline BitRays incomingSliders(Vector bits, BitRays closest) {
        const auto mask = Vector::load(INCOMING_SLIDER_MASK.data());
        const Vector vec{vtstq_u8(bits[0], mask[0]), vtstq_u8(bits[1], mask[1]), vtstq_u8(bits[2], mask[2]), vtstq_u8(bits[3], mask[3])};
        return vec.mask() & closest & 0xFEFEFEFEFEFEFEFE;
    }

#else

#error "Unsupported architecture!"

#endif

};

}
