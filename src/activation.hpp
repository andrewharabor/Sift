#pragma once

#include "simd.hpp"
#include "types.hpp"


namespace Sift {

struct ReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        const Vec<Int16> clamped = SIMD::max<Int16>(inputs, zero);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct CReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct SCReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        const Vec<Int16> crelu = SIMD::mulLo<Int16>(clamped, weights);
        return SIMD::mulAddAdjAcc<Int16>(sum, crelu, clamped);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value / MAX; }
};

}
