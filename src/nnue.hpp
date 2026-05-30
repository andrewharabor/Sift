#pragma once

#include <cassert>

#include "arch.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "simd.hpp"
#include "types.hpp"


namespace Syft {

struct InputFeature {
    Piece piece;
    Square square;

    constexpr InputFeature() noexcept : piece(), square() {}

    constexpr InputFeature(Piece piece, Square square) noexcept : piece(piece), square(square) {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
    }

    constexpr USize index(Color color) const noexcept {
        assert(color != Color::NONE);

        const USize colorIndex = static_cast<USize>(color);
        const USize pieceTypeIndex = static_cast<USize>(piece.type());
        const USize pieceColorIndex = static_cast<USize>(piece.color());
        const USize squareIndex = (color == Color::BLACK) ? static_cast<USize>(square.flipped().index()) : static_cast<USize>(square.index());

        return squareIndex + (pieceTypeIndex + ((pieceColorIndex ^ colorIndex) * 6)) * 64;
    }
};

}
