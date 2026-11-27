#pragma once

#include <array>
#include <cassert>

#include "bitboard.hpp"
#include "color.hpp"
#include "features.hpp"
#include "types.hpp"

namespace Sift {
    struct Updates {
        std::array<bool, 2> psqRefresh = {};

        std::array<PSQFeature, 2> psqAdds = {};
        USize psqAddSize = 0;

        std::array<PSQFeature, 2> psqSubs = {};
        USize psqSubSize = 0;

        std::array<bool, 2> tiRefresh = {};

        std::array<TIFeature, 128> tiAdds = {};
        USize tiAddSize = 0;

        std::array<TIFeature, 128> tiSubs = {};
        USize tiSubSize = 0;

        std::array<Bitboard, 2> pawnsBefore = {};
        std::array<Bitboard, 2> pawnsAfter = {};

        constexpr void setPSQRefresh(Color color) noexcept { psqRefresh[color.index()] = true; }

        constexpr bool needsPSQRefresh(Color color) const noexcept { return psqRefresh[color.index()]; }

        constexpr void addPSQFeature(PSQFeature feature) noexcept {
            assert(psqAddSize < 2);
            psqAdds[psqAddSize++] = feature;
        }

        constexpr void subPSQFeature(PSQFeature feature) noexcept {
            assert(psqSubSize < 2);
            psqSubs[psqSubSize++] = feature;
        }

        constexpr void setTIRefresh(Color color) noexcept { tiRefresh[color.index()] = true; };

        constexpr bool needsTIRefresh(Color color) const noexcept { return tiRefresh[color.index()]; }

        constexpr void addTIFeature(TIFeature feature) noexcept {
            assert(tiAddSize < 128);
            tiAdds[tiAddSize++] = feature;
        }

        constexpr void subTIFeature(TIFeature feature) noexcept {
            assert(tiSubSize < 128);
            tiSubs[tiSubSize++] = feature;
        }

        template<typename Function>
        inline void writeAddTIFeatures(Function func) noexcept {
            assert(tiAddSize + 16 <= 128);
            tiAddSize += func(&tiAdds[tiAddSize]);
        }

        template<typename Function>
        inline void writeSubTIFeatures(Function func) noexcept {
            assert(tiSubSize + 16 <= 128);
            tiSubSize += func(&tiSubs[tiSubSize]);
        }

        constexpr void setPawns(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
            pawnsBefore[0] = whiteBefore;
            pawnsBefore[1] = blackBefore;
            pawnsAfter[0] = whiteAfter;
            pawnsAfter[1] = blackAfter;
        }
    };
}
