#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <string_view>

#include "coords.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Sift {

enum class MoveType : UInt16 {
    NORMAL = 0,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
};

class Move {
public:
    constexpr static UInt16 NULL_MOVE = 0;

    constexpr Move() noexcept : move_(NULL_MOVE) {}
    constexpr Move(UInt16 move) noexcept : move_(move) {}

    constexpr Move(Square from, Square to, MoveType type = MoveType::NORMAL, PieceType promotion = PieceType::NONE) noexcept : move_(NULL_MOVE) {
        assert(from != Square::NONE && to != Square::NONE);

        const UInt16 typeBits = static_cast<UInt16>(type);
        UInt16 promotionBits = 0;
        if (type == MoveType::PROMOTION) {
            assert(promotion >= PieceType(PieceType::KNIGHT) && promotion <= PieceType(PieceType::QUEEN));
            promotionBits = static_cast<UInt16>((promotion - PieceType(PieceType::KNIGHT)) << 12);
        } else {
            assert(promotion == PieceType::NONE);
        }

        const UInt16 fromBits = static_cast<UInt16>(from.index() << 6);
        const UInt16 toBits = static_cast<UInt16>(to.index());
        move_ = static_cast<UInt16>(typeBits | promotionBits | fromBits | toBits);
    }

    constexpr bool operator==(const Move &other) const noexcept { return move_ == other.move_; }
    constexpr bool operator!=(const Move &other) const noexcept { return move_ != other.move_; }

    constexpr explicit operator std::string() const {
        if (move_ == NULL_MOVE) {
            return "(none)";
        }

        std::string fromString = std::string(from());
        Square toSquare = to();
        if (type() == MoveType::CASTLING) {
            if (toSquare.file() == File::A) {
                toSquare = Square(File::C, toSquare.rank());
            } else if (toSquare.file() == File::H) {
                toSquare = Square(File::G, toSquare.rank());
            }
        }
        std::string toString = std::string(toSquare);
        std::string promotionString;
        if (type() == MoveType::PROMOTION) {
            promotionString = std::string(promotion());
        }
        return fromString + toString + promotionString;
    }

    constexpr Square from() const noexcept { return Square((move_ >> 6) & 0x3F); }
    constexpr Square to() const noexcept { return Square(move_ & 0x3F); }

    constexpr MoveType type() const noexcept { return static_cast<MoveType>(move_ & (3 << 14)); }

    constexpr PieceType promotion() const noexcept {
        if (type() != MoveType::PROMOTION) {
            return PieceType::NONE;
        }
        return PieceType(((move_ >> 12) & 3) + PieceType(PieceType::KNIGHT));
    }

    constexpr UInt16 fromTo() const noexcept { return move_ & 0xFFF; }

    constexpr UInt16 internal() const noexcept { return move_; }

private:
    UInt16 move_;
};

class MoveList {
public:
    constexpr static USize MAX_MOVES = 256;

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
        assert(size_ < MAX_MOVES);
        moveList_[size_++] = move;
    }

    constexpr void add(Move &&move) noexcept {
        assert(size_ < MAX_MOVES);
        moveList_[size_++] = std::move(move);
    }

    constexpr void clear() noexcept { size_ = 0; }

    constexpr USize size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    template<typename Function>
    void sort(Function compare) noexcept {
        std::stable_sort(moveList_.begin(), moveList_.begin() + size_, compare);
    }

    constexpr USize find(Move move) const noexcept {
        auto it = std::find(moveList_.begin(), moveList_.begin() + size_, move);
        return static_cast<USize>(it - moveList_.begin());
    }

    template<typename Function>
    constexpr USize findIf(Function compare) const noexcept {
        auto it = std::find_if(moveList_.begin(), moveList_.begin() + size_, compare);
        if (it != moveList_.begin() + size_) {
            return static_cast<USize>(it - moveList_.begin());
        }
        return size_;
    }

private:
    std::array<Move, MAX_MOVES> moveList_;
    USize size_ = 0;

};

}
