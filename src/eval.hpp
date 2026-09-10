#pragma once

#include <algorithm>

#include "nnue.hpp"
#include "position.hpp"
#include "score.hpp"
#include "tunable.hpp"
#include "types.hpp"

namespace Sift {

class Eval {
public:
    static Int32 raw(const Position &position, NNUE &nnue, const std::array<Int32, 2> &contempt) noexcept {
        const Int32 score = nnue.forward(position) + contempt[position.sideToMove().index()];
        return std::clamp(score, Score::LOSS + 1, Score::WIN - 1);
    }

    static Int32 adjust(Int32 score, const Position &position, const std::array<Int32, 2> &optimism, Int32 correction = 0) noexcept {
        const Int32 material = EVAL_ADJUST_PAWN_SCALE * position.pieces(PieceType::PAWN).count() + EVAL_ADJUST_KNIGHT_SCALE * position.pieces(PieceType::KNIGHT).count() + EVAL_ADJUST_BISHOP_SCALE * position.pieces(PieceType::BISHOP).count() + EVAL_ADJUST_ROOK_SCALE * position.pieces(PieceType::ROOK).count() + EVAL_ADJUST_QUEEN_SCALE * position.pieces(PieceType::QUEEN).count();
        const Int32 materialScale = EVAL_ADJUST_MATERIAL_BASE + material;
        const Int32 optimismBonus = optimism[position.sideToMove().index()] * (EVAL_ADJUST_OPTIMISM_SCALE_BASE + EVAL_ADJUST_OPTIMISM_SCALE_MATERIAL_SCALE * material / 1024);
        score = (score * materialScale + optimismBonus) / 32768;
        score = score * (EVAL_ADJUST_HALF_MOVE_SCALE - static_cast<Int32>(position.halfmoveClock())) / EVAL_ADJUST_HALF_MOVE_SCALE;
        score += correction;
        score = std::clamp(score, Score::LOSS + 1, Score::WIN - 1);
        return score;
    }

    static Int32 adjusted(const Position &position, NNUE &nnue, const std::array<Int32, 2> &optimism, const std::array<Int32, 2> &contempt, Int32 correction = 0) noexcept { return adjust(raw(position, nnue, contempt), position, optimism, correction); }
};

}
