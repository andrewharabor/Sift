#pragma once

#include <span>

#include "arch.hpp"
#include "ft.hpp"
#include "multilayer.hpp"
#include "position.hpp"
#include "reader.hpp"
#include "types.hpp"

namespace Sift {
    class Network {
    public:
        constexpr const FT& ft() const noexcept { return ft_; }

        inline Int32 forward(const Position& position, std::span<const Int16, FT::OUTPUT_SIZE> friendlyPSQInputs,
            std::span<const Int16, FT::OUTPUT_SIZE> enemyPSQInputs, std::span<const Int16, FT::OUTPUT_SIZE> friendlyThreatInputs,
            std::span<const Int16, FT::OUTPUT_SIZE> enemyThreatInputs) const noexcept {
            return multilayer_.forward(Arch::outputBucket(position), friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs,
                enemyThreatInputs);
        }

        inline bool load(ByteReader& reader) noexcept { return ft_.load(reader) && multilayer_.load(reader); }

        static constexpr USize byteSize() noexcept { return FT::byteSize() + Multilayer::byteSize(); }

    private:
        FT ft_;
        Multilayer multilayer_;
    };
}
