#pragma once

#include <array>

#include "bitboard.hpp"
#include "color.hpp"
#include "constants.hpp"
#include "coordinates.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"


namespace Clownfish {

class NNUE {
public:
    constexpr NNUE() : hiddenWeights_{}, hiddenBiases_{}, outputWeights{}, outputBias_() {}

    // constexpr std::int32_t evaluate(Bitboard occupied, const std::array<Piece, 64> &board) const noexcept {
    //     std::int32_t score = SIMD::fullyConnected(accumulator_.data(Color::WHITE), outputWeights[static_cast<std::size_t>(Color::WHITE)]);
    //     score += SIMD::fullyConnected(accumulator_.data(Color::BLACK), outputWeights[static_cast<std::size_t>(Color::BLACK)]);
    //     score /= Constants::NNUE_QUANT_A;
    //     score += outputBias_;
    //     score *= Constants::NNUE_SCALE;
    //     score /= (Constants::NNUE_QUANT_A * Constants::NNUE_QUANT_B);
    //     return score;
    // }

    void refresh(Accumulator &accumulator, Bitboard occupied, const std::array<Piece, 64> &board) noexcept {
        for (Color color : {Color::WHITE, Color::BLACK}) {
            AlignedVector &data = accumulator.data(color);
            data = hiddenBiases_;
            Bitboard pieces = occupied;
            while (pieces) {
                const Square square = static_cast<int>(pieces.pop());
                const Piece piece = board[static_cast<std::size_t>(square)];
                const int featureIndex = NNUE::featureIndex(piece, square, color);
                SIMD::add(hiddenWeights_[featureIndex], data);
            }
        }
    }

    void update(const Position &position) noexcept {
        const Bitboard rootOccupied = position.rootOccupied();
        const std::array<Piece, 64> &rootBoard = position.rootBoard();

        if (length_ == 0 || cachedRootOccupied_ != rootOccupied || cachedRootBoard_ != rootBoard) {
            refresh(accumulatorHistory_[0], rootOccupied, rootBoard);
            cachedRootOccupied_ = rootOccupied;
            cachedRootBoard_ = rootBoard;
            length_ = 1;
        }

        const std::size_t depth = position.depth();
        if (length_ > depth + 1) {
            length_ = depth + 1;
        }

        const std::array<Position::BoardChanges, Constants::MAX_GAME_LENGTH> &changesHistory = position.changesHistory();
        while (length_ <= depth) {
            const Position::BoardChanges &changes = changesHistory[length_ - 1];
            Accumulator &accumulator = accumulatorHistory_[length_];
            accumulator = accumulatorHistory_[length_ - 1];
            for (Color color : {Color::WHITE, Color::BLACK}) {
                for (std::size_t i = 0; i < changes.size(); i++) {
                    const Position::BoardChanges::Change &change = changes.changes()[i];
                    const int featureIndex = NNUE::featureIndex(change.piece, change.square, color);
                    if (change.added) {
                        SIMD::add(hiddenWeights_[featureIndex], accumulator.data(color));
                    } else {
                        SIMD::sub(hiddenWeights_[featureIndex], accumulator.data(color));
                    }
                }
            }
            length_++;
        }
    }

    static int featureIndex(Piece piece, Square square, Color color) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE && color != Color::NONE);

        const int colorIndex = static_cast<int>(color);
        const int pieceTypeIndex = static_cast<int>(piece.type());
        const int pieceColorIndex = static_cast<int>(piece.color());

        if (color == Color::BLACK) {
            square.mirror();
        }
        const int squareIndex = static_cast<int>(square);

        return squareIndex + (pieceTypeIndex + ((pieceColorIndex ^ colorIndex) * 6)) * 64;
    }

private:
    std::size_t length_;
    std::array<Accumulator, Constants::MAX_GAME_LENGTH + 1> accumulatorHistory_;

    Bitboard cachedRootOccupied_;
    std::array<Piece, 64> cachedRootBoard_;

    std::array<AlignedVector, Constants::NNUE_INPUT_SIZE> hiddenWeights_;
    AlignedVector hiddenBiases_;
    std::array<AlignedVector, 2> outputWeights;
    std::int16_t outputBias_;
};

}
