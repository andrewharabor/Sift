#pragma once

#include <array>
#include <bit>
#include <cassert>

#include "coords.hpp"
#include "piece.hpp"
#include "types.hpp"

namespace Sift {
    class JSF64RNG {
    public:
        explicit constexpr JSF64RNG(UInt64 seed) noexcept : b_(seed), c_(seed), d_(seed) {
            for (USize i = 0; i < 20; i++) { next(); }
        }

        constexpr UInt64 next() noexcept {
            const UInt64 e = a_ - std::rotl(b_, 7);
            a_ = b_ ^ std::rotl(c_, 13);
            b_ = c_ + std::rotl(d_, 37);
            c_ = d_ + e;
            d_ = e + a_;
            return d_;
        }

    private:
        UInt64 a_ = 0xF1EA5EED;
        UInt64 b_;
        UInt64 c_;
        UInt64 d_;
    };

    class Hash {
    public:
        static constexpr UInt64 piece(Piece piece, Square square) noexcept {
            assert(piece != Piece::NONE && square != Square::NONE);
            return KEYS[(square.index() * 12) + piece.index()];
        }

        static constexpr UInt64 sideToMove() noexcept { return KEYS[768]; }

        static constexpr UInt64 castlingIndex(UInt8 castlingIndex) noexcept {
            assert(castlingIndex < 4);
            return KEYS[769 + castlingIndex];
        }

        static constexpr UInt64 castling(UInt8 castlingRights) noexcept {
            assert(castlingRights < 16);
            return CASTLING_KEYS[castlingRights];
        }

        static constexpr UInt64 enPassant(File file) noexcept {
            assert(file != File::NONE);
            return KEYS[773 + file.index()];
        }

    private:
        static constexpr std::array<UInt64, 781> KEYS = [] {
            constexpr UInt64 SEED = 0xD06C659954EC904A;
            std::array<UInt64, 781> keys = {};
            JSF64RNG rng = JSF64RNG(SEED);
            for (UInt64& key : keys) { key = rng.next(); }
            return keys;
        }();

        static constexpr std::array<UInt64, 16> CASTLING_KEYS = [] {
            std::array<UInt64, 16> keys = {};
            for (UInt8 rights = 0; rights < 16; rights++) {
                UInt64 key = 0;
                for (UInt64 bit = 0; bit < 4; bit++) {
                    if (rights & (1 << bit)) { key ^= KEYS[769 + bit]; }
                }
                keys[rights] = key;
            }
            return keys;
        }();
    };
}
