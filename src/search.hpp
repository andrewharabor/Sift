#pragma once

#include <array>
#include <cassert>
#include <utility>

#include "move.hpp"
#include "move-generator.hpp"
#include "position.hpp"
#include "score.hpp"


namespace Clownfish {

class Search {
public:
    Search() noexcept {
        reset();
    }

    void run(const Position &position) noexcept {
        reset();
        position_ = position;
        MoveGenerator::legal(position_, rootMoves_);
        iterativeDeepening();
    }

    constexpr void reset() noexcept {
        position_.reset();
        rootDepth_ = 0;
        rootPly_ = 0;
        selDepth_ = 0;
        nodes_ = 0;
        rootMoves_.clear();
        for (USize i = 0; i <= MAX_PLY; i++) {
            stack_[i].pv.clear();
            stack_[i].currentMove = Move::NULL_MOVE;
            stack_[i].excludedMove = Move::NULL_MOVE;
            stack_[i].staticEval = ScoreLimits::NONE;
            stack_[i].eval = ScoreLimits::NONE;
        }
    }

private:
    static constexpr USize MAX_PLY = static_cast<USize>(ScoreLimits::MAX_PLY);

    struct Node {
        MoveList pv;

        Move currentMove;
        // Move excludedMove;

        Score staticEval;
        Score eval;
    };

    struct RootMove {
        Move move = Move::NULL_MOVE;
        MoveList pv;

        Score score = ScoreLimits::NONE;
        Score prevScore = ScoreLimits::NONE;
        bool lowerBound = false;
        bool upperBound = false;

        U64 nodes = 0;

        constexpr RootMove(Move move) noexcept : move(move) {}
    };

    struct Info {
        // TODO
    };

    struct Limits {
        I32 depth;
        I32 nodes;
        // TODO: time
    };

    Position position_;

    I32 rootDepth_;
    I32 rootPly_;
    I32 selDepth_;
    U64 nodes_;

    MoveList rootMoves_;
    std::array<Node, MAX_PLY + 1> stack_;

    Limits limits_;

    std::pair<Move, Score> iterativeDeepening() noexcept {
        // TODO
        return {Move::NULL_MOVE, ScoreLimits::NONE};
    }

    Score search(USize ply, I32 depth, Score alpha, Score beta, bool pvNode, bool cutNode) noexcept {
        assert(!(pvNode && cutNode));

        if (rootPly_ + 1 > selDepth_) {
            selDepth_ = rootPly_ + 1;
        }

        depth = std::min(depth, static_cast<I32>(MAX_PLY - 1));

        alpha = std::max(alpha, ScoreLimits::MATED + rootPly_);
        beta = std::min(beta, ScoreLimits::MATE - rootPly_);
        if (alpha >= beta) {
            return alpha;
        }

        bool rootNode = (rootPly_ == 0);
        bool check = position_.check();

        return ScoreLimits::NONE;
    }

    void makeMove(USize ply, Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        position_.make(move);
        rootPly_++;
        nodes_++;
        stack_[ply].currentMove = move;
    }

    void unmakeMove(USize ply) noexcept {
        position_.unmake(stack_[ply].currentMove);
        rootPly_--;
        stack_[ply].currentMove = Move::NULL_MOVE;
    }

    void makeNullMove(USize ply) noexcept {
        position_.makeNull();
        rootPly_++;
    }

    void unmakeNullMove(USize ply) noexcept {
        position_.unmakeNull();
        rootPly_--;
    }
};

}
