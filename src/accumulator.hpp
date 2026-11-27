#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "ft.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "updates.hpp"

namespace Sift {
    class Accumulator {
    private:
        alignas(SIMD::ALIGNMENT) MultiArray<Int16, 2, FT::OUTPUT_SIZE> data_;

    public:
        Accumulator() noexcept : data_() {}

        inline std::span<const Int16, FT::OUTPUT_SIZE> data(Color color) const noexcept {
            assert(color != Color::NONE);
            return data_[color.index()];
        }

        inline std::span<Int16, FT::OUTPUT_SIZE> data(Color color) noexcept {
            assert(color != Color::NONE);
            return data_[color.index()];
        }

        inline void init(const FT& ft) noexcept {
            std::ranges::copy(ft.biases, data_[0].begin());
            std::ranges::copy(ft.biases, data_[1].begin());
        }

        inline void copy(Color color, const Accumulator& other) { std::ranges::copy(other.data(color), data_[color.index()].begin()); }

        inline void sub1Add1From(const Accumulator& acc, const FT& ft, Color color, USize sub, USize add) noexcept {
            assert(color != Color::NONE);
            assert(sub < FT::PSQ_INPUT_SIZE);
            assert(add < FT::PSQ_INPUT_SIZE);

            std::span<const Int16, FT::OUTPUT_SIZE> src = acc.data(color);
            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize subOffset = sub * FT::OUTPUT_SIZE;
            USize addOffset = add * FT::OUTPUT_SIZE;

            assert(subOffset + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) { dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i]; }
        }

