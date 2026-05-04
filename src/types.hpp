#pragma once

#include <array>
#include <cstddef>
#include <cstdint>


namespace Syft {

using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

using Int8 = std::int8_t;
using Int16 = std::int16_t;
using Int32 = std::int32_t;
using Int64 = std::int64_t;

using USize = std::size_t;

using Float32 = float;
using Float64 = double;

namespace Internal {

template<typename Type, USize SIZE, USize... SIZES>
struct MultiArrayImpl {
    using Type = std::array<typename MultiArrayImpl<Type, SIZES...>::Type, SIZE>;
};

template<typename Type, USize SIZE>
struct MultiArrayImpl<Type, SIZE> {
    using Type = std::array<Type, SIZE>;
};

}

template<typename Type, USize... SIZES>
using MultiArray = typename Internal::MultiArrayImpl<Type, SIZES...>::Type;

}
