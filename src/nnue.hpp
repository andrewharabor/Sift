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
                const std::size_t featureIndex = NNUE::featureIndex(piece, square, color);
                SIMD::add(data, hiddenWeights_[featureIndex]);
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
                AlignedVector &data = accumulator.data(color);
                if (changes.sizeAdd() == 1 && changes.sizeRemove() == 0) {
                    const Position::BoardChanges::Change &change = changes.additions()[0];
                    const std::size_t index = NNUE::featureIndex(change.piece, change.square, color);
                    SIMD::add(data, hiddenWeights_[index]);
                } else if (changes.sizeAdd() == 0 && changes.sizeRemove() == 1) {
                    const Position::BoardChanges::Change &change = changes.removals()[0];
                    const std::size_t index = NNUE::featureIndex(change.piece, change.square, color);
                    SIMD::sub(data, hiddenWeights_[index]);
                } else if (changes.sizeAdd() == 1 && changes.sizeRemove() == 1) {
                    const Position::BoardChanges::Change &add = changes.additions()[0];
                    const Position::BoardChanges::Change &remove = changes.removals()[0];
                    const std::size_t addIndex = NNUE::featureIndex(add.piece, add.square, color);
                    const std::size_t removeIndex = NNUE::featureIndex(remove.piece, remove.square, color);
                    SIMD::addSub(data, hiddenWeights_[addIndex], hiddenWeights_[removeIndex]);
                } else if (changes.sizeAdd() == 1 && changes.sizeRemove() == 2) {
                    const Position::BoardChanges::Change &add = changes.additions()[0];
                    const Position::BoardChanges::Change &remove1 = changes.removals()[0];
                    const Position::BoardChanges::Change &remove2 = changes.removals()[1];
                    const std::size_t addIndex = NNUE::featureIndex(add.piece, add.square, color);
                    const std::size_t removeIndex1 = NNUE::featureIndex(remove1.piece, remove1.square, color);
                    const std::size_t removeIndex2 = NNUE::featureIndex(remove2.piece, remove2.square, color);
                    SIMD::addSub2(data, hiddenWeights_[addIndex], hiddenWeights_[removeIndex1], hiddenWeights_[removeIndex2]);
                } else if (changes.sizeAdd() == 2 && changes.sizeRemove() == 2) {
                    const Position::BoardChanges::Change &add1 = changes.additions()[0];
                    const Position::BoardChanges::Change &add2 = changes.additions()[1];
                    const Position::BoardChanges::Change &remove1 = changes.removals()[0];
                    const Position::BoardChanges::Change &remove2 = changes.removals()[1];
                    const std::size_t addIndex1 = NNUE::featureIndex(add1.piece, add1.square, color);
                    const std::size_t addIndex2 = NNUE::featureIndex(add2.piece, add2.square, color);
                    const std::size_t removeIndex1 = NNUE::featureIndex(remove1.piece, remove1.square, color);
                    const std::size_t removeIndex2 = NNUE::featureIndex(remove2.piece, remove2.square, color);
                    SIMD::add2Sub2(data, hiddenWeights_[addIndex1], hiddenWeights_[addIndex2], hiddenWeights_[removeIndex1], hiddenWeights_[removeIndex2]);
                } else {
                    for (std::size_t i = 0; i < changes.sizeAdd(); i++) {
                        const Position::BoardChanges::Change &change = changes.additions()[i];
                        const std::size_t index = NNUE::featureIndex(change.piece, change.square, color);
                        SIMD::add(data, hiddenWeights_[index]);
                    }
                    for (std::size_t i = 0; i < changes.sizeRemove(); i++) {
                        const Position::BoardChanges::Change &change = changes.removals()[i];
                        const std::size_t index = NNUE::featureIndex(change.piece, change.square, color);
                        SIMD::sub(data, hiddenWeights_[index]);
                    }
                }
            }
            length_++;
        }
    }

    static std::size_t featureIndex(Piece piece, Square square, Color color) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE && color != Color::NONE);

        const int colorIndex = static_cast<int>(color);
        const int pieceTypeIndex = static_cast<int>(piece.type());
        const int pieceColorIndex = static_cast<int>(piece.color());

        if (color == Color::BLACK) {
            square.mirror();
        }
        const int squareIndex = static_cast<int>(square);

        return static_cast<std::size_t>(squareIndex + (pieceTypeIndex + ((pieceColorIndex ^ colorIndex) * 6)) * 64);
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
