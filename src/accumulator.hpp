#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "network.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "updates.hpp"

namespace Sift {
    class Accumulator {
    private:
        alignas(SIMD::ALIGNMENT) MultiArray<Int16, 2, Network::ACC_SIZE> data_;

    public:
        Accumulator() noexcept : data_() {}

        inline std::span<const Int16, Network::ACC_SIZE> data(Color color) const noexcept {
            assert(color != Color::NONE);
            return data_[color.index()];
        }

        inline std::span<Int16, Network::ACC_SIZE> data(Color color) noexcept {
            assert(color != Color::NONE);
            return data_[color.index()];
        }

        inline void init(std::span<const Int16, Network::FT_BIAS_SIZE> biases) noexcept {
            std::ranges::copy(biases, data_[0].begin());
            std::ranges::copy(biases, data_[1].begin());
        }

        inline void copy(Color color, const Accumulator& other) { std::ranges::copy(other.data(color), data_[color.index()].begin()); }

        inline void sub1Add1From(const Accumulator& acc, std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color,
            USize sub, USize add) noexcept {
            assert(color != Color::NONE);
            assert(sub < Network::PSQ_INPUT_SIZE);
            assert(add < Network::PSQ_INPUT_SIZE);

            std::span<const Int16, Network::ACC_SIZE> src = acc.data(color);
            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize subOffset = sub * Network::ACC_SIZE;
            USize addOffset = add * Network::ACC_SIZE;

            assert(subOffset + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) { dst[i] = src[i] + weights[addOffset + i] - weights[subOffset + i]; }
        }

        inline void sub2Add1From(const Accumulator& acc, std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color,
            USize sub1, USize sub2, USize add) noexcept {
            assert(color != Color::NONE);
            assert(sub1 < Network::PSQ_INPUT_SIZE);
            assert(sub2 < Network::PSQ_INPUT_SIZE);
            assert(add < Network::PSQ_INPUT_SIZE);

            std::span<const Int16, Network::ACC_SIZE> src = acc.data(color);
            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize subOffset1 = sub1 * Network::ACC_SIZE;
            USize subOffset2 = sub2 * Network::ACC_SIZE;
            USize addOffset = add * Network::ACC_SIZE;

            assert(subOffset1 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(subOffset2 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) {
                dst[i] = src[i] + weights[addOffset + i] - weights[subOffset1 + i] - weights[subOffset2 + i];
            }
        }

        inline void sub2Add2From(const Accumulator& acc, std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color,
            USize sub1, USize sub2, USize add1, USize add2) noexcept {
            assert(color != Color::NONE);
            assert(sub1 < Network::PSQ_INPUT_SIZE);
            assert(sub2 < Network::PSQ_INPUT_SIZE);
            assert(add1 < Network::PSQ_INPUT_SIZE);
            assert(add2 < Network::PSQ_INPUT_SIZE);

            std::span<const Int16, Network::ACC_SIZE> src = acc.data(color);
            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize subOffset1 = sub1 * Network::ACC_SIZE;
            USize subOffset2 = sub2 * Network::ACC_SIZE;
            USize addOffset1 = add1 * Network::ACC_SIZE;
            USize addOffset2 = add2 * Network::ACC_SIZE;

            assert(subOffset1 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(subOffset2 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset1 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset2 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) {
                dst[i] = src[i] + weights[addOffset1 + i] + weights[addOffset2 + i] - weights[subOffset1 + i] - weights[subOffset2 + i];
            }
        }

        inline void add1(std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color, USize feature) noexcept {
            assert(color != Color::NONE);
            assert(feature < Network::PSQ_INPUT_SIZE);

            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize addOffset = feature * Network::ACC_SIZE;

            assert(addOffset + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) { dst[i] += weights[addOffset + i]; }
        }

        inline void sub1(std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color, USize feature) noexcept {
            assert(color != Color::NONE);
            assert(feature < Network::PSQ_INPUT_SIZE);

            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize subOffset = feature * Network::ACC_SIZE;

            assert(subOffset + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) { dst[i] -= weights[subOffset + i]; }
        }

        inline void add4(std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color, USize feature1, USize feature2,
            USize feature3, USize feature4) noexcept {
            assert(color != Color::NONE);
            assert(feature1 < Network::PSQ_INPUT_SIZE);
            assert(feature2 < Network::PSQ_INPUT_SIZE);
            assert(feature3 < Network::PSQ_INPUT_SIZE);
            assert(feature4 < Network::PSQ_INPUT_SIZE);

            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize addOffset1 = feature1 * Network::ACC_SIZE;
            USize addOffset2 = feature2 * Network::ACC_SIZE;
            USize addOffset3 = feature3 * Network::ACC_SIZE;
            USize addOffset4 = feature4 * Network::ACC_SIZE;

            assert(addOffset1 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset2 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset3 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(addOffset4 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) {
                dst[i] += weights[addOffset1 + i] + weights[addOffset2 + i] + weights[addOffset3 + i] + weights[addOffset4 + i];
            }
        }

        inline void sub4(std::span<const Int16, Network::FT_PSQ_WEIGHT_SIZE> weights, Color color, USize feature1, USize feature2,
            USize feature3, USize feature4) noexcept {
            assert(color != Color::NONE);
            assert(feature1 < Network::PSQ_INPUT_SIZE);
            assert(feature2 < Network::PSQ_INPUT_SIZE);
            assert(feature3 < Network::PSQ_INPUT_SIZE);
            assert(feature4 < Network::PSQ_INPUT_SIZE);

            std::span<Int16, Network::ACC_SIZE> dst = data(color);
            USize subOffset1 = feature1 * Network::ACC_SIZE;
            USize subOffset2 = feature2 * Network::ACC_SIZE;
            USize subOffset3 = feature3 * Network::ACC_SIZE;
            USize subOffset4 = feature4 * Network::ACC_SIZE;

            assert(subOffset1 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(subOffset2 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(subOffset3 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);
            assert(subOffset4 + Network::ACC_SIZE <= Network::FT_PSQ_WEIGHT_SIZE);

            for (USize i = 0; i < Network::ACC_SIZE; i++) {
                dst[i] -= weights[subOffset1 + i] + weights[subOffset2 + i] + weights[subOffset3 + i] + weights[subOffset4 + i];
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

        inline void init(const Network& network) noexcept {
            for (auto& entry : entries) {
                entry.acc.init(network.ftBiases);
                for (auto& colorBitboards : entry.pieceBitboards) { colorBitboards.fill(Bitboard()); }
                for (auto& colorOccupancy : entry.occupancyBitboards) { colorOccupancy.fill(Bitboard()); }
            }
        }
    };
}
