#pragma once

#include <cassert>

#include "bitboard.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "types.hpp"

namespace Sift {
    struct SingleBucketOutput {
    public:
        static constexpr USize BUCKET_COUNT = 1;

        static constexpr USize bucket(const Position&) noexcept { return 0; }
    };

    struct OppColoredBishopBucketOutput {
    public:
        static constexpr USize BUCKET_COUNT = 2;

        static constexpr USize bucket(const Position& position) noexcept {
            const bool bishops = !position.pieces(Piece::WHITE_BISHOP).empty() && !position.pieces(Piece::BLACK_BISHOP).empty();
            const bool oppColored = (position.pieces(Piece::WHITE_BISHOP) & LIGHT_SQUARES).empty() !=
                                    (position.pieces(Piece::BLACK_BISHOP) & LIGHT_SQUARES).empty();
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

        static constexpr USize bucket(const Position& position) noexcept { return (position.occupied().count() - 2) / DIV; }

    private:
        static constexpr USize DIV = 32 / BUCKET_COUNT;
    };
}
