#pragma once

#include <array>

#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "types.hpp"

namespace Sift {
    class Arch {
    public:
        static constexpr USize PSQ_FEATURES = 704;  // FIXME: 768
        static constexpr USize THREAT_FEATURES = 64368;
        static constexpr Int64 THREAT_OFFSET = 4560;

        static constexpr bool MERGED_KINGS = true;  // FIXME: remove

        static constexpr std::array<USize, 64> KING_BUCKET_LAYOUT = {
            // clang-format off
        0, 1, 2, 3, 3, 2, 1, 0,
        4, 5, 6, 7, 7, 6, 5, 4,
        8, 9, 10, 11, 11, 10, 9, 8,
        8, 9, 10, 11, 11, 10, 9, 8,
        12, 12, 13, 13, 13, 13, 12, 12,
        12, 12, 13, 13, 13, 13, 12, 12,
        14, 14, 15, 15, 15, 15, 14, 14,
        14, 14, 15, 15, 15, 15, 14, 14,
            // clang-format on
        };

        static constexpr USize KING_BUCKET_COUNT = 16;
        static constexpr USize REFRESH_TABLE_SIZE = KING_BUCKET_COUNT * 2;

        static constexpr USize L1_SIZE = 1024;
        static constexpr USize L2_SIZE = 32;
        static constexpr USize L3_SIZE = 64;

        static constexpr Int32 FT_SCALE_BITS = 7;
        static constexpr Int32 FT_QUANT_BITS = 8;
        static constexpr Int32 L1_QUANT_BITS = 7;

        static constexpr Int32 SCALE = 400;

        static constexpr bool DUAL_ACTIVATION = true;  // FIXME: remove
        static constexpr bool SKIP_L2 = true;          // FIXME: remove

        static constexpr USize OUTPUT_BUCKET_COUNT = 8;

        static constexpr bool needsMirror(Square kingSquare) noexcept { return kingSquare.file() > File::D; }

        static constexpr Square mirror(Square square, Square kingSquare) noexcept {
            return (needsMirror(kingSquare)) ? square.mirrored() : square;
        }

        static constexpr USize kingBucket(Color color, Square kingSquare) noexcept {
            kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
            return Arch::KING_BUCKET_LAYOUT[kingSquare.index()];
        }

        static constexpr USize refreshTableIdx(Color color, Square kingSquare) noexcept {
            kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
            return Arch::KING_BUCKET_LAYOUT[kingSquare.index()] * 2 + needsMirror(kingSquare);
        }

        static constexpr bool needsRefresh(Color color, Square prevKingSquare, Square kingSquare) noexcept {
            assert(color != Color::NONE);
            assert(prevKingSquare != Square::NONE);
            assert(kingSquare != Square::NONE);

            if (needsMirror(prevKingSquare) != needsMirror(kingSquare)) { return true; }

            prevKingSquare = (color == Color::WHITE) ? prevKingSquare : prevKingSquare.flipped();
            kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
            return Arch::KING_BUCKET_LAYOUT[prevKingSquare.index()] != Arch::KING_BUCKET_LAYOUT[kingSquare.index()];
        }

        static constexpr USize outputBucket(const Position& position) noexcept {
            return (position.occupied().count() - 2) / (32 / Arch::OUTPUT_BUCKET_COUNT);
        }

    private:
    };
}
