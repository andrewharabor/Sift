#pragma once

#include <bit>
#include <cstdint>

#include "coordinates.hpp"
#include "types.hpp"


namespace Clownfish {

class Bitboard {
public:
    constexpr Bitboard() noexcept : bitboard_(0) {}
    constexpr Bitboard(U64 bits) noexcept : bitboard_(bits) {}

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

    constexpr bool operator==(U64 bits) const noexcept { return bitboard_ == bits; }
    constexpr bool operator!=(U64 bits) const noexcept { return bitboard_ != bits; }
    constexpr bool operator&&(U64 bits) const noexcept { return bitboard_ && bits; }
    constexpr bool operator||(U64 bits) const noexcept { return bitboard_ || bits; }

    constexpr explicit operator bool() const noexcept { return bitboard_ != 0; }

    constexpr Bitboard operator&(U64 bits) const noexcept { return Bitboard(bitboard_ & bits); }
    constexpr Bitboard operator|(U64 bits) const noexcept { return Bitboard(bitboard_ | bits); }
    constexpr Bitboard operator^(U64 bits) const noexcept { return Bitboard(bitboard_ ^ bits); }
    constexpr Bitboard operator<<(U64 shift) const noexcept { return Bitboard(bitboard_ << shift); }
    constexpr Bitboard operator>>(U64 shift) const noexcept { return Bitboard(bitboard_ >> shift); }

    constexpr Bitboard &operator&=(U64 bits) noexcept {
        bitboard_ &= bits;
        return *this;
    }

    constexpr Bitboard &operator|=(U64 bits) noexcept {
        bitboard_ |= bits;
        return *this;
    }

    constexpr Bitboard &operator^=(U64 bits) noexcept {
        bitboard_ ^= bits;
        return *this;
    }

    constexpr Bitboard &set(int index) noexcept {
        assert(index >= 0 && index < 64);
        bitboard_ |= (1ULL << index);
        return *this;
    }

    constexpr bool get(int index) const noexcept {
        assert(index >= 0 && index < 64);
        return bitboard_ & (1ULL << index);
    }

    constexpr Bitboard &toggle(int index) noexcept {
        assert(index >= 0 && index < 64);
        bitboard_ ^= (1ULL << index);
        return *this;
    }

    constexpr Bitboard &clear(int index) noexcept {
        assert(index >= 0 && index < 64);
        bitboard_ &= ~(1ULL << index);
        return *this;
    }

    constexpr Bitboard &clear() noexcept {
        bitboard_ = 0;
        return *this;
    }

    constexpr bool empty() const noexcept { return bitboard_ == 0; }

    constexpr int lsb() const noexcept {
        assert(bitboard_ != 0);
        return std::countr_zero(bitboard_);
    }

    constexpr int msb() const noexcept {
        assert(bitboard_ != 0);
        return 63 - std::countl_zero(bitboard_);
    }

    constexpr int count() const noexcept {
        return std::popcount(bitboard_);
    }

    constexpr U64 pop() noexcept {
        assert(bitboard_ != 0);
        const U64 index = static_cast<U64>(lsb());
        bitboard_ &= bitboard_ - 1;
        return index;
    }

    constexpr U64 bits() const noexcept { return bitboard_; }

private:
    U64 bitboard_;
};

}
