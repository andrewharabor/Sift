#pragma once

#include <array>
#include <cassert>
#include <iterator>

#include "constants.hpp"
#include "coordinates.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Clownfish {

enum class MoveType : U16 {
    NORMAL = 0,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
};

class Move {
public:
    constexpr static U16 NULL_MOVE = 0;

    constexpr Move() noexcept : move_(NULL_MOVE), score_(0) {}
    constexpr Move(U16 move) noexcept : move_(move), score_(0) {}

    constexpr Move(Square from, Square to, MoveType type = MoveType::NORMAL, PieceType promotion = PieceType::NONE) noexcept
        : move_(NULL_MOVE), score_(0) {
        assert(from != Square::NONE && to != Square::NONE);

        const U16 typeBits = static_cast<U16>(type);
        U16 promotionBits = 0;
        if (type == MoveType::PROMOTION) {
            assert(promotion >= PieceType(PieceType::KNIGHT) && promotion <= PieceType(PieceType::QUEEN));
            promotionBits = static_cast<U16>((promotion - PieceType(PieceType::KNIGHT)) << 12);
        } else {
            assert(promotion == PieceType::NONE);
        }

        const U16 fromBits = static_cast<U16>(from.index() << 6);
        const U16 toBits = static_cast<U16>(to.index());
        move_ = static_cast<U16>(typeBits | promotionBits | fromBits | toBits);
    }

    constexpr bool operator==(const Move &other) const noexcept { return move_ == other.move_; }
    constexpr bool operator!=(const Move &other) const noexcept { return move_ != other.move_; }

    constexpr Square from() const noexcept { return Square((move_ >> 6) & 0x3F); }
    constexpr Square to() const noexcept { return Square(move_ & 0x3F); }

    constexpr MoveType type() const noexcept { return static_cast<MoveType>(move_ & (3 << 14)); }

    constexpr PieceType promotion() const noexcept {
        if (type() != MoveType::PROMOTION) {
            return PieceType::NONE;
        }
        return PieceType(((move_ >> 12) & 3) + PieceType(PieceType::KNIGHT));
    }

    constexpr void setScore(I16 score) noexcept { score_ = score; }
    constexpr I16 score() const noexcept { return score_; }

    constexpr U16 internal() const noexcept { return move_; }

private:
    U16 move_;
    I16 score_;
};

class MoveList {
public:
    constexpr Move &at(USize index) noexcept {
        assert(index < size_);
        return moveList_[index];
    }

    constexpr const Move &at(USize index) const noexcept {
        assert(index < size_);
        return moveList_[index];
    }

    constexpr Move &operator[](USize index) noexcept { return moveList_[index]; }
    constexpr const Move &operator[](USize index) const noexcept { return moveList_[index]; }

    constexpr Move &front() noexcept {
        assert(size_ > 0);
        return moveList_[0];
    }

    constexpr const Move &front() const noexcept {
        assert(size_ > 0);
        return moveList_[0];
    }

    constexpr Move &back() noexcept {
        assert(size_ > 0);
        return moveList_[size_ - 1];
    }

    constexpr const Move &back() const noexcept {
        assert(size_ > 0);
        return moveList_[size_ - 1];
    }

    constexpr Move *begin() noexcept { return &moveList_[0]; }
    constexpr const Move *begin() const noexcept { return &moveList_[0]; }
    constexpr Move *end() noexcept { return &moveList_[size_]; }
    constexpr const Move *end() const noexcept { return &moveList_[size_]; }

    constexpr void add(const Move &move) noexcept {
        assert(size_ < Constants::MAX_MOVES);
        moveList_[size_++] = move;
    }

    constexpr void add(Move &&move) noexcept {
        assert(size_ < Constants::MAX_MOVES);
        moveList_[size_++] = std::move(move);
    }

    constexpr void clear() noexcept { size_ = 0; }

    constexpr USize size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr USize find(const Move &move) const noexcept {
        for (USize i = 0; i < size_; i++) {
            if (moveList_[i] == move) {
                return i;
            }
        }
        return size_;
    }

private:
    std::array<Move, Constants::MAX_MOVES> moveList_;
    USize size_ = 0;

};

}
