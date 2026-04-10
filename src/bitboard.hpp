#pragma once

#include <bit>
#include <cstdint>

#include "coords.hpp"
#include "types.hpp"


namespace Clownfish {

class Bitboard {
public:
    constexpr Bitboard() noexcept : bitboard_(0) {}
    constexpr Bitboard(UInt64 bits) noexcept : bitboard_(bits) {}

    constexpr Bitboard(File file) noexcept : bitboard_(0) {
        assert(file != File::NONE);
        bitboard_ = 0x0101010101010101ULL << file;
    }

    constexpr Bitboard(Rank rank) noexcept : bitboard_(0) {
        assert(rank != Rank::NONE);
        bitboard_ = 0xFFULL << (rank * 8);
    }

    constexpr Bitboard(Square square) noexcept : bitboard_(0) {
        assert(square != Square::NONE);
        bitboard_ = 1ULL << square.index();
    }

    constexpr bool operator==(const Bitboard &other) const noexcept { return bitboard_ == other.bitboard_; }
    constexpr bool operator!=(const Bitboard &other) const noexcept { return bitboard_ != other.bitboard_; }
    constexpr bool operator&&(const Bitboard &other) const noexcept { return bitboard_ && other.bitboard_; }
    constexpr bool operator||(const Bitboard &other) const noexcept { return bitboard_ || other.bitboard_; }

    constexpr Bitboard operator&(const Bitboard &other) const noexcept { return Bitboard(bitboard_ & other.bitboard_); }
    constexpr Bitboard operator|(const Bitboard &other) const noexcept { return Bitboard(bitboard_ | other.bitboard_); }
    constexpr Bitboard operator^(const Bitboard &other) const noexcept { return Bitboard(bitboard_ ^ other.bitboard_); }
    constexpr Bitboard operator~() const noexcept { return Bitboard(~bitboard_); }

    constexpr Bitboard &operator&=(const Bitboard &other) noexcept {
        bitboard_ &= other.bitboard_;
        return *this;
    }

    constexpr Bitboard &operator|=(const Bitboard &other) noexcept {
        bitboard_ |= other.bitboard_;
        return *this;
    }

    constexpr Bitboard &operator^=(const Bitboard &other) noexcept {
        bitboard_ ^= other.bitboard_;
        return *this;
    }

    constexpr bool operator==(UInt64 bits) const noexcept { return bitboard_ == bits; }
    constexpr bool operator!=(UInt64 bits) const noexcept { return bitboard_ != bits; }
    constexpr bool operator&&(UInt64 bits) const noexcept { return bitboard_ && bits; }
    constexpr bool operator||(UInt64 bits) const noexcept { return bitboard_ || bits; }

    constexpr explicit operator bool() const noexcept { return bitboard_ != 0; }

    constexpr Bitboard operator&(UInt64 bits) const noexcept { return Bitboard(bitboard_ & bits); }
    constexpr Bitboard operator|(UInt64 bits) const noexcept { return Bitboard(bitboard_ | bits); }
    constexpr Bitboard operator^(UInt64 bits) const noexcept { return Bitboard(bitboard_ ^ bits); }
    constexpr Bitboard operator<<(UInt64 shift) const noexcept { return Bitboard(bitboard_ << shift); }
    constexpr Bitboard operator>>(UInt64 shift) const noexcept { return Bitboard(bitboard_ >> shift); }

    constexpr Bitboard &operator&=(UInt64 bits) noexcept {
        bitboard_ &= bits;
        return *this;
    }

    constexpr Bitboard &operator|=(UInt64 bits) noexcept {
        bitboard_ |= bits;
        return *this;
    }

    constexpr Bitboard &operator^=(UInt64 bits) noexcept {
        bitboard_ ^= bits;
        return *this;
    }

    constexpr Bitboard &set(UInt8 index) noexcept {
        assert(index < 64);
        bitboard_ |= (1ULL << index);
        return *this;
    }

    constexpr bool get(UInt8 index) const noexcept {
        assert(index < 64);
        return bitboard_ & (1ULL << index);
    }

    constexpr Bitboard &toggle(UInt8 index) noexcept {
        assert(index < 64);
        bitboard_ ^= (1ULL << index);
        return *this;
    }

    constexpr Bitboard &clear(UInt8 index) noexcept {
        assert(index < 64);
        bitboard_ &= ~(1ULL << index);
        return *this;
    }

    constexpr Bitboard &clear() noexcept {
        bitboard_ = 0;
        return *this;
    }

    constexpr bool empty() const noexcept { return bitboard_ == 0; }

    constexpr UInt8 lsb() const noexcept {
        assert(bitboard_ != 0);
        return static_cast<UInt8>(std::countr_zero(bitboard_));
    }

    constexpr UInt8 msb() const noexcept {
        assert(bitboard_ != 0);
        return static_cast<UInt8>(63 - std::countl_zero(bitboard_));
    }

    constexpr UInt8 count() const noexcept {
        return static_cast<UInt8>(std::popcount(bitboard_));
    }

    constexpr UInt8 pop() noexcept {
        assert(bitboard_ != 0);
        const UInt8 index = lsb();
        bitboard_ &= bitboard_ - 1;
        return index;
    }

    constexpr UInt64 bits() const noexcept { return bitboard_; }

private:
    UInt64 bitboard_;
};

}
