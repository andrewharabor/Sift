#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <utility>

#include "coords.hpp"
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

    constexpr Move() noexcept : move_(0) {}
    constexpr Move(std::uint16_t move) noexcept : move_(move) {}

    constexpr bool operator==(const Move &other) const noexcept { return move_ == other.move_; }
    constexpr bool operator!=(const Move &other) const noexcept { return move_ != other.move_; }

    template<MoveType MT>
    static constexpr Move create(Square from, Square to, PieceType promotion = PieceType::NONE) noexcept {
        const std::uint16_t typeBits = static_cast<std::uint16_t>(MT);
        std::uint16_t promotionBits = 0;
        if constexpr (MT == MoveType::PROMOTION) {
            assert(promotion >= PieceType(PieceType::KNIGHT) && promotion <= PieceType(PieceType::QUEEN));
            promotionBits = static_cast<std::uint16_t>((static_cast<int>(promotion.internal()) - static_cast<int>(PieceType::KNIGHT)) << 12);
        }
        const std::uint16_t fromBits = static_cast<std::uint16_t>(from.index() << 6);
        const std::uint16_t toBits = static_cast<std::uint16_t>(to.index());
        return Move(typeBits | promotionBits | fromBits | toBits);
    }

    constexpr Square from() const noexcept { return Square((move_ >> 6) & 0x3F); }
    constexpr Square to() const noexcept { return Square(move_ & 0x3F); }

    constexpr MoveType type() const noexcept { return static_cast<MoveType>(move_ & (3 << 14)); }

    constexpr PieceType promotion() const noexcept {
        if (type() != MoveType::PROMOTION) {
            return PieceType::NONE;
        }
        return PieceType(((move_ >> 12) & 3) + static_cast<int>(PieceType::KNIGHT));
    }

    constexpr std::uint16_t move() const noexcept { return move_; }

private:
    std::uint16_t move_;
};

class MoveList {
public:
    constexpr static std::size_t MAX_MOVES = 256;

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
        assert(size_ < MAX_MOVES);
        moveList_[size_++] = move;
    }

    constexpr void add(Move &&move) noexcept {
        assert(size_ < MAX_MOVES);
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
    std::array<Move, MAX_MOVES> moveList_;
    std::size_t size_ = 0;

};

}