        inline void sub2Add1From(const Accumulator& acc, const FT& ft, Color color, USize sub1, USize sub2, USize add) noexcept {
            assert(color != Color::NONE);
            assert(sub1 < FT::PSQ_INPUT_SIZE);
            assert(sub2 < FT::PSQ_INPUT_SIZE);
            assert(add < FT::PSQ_INPUT_SIZE);

            std::span<const Int16, FT::OUTPUT_SIZE> src = acc.data(color);
            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize subOffset1 = sub1 * FT::OUTPUT_SIZE;
            USize subOffset2 = sub2 * FT::OUTPUT_SIZE;
            USize addOffset = add * FT::OUTPUT_SIZE;

            assert(subOffset1 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(subOffset2 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) {
                dst[i] = src[i] + delta[addOffset + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
            }
        }

        inline void sub2Add2From(const Accumulator& acc, const FT& ft, Color color, USize sub1, USize sub2, USize add1,
            USize add2) noexcept {
            assert(color != Color::NONE);
            assert(sub1 < FT::PSQ_INPUT_SIZE);
            assert(sub2 < FT::PSQ_INPUT_SIZE);
            assert(add1 < FT::PSQ_INPUT_SIZE);
            assert(add2 < FT::PSQ_INPUT_SIZE);

            std::span<const Int16, FT::OUTPUT_SIZE> src = acc.data(color);
            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize subOffset1 = sub1 * FT::OUTPUT_SIZE;
            USize subOffset2 = sub2 * FT::OUTPUT_SIZE;
            USize addOffset1 = add1 * FT::OUTPUT_SIZE;
            USize addOffset2 = add2 * FT::OUTPUT_SIZE;

            assert(subOffset1 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(subOffset2 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset1 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset2 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) {
                dst[i] = src[i] + delta[addOffset1 + i] + delta[addOffset2 + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
            }
        }

        inline void add1(const FT& ft, Color color, USize feature) noexcept {
            assert(color != Color::NONE);
            assert(feature < FT::PSQ_INPUT_SIZE);

            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize addOffset = feature * FT::OUTPUT_SIZE;

            assert(addOffset + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) { dst[i] += delta[addOffset + i]; }
        }

        inline void sub1(const FT& ft, Color color, USize feature) noexcept {
            assert(color != Color::NONE);
            assert(feature < FT::PSQ_INPUT_SIZE);

            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize subOffset = feature * FT::OUTPUT_SIZE;

            assert(subOffset + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) { dst[i] -= delta[subOffset + i]; }
        }

        inline void add4(const FT& ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
            assert(color != Color::NONE);
            assert(feature1 < FT::PSQ_INPUT_SIZE);
            assert(feature2 < FT::PSQ_INPUT_SIZE);
            assert(feature3 < FT::PSQ_INPUT_SIZE);
            assert(feature4 < FT::PSQ_INPUT_SIZE);

            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize addOffset1 = feature1 * FT::OUTPUT_SIZE;
            USize addOffset2 = feature2 * FT::OUTPUT_SIZE;
            USize addOffset3 = feature3 * FT::OUTPUT_SIZE;
            USize addOffset4 = feature4 * FT::OUTPUT_SIZE;

            assert(addOffset1 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset2 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset3 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(addOffset4 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) {
                dst[i] += delta[addOffset1 + i] + delta[addOffset2 + i] + delta[addOffset3 + i] + delta[addOffset4 + i];
            }
        }

        inline void sub4(const FT& ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
            assert(color != Color::NONE);
            assert(feature1 < FT::PSQ_INPUT_SIZE);
            assert(feature2 < FT::PSQ_INPUT_SIZE);
            assert(feature3 < FT::PSQ_INPUT_SIZE);
            assert(feature4 < FT::PSQ_INPUT_SIZE);

            std::span<Int16, FT::OUTPUT_SIZE> dst = data(color);
            std::span<const Int16, FT::PSQ_WEIGHT_SIZE> delta = ft.psqWeights;
            USize subOffset1 = feature1 * FT::OUTPUT_SIZE;
            USize subOffset2 = feature2 * FT::OUTPUT_SIZE;
            USize subOffset3 = feature3 * FT::OUTPUT_SIZE;
            USize subOffset4 = feature4 * FT::OUTPUT_SIZE;

            assert(subOffset1 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(subOffset2 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(subOffset3 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);
            assert(subOffset4 + FT::OUTPUT_SIZE <= FT::PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < FT::OUTPUT_SIZE; i++) {
                dst[i] -= delta[subOffset1 + i] + delta[subOffset2 + i] + delta[subOffset3 + i] + delta[subOffset4 + i];
            }
        }
    };

    struct UpdatableAccumulator {
        Accumulator psqAcc;
        Accumulator threatAcc;
        std::array<bool, 2> psqDirty;
        std::array<bool, 2> threatDirty;
        Updates updates;

        constexpr void setPSQDirty() noexcept { psqDirty[0] = psqDirty[1] = true; }

        constexpr void setPSQClean(Color color) noexcept {
            assert(color != Color::NONE);
            psqDirty[color.index()] = false;
        }

        constexpr bool isPSQDirty(Color color) const noexcept {
            assert(color != Color::NONE);
            return psqDirty[color.index()];
        }

        constexpr void setThreatDirty() noexcept { threatDirty[0] = threatDirty[1] = true; }

        constexpr void setThreatClean(Color color) noexcept {
            assert(color != Color::NONE);
            threatDirty[color.index()] = false;
        }

        constexpr bool isThreatDirty(Color color) const noexcept {
            assert(color != Color::NONE);
            return threatDirty[color.index()];
        }
    };

    struct RefreshTableEntry {
        Accumulator acc = Accumulator();
        MultiArray<Bitboard, 2, 6> pieceBitboards = {};
        MultiArray<Bitboard, 2, 2> occupancyBitboards = {};

        constexpr Bitboard pieces(Color color, Piece piece) const noexcept {
            assert(color != Color::NONE);
            assert(piece != Piece::NONE);
            return pieceBitboards[color.index()][piece.type().index()] & occupancyBitboards[color.index()][piece.color().index()];
        }

        constexpr void updateBitboards(Color color, const Position& position) noexcept {
            assert(color != Color::NONE);
            for (UInt8 pt = 0; pt < 6; pt++) {
                const PieceType pieceType = PieceType(pt);
                pieceBitboards[color.index()][pieceType.index()] = position.pieces(pieceType);
            }

            for (UInt8 c = 0; c < 2; c++) {
                const Color pieceColor = Color(c);
                occupancyBitboards[color.index()][pieceColor.index()] = position.friendly(pieceColor);
            }
        }
    };

    struct RefreshTable {
        std::array<RefreshTableEntry, Arch::REFRESH_TABLE_SIZE> entries = {};

        inline void init(const FT& ft) noexcept {
            for (auto& entry : entries) {
                entry.acc.init(ft);
                for (auto& colorBitboards : entry.pieceBitboards) { colorBitboards.fill(Bitboard()); }
                for (auto& colorOccupancy : entry.occupancyBitboards) { colorOccupancy.fill(Bitboard()); }
            }
        }
    };
}
