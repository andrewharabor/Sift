#pragma once

#include <array>
#include <cassert>
#include <utility>

#include "move-gen.hpp"
#include "move.hpp"
#include "position.hpp"
#include "score.hpp"
#include "ttable.hpp"
#include "types.hpp"


namespace Clownfish {

class Search {
public:
    Search(USize tableSizeMB) noexcept : tTable_(tableSizeMB) {
        reset();
    }

    void run(const Position &position) noexcept {
        reset();
        tTable_.incrementAge();

        position_ = position;
        MoveGen::legal(position_, rootMoves_);
        iterativeDeepening();
    }

    constexpr void newGame() noexcept {
        reset();
        tTable_.reset();
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
            // stack_[i].excludedMove = Move::NULL_MOVE;
            stack_[i].staticEval = Score::NONE;
            stack_[i].eval = Score::NONE;
        }
    }

    constexpr void resizeTTable(USize sizeMB) noexcept {
        tTable_.resize(sizeMB);
    }

private:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    struct Node {
        MoveList pv;

        Move currentMove;
        // Move excludedMove;

        I32 staticEval;
        I32 eval;
    };

    struct RootMove {
        Move move = Move::NULL_MOVE;
        MoveList pv;

        I32 score = Score::NONE;
        I32 prevScore = Score::NONE;
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

    TTable tTable_;

    Limits limits_;

    std::pair<Move, I32> iterativeDeepening() noexcept {
        // TODO
        return {Move::NULL_MOVE, Score::NONE};
    }

    I32 search(USize ply, I32 depth, I32 alpha, I32 beta, bool pvNode, bool cutNode) noexcept {
        assert(!(pvNode && cutNode));

        if (rootPly_ + 1 > selDepth_) {
            selDepth_ = rootPly_ + 1;
        }

        depth = std::min(depth, static_cast<I32>(MAX_PLY - 1));

        alpha = std::max(alpha, Score::MATED + rootPly_);
        beta = std::min(beta, Score::MATE - rootPly_);
        if (alpha >= beta) {
            return alpha;
        }

        bool rootNode = (rootPly_ == 0);
        bool check = position_.check();

        if (!rootNode && alpha < Score::DRAW && position_.upcomingRepetition(rootPly_)) {
            alpha = Score::DRAW;
            if (alpha >= beta) {
                return alpha;
            }
        }

        MoveList moves;
        MoveGen::legal(position_, moves);

        if (position_.draw(moves, rootPly_)) {
            return Score::DRAW;
        }

        if (depth <= 0) {
            // TODO: quiescence search
            return Score::NONE;
        }


        // TODO

        return Score::NONE;
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
