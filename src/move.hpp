#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>

#include "constants.hpp"
#include "coordinates.hpp"
#include "piece.hpp"


namespace Clownfish {

class Move {
public:
    enum class MoveType : std::uint16_t {
        NORMAL = 0,
        PROMOTION = 1 << 14,
        EN_PASSANT = 2 << 14,
        CASTLING = 3 << 14
    };

    constexpr static MoveType NORMAL = MoveType::NORMAL;
    constexpr static MoveType PROMOTION = MoveType::PROMOTION;
    constexpr static MoveType EN_PASSANT = MoveType::EN_PASSANT;
    constexpr static MoveType CASTLING = MoveType::CASTLING;

    constexpr static std::uint16_t NULL_MOVE = 0;

    constexpr Move() noexcept : move_(NULL_MOVE), score_(0) {}
    constexpr Move(std::uint16_t move) noexcept : move_(move), score_(0) {}

    constexpr Move(Square from, Square to, MoveType type = MoveType::NORMAL, PieceType promotion = PieceType::NONE) noexcept
        : move_(NULL_MOVE), score_(0) {
        assert(from != Square::NONE && to != Square::NONE);

        const std::uint16_t typeBits = static_cast<std::uint16_t>(type);
        std::uint16_t promotionBits = 0;
        if (type == MoveType::PROMOTION) {
            assert(promotion >= PieceType(PieceType::KNIGHT) && promotion <= PieceType(PieceType::QUEEN));
            promotionBits = static_cast<std::uint16_t>((promotion - PieceType(PieceType::KNIGHT)) << 12);
        } else {
            assert(promotion == PieceType::NONE);
        }

        const std::uint16_t fromBits = static_cast<std::uint16_t>(from.index() << 6);
        const std::uint16_t toBits = static_cast<std::uint16_t>(to.index());
        move_ = static_cast<std::uint16_t>(typeBits | promotionBits | fromBits | toBits);
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

    constexpr void setScore(std::int16_t score) noexcept { score_ = score; }
    constexpr std::int16_t score() const noexcept { return score_; }

    constexpr std::uint16_t internal() const noexcept { return move_; }

private:
    std::uint16_t move_;
    std::int16_t score_;
};

class MoveList {
public:
    constexpr Move &at(std::size_t index) noexcept {
        assert(index < size_);
        return moveList_[index];
    }

    constexpr const Move &at(std::size_t index) const noexcept {
        assert(index < size_);
        return moveList_[index];
    }

    constexpr Move &operator[](std::size_t index) noexcept { return moveList_[index]; }
    constexpr const Move &operator[](std::size_t index) const noexcept { return moveList_[index]; }

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

    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr std::size_t find(const Move &move) const noexcept {
        for (std::size_t i = 0; i < size_; i++) {
            if (moveList_[i] == move) {
                return i;
            }
        }
        return size_;
    }

private:
    std::array<Move, Constants::MAX_MOVES> moveList_;
    std::size_t size_ = 0;

};

}
