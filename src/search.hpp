#pragma once

#include <array>
#include <cassert>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "eval.hpp"
#include "move-gen.hpp"
#include "move-order.hpp"
#include "move.hpp"
#include "position.hpp"
#include "score.hpp"
#include "time.hpp"
#include "ttable.hpp"
#include "types.hpp"


namespace Clownfish {

struct SearchNode {
    MoveList pv;

    Move playedMove;
    Piece movedPiece;
    Move excludedMove;

    std::array<Move, 2> killerMoves;

    Int32 staticEval;
    Int32 eval;

    UInt32 failHighCount;
};

struct SearchInfo {
    Int32 depth;
    Int32 selDepth;
    MS time;
    UInt64 nodes;
    USize hashfull;
    Int32 score;
    bool lowerBound;
    bool upperBound;
    MoveList pv;
};

class Search {
public:
    using Callback = std::function<void(const SearchInfo &)>;

    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    Search(USize tableSizeMB, Callback uciPrintSearchInfo) : tTable_(tableSizeMB), timeManager_(), uciPrintSearchInfo_(std::move(uciPrintSearchInfo)) { reset(); }

    std::pair<Move, Int32> run(const Position &position, const SearchLimits &limits) noexcept {
        reset();
        tTable_.incrementAge();

        position_ = position;

        initRootMoves();
        if (rootMoves_.empty()) {
            return {Move::NULL_MOVE, Score::NONE};
        }

        timeUp_ = false;
        limits_ = limits;
        timeManager_.limits(limits, position_.sideToMove(), rootMoves_.size());
        timeManager_.start();

        return iterativeDeepening();
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
            searchStack_[i].pv.clear();
            searchStack_[i].playedMove = Move::NULL_MOVE;
            searchStack_[i].movedPiece = Piece::NONE;
            searchStack_[i].excludedMove = Move::NULL_MOVE;
            searchStack_[i].killerMoves[0] = searchStack_[i].killerMoves[1] = Move::NULL_MOVE;
            searchStack_[i].staticEval = Score::NONE;
            searchStack_[i].eval = Score::NONE;
            searchStack_[i].failHighCount = 0;
        }
        timeUp_ = false;
        limits_ = SearchLimits();
    }

    constexpr void resizeTTable(USize sizeMB) noexcept { tTable_.resize(sizeMB); }

private:
    struct RootMove {
        Move move = Move::NULL_MOVE;
        MoveList pv;

        Int32 score = Score::NONE;
        Int32 displayScore = Score::NONE;
        Int32 prevScore = Score::NONE;
        bool lowerBound = false;
        bool upperBound = false;

        Int32 selDepth = 0;
        UInt64 nodes = 0;

        constexpr RootMove(Move move) noexcept : move(move) {}
    };

    static constexpr Int32 WINDOW_INIT_DELTA = 10;
    static constexpr Int32 MIN_WINDOW_DEPTH = 6;
    static constexpr Int32 WINDOW_WIDENING_FACTOR = 58;
    static constexpr Int32 WINDOW_WIDENING_SCALE = 256;

    Position position_;

    Int32 rootDepth_;
    Int32 rootPly_;
    Int32 selDepth_;
    UInt64 nodes_;

    std::vector<RootMove> rootMoves_;
    std::array<SearchNode, Search::MAX_PLY + 1> searchStack_;

    TTable tTable_;

    TimeManager timeManager_;
    SearchLimits limits_;
    bool timeUp_;

    Callback uciPrintSearchInfo_;

    std::pair<Move, Int32> iterativeDeepening() noexcept {
        const Int32 maxDepth = std::min(limits_.depth, static_cast<Int32>(MAX_PLY - 1));
        Int32 score = 0;

        for (Int32 depth = 1; depth <= maxDepth; depth++) {
            rootDepth_ = depth;
            selDepth_ = 0;

            Int32 alpha = Score::MIN;
            Int32 beta = Score::MAX;
            Int32 delta = WINDOW_INIT_DELTA;
            Int32 searchDepth = depth;

            if (depth >= MIN_WINDOW_DEPTH) {
                alpha = std::max(score - delta, Score::MIN);
                beta = std::min(score + delta, Score::MAX);
            }

            Int32 searchScore = 0;

            while (true) {
                searchScore = search(0, searchDepth, alpha, beta, true, false);
                sortRootMoves();

                printSearchInfo(depth);

                if (timeUp_) {
                    break;
                }

                if (searchScore <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, Score::MIN);
                } else if (searchScore >= beta) {
                    beta = std::min(beta + delta, Score::MAX);
                } else {
                    break;
                }

                delta += (delta * WINDOW_WIDENING_FACTOR) / WINDOW_WIDENING_SCALE;
            }

            if (timeUp_) {
                break;
            }

            score = searchScore;

            if (timeManager_.stopSoft(limits_, rootMoves_[0].move, rootMoves_[0].nodes, nodes_)) {
                timeUp_ = true;
                break;
            }
        }

        return {rootMoves_[0].move, score};
    }

    Int32 search(USize ply, Int32 depth, Int32 alpha, Int32 beta, bool pvNode, bool cutNode) noexcept {
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);
        assert(!(pvNode && cutNode));

        if (timeUp_ || timeManager_.stopHard(limits_, nodes_)) {
            timeUp_ = true;
            return alpha;
        }

        if (rootPly_ + 1 > selDepth_) {
            selDepth_ = rootPly_ + 1;
        }

        depth = std::min(depth, static_cast<Int32>(MAX_PLY - 1));

        alpha = std::max(alpha, Score::matedIn(rootPly_));
        beta = std::min(beta, Score::mateIn(rootPly_));
        if (alpha >= beta) {
            return alpha;
        }

        const bool rootNode = (rootPly_ == 0);
        const bool inCheck = position_.inCheck();
        const bool excludedMove = searchStack_[ply].excludedMove != Move::NULL_MOVE;

        searchStack_[ply].pv.clear();

        if (!rootNode && position_.halfmoveClock() >= 3 && alpha < 0 && position_.upcomingRepetition(rootPly_)) {
            alpha = Score::DRAW;
            if (alpha >= beta) {
                return alpha;
            }
        }

        if (positionDraw()) {
            return Score::DRAW;
        }

        if (static_cast<USize>(rootPly_) >= MAX_PLY) {
            return SimPLYChessEval::evaluate(position_); // FIXME use Eval
        }

        if (depth <= 0) {
            return quiescenceSearch(ply, alpha, beta, pvNode);
        }

        TTableEntry tableEntry = TTableEntry();
        bool tableHit = false;

        Int32 staticEval = Score::NONE;

        // FIXME: correplexity?

        if (!excludedMove) {
            std::tie(tableEntry, tableHit) = tTable_.probe(position_.hash(), rootPly_);

            if (tableHit && !pvNode && tableEntry.depth >= depth && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= beta) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= alpha))) {
                return tableEntry.score;
            }

            if (inCheck) {
                searchStack_[ply].staticEval = Score::NONE;
                searchStack_[ply].eval = Score::NONE;
            } else {
                if (tableHit) {
                    staticEval = tableEntry.staticEval;
                } else {
                    staticEval = SimPLYChessEval::evaluate(position_); // FIXME: use Eval
                }

                // FIXME: history
                searchStack_[ply].staticEval = staticEval;
                searchStack_[ply].eval = searchStack_[ply].staticEval;
                if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= searchStack_[ply].eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= searchStack_[ply].eval))) {
                    searchStack_[ply].eval = tableEntry.score;
                }
            }
        }

        const bool tablePV = pvNode || (tableHit && tableEntry.pv);

        searchStack_[ply + 1].killerMoves[0] = searchStack_[ply + 1].killerMoves[1] = Move::NULL_MOVE;

        // TODO: more pruning

        searchStack_[ply + 1].failHighCount = 0;

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        MoveList quietsTried;
        MoveList noisiesTried;
        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::MIN;

        MoveOrder moveOrder = MoveOrder(position_, tableEntry.move, searchStack_[ply].killerMoves);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            const auto [move, moveScore] = scoredMove;
            if (move == searchStack_[ply].excludedMove) {
                continue;
            }

            const bool quiet = position_.quiet(move);

            tTable_.prefetch(position_.zobristAfter(move));

            const UInt64 nodesBefore = nodes_;

            makeMove(ply, move);
            movesTried++;

            if (quiet) {
                quietsTried.add(move);
            } else {
                noisiesTried.add(move);
            }

            const Int32 newDepth = depth - 1;
            Int32 score = 0;

            if (!pvNode || movesTried > 1) {
                score = -search(ply + 1, newDepth, -alpha - 1, -alpha, false, !cutNode);
            }

            if (pvNode && (movesTried == 1 || score > alpha)) {
                score = -search(ply + 1, newDepth, -beta, -alpha, true, false);
            }

            unmakeMove(ply);

            if (timeUp_) {
                return alpha;
            }

            if (rootNode) {
                RootMove &rootMove = findRootMove(move);
                rootMove.prevScore = rootMove.score;
                rootMove.nodes += nodes_ - nodesBefore;

                if (movesTried == 1 || score > alpha) {
                    rootMove.score = score;
                    rootMove.displayScore = score;
                    rootMove.selDepth = selDepth_;
                    rootMove.lowerBound = false;
                    rootMove.upperBound = false;

                    if (score >= beta) {
                        rootMove.displayScore = beta;
                        rootMove.lowerBound = true;
                    } else if (score <= alpha) {
                        rootMove.displayScore = alpha;
                        rootMove.upperBound = true;
                    }

                    rootMove.pv.clear();
                    rootMove.pv.add(move);
                    for (Move pvMove : searchStack_[ply + 1].pv) {
                        rootMove.pv.add(pvMove);
                    }
                } else {
                    rootMove.score = Score::NONE;
                }
            }

            if (score > bestScore) {
                bestScore = score;

                if (bestScore > alpha) {
                    bound = TTableEntry::Bound::EXACT;
                    alpha = bestScore;
                    bestMove = move;
                    if (pvNode) {
                        searchStack_[ply].pv.clear();
                        searchStack_[ply].pv.add(move);
                        for (Move pvMove : searchStack_[ply + 1].pv) {
                            searchStack_[ply].pv.add(pvMove);
                        }
                    }
                }

                if (bestScore >= beta) {
                    bound = TTableEntry::Bound::LOWER;
                    searchStack_[ply].failHighCount++;

                    if (quiet && searchStack_[ply].killerMoves[0] != move) {
                        searchStack_[ply].killerMoves[1] = searchStack_[ply].killerMoves[0];
                        searchStack_[ply].killerMoves[0] = move;
                    }

                    // TODO: history updates

                    break;
                }
            }
        }

        if (movesTried == 0) {
            if (excludedMove) {
                return alpha;
            }
            if (inCheck) {
                return Score::matedIn(rootPly_);
            } else {
                return Score::DRAW;
            }
        }

        if (!excludedMove) {
            // TODO: history update

            tTable_.write(position_.hash(), rootPly_, bestScore, staticEval, bestMove, depth, tablePV, bound);
        }

        return bestScore;
    }

    Int32 quiescenceSearch(USize ply, Int32 alpha, Int32 beta, bool pvNode) noexcept {
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);

        if (timeUp_ || timeManager_.stopHard(limits_, nodes_)) {
            timeUp_ = true;
            return alpha;
        }

        if (rootPly_ + 1 > selDepth_) {
            selDepth_ = rootPly_ + 1;
        }

        if (position_.insufficientMaterial()) {
            return Score::DRAW;
        }

        searchStack_[ply].pv.clear();

        auto [tableEntry, tableHit] = tTable_.probe(position_.hash(), rootPly_);
        const bool tablePV = pvNode || (tableHit && tableEntry.pv);

        if (tableHit && !pvNode && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= beta) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= alpha))) {
            return tableEntry.score;
        }

        const bool inCheck = position_.inCheck();
        Int32 staticEval = Score::NONE;

        if (inCheck) {
            searchStack_[ply].staticEval = Score::NONE;
            searchStack_[ply].eval = Score::NONE;
        } else {
            if (tableHit) {
                staticEval = tableEntry.staticEval;
            } else {
                staticEval = SimPLYChessEval::evaluate(position_); // FIXME: use Eval
            }

            // TODO: history stuff

            searchStack_[ply].staticEval = staticEval;
            searchStack_[ply].eval = searchStack_[ply].staticEval;
            if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= searchStack_[ply].eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= searchStack_[ply].eval))) {
                searchStack_[ply].eval = tableEntry.score;
            }
        }

        if (searchStack_[ply].eval >= beta) {
            if (!tableHit) {
                tTable_.write(position_.hash(), rootPly_, searchStack_[ply].eval, staticEval, Move::NULL_MOVE, 0, tablePV, TTableEntry::Bound::LOWER);
            }
            return searchStack_[ply].eval;
        }

        if (searchStack_[ply].eval > alpha) {
            alpha = searchStack_[ply].eval;
        }

        if (static_cast<USize>(rootPly_) >= MAX_PLY) {
            return alpha;
        }

        // TODO: futility =

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::NONE;

        if (inCheck) {
            bestScore = Score::MIN;
        } else {
            bestScore = searchStack_[ply].eval;
        }

        MoveOrder moveOrder = [&]() {
            if (inCheck) {
                return MoveOrder(position_, tableEntry.move, searchStack_[ply].killerMoves);
            } else {
                return MoveOrder(position_, tableEntry.move);
            }
        }();

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            // TODO: try this:
            // if (!inCheck && movesTried >= 2) {
            //     break;
            // }

            const auto [move, moveScore] = scoredMove;

            // TODO: SEE and futility pruning

            tTable_.prefetch(position_.zobristAfter(move));

            makeMove(ply, move);
            movesTried++;

            const Int32 score = -quiescenceSearch(ply + 1, -beta, -alpha, pvNode);

            unmakeMove(ply);

            if (timeUp_) {
                return alpha;
            }

            if (score > bestScore) {
                bestScore = score;

                if (bestScore > alpha) {
                    alpha = bestScore;
                    bestMove = move;

                    searchStack_[ply].pv.clear();
                    searchStack_[ply].pv.add(move);
                    for (Move pvMove : searchStack_[ply + 1].pv) {
                        searchStack_[ply].pv.add(pvMove);
                    }
                }

                if (bestScore >= beta) {
                    bound = TTableEntry::Bound::LOWER;
                    break;
                }
            }

            // TODO: try this
            // if (position_.quiet(move) && inCheck && bestScore > Score::LOSS) {
            //     break;
            // }
        }

        if (inCheck && movesTried == 0) {
            return Score::matedIn(rootPly_);
        }

        tTable_.write(position_.hash(), rootPly_, bestScore, staticEval, bestMove, 0, tablePV, bound);

        return bestScore;
    }

    bool positionDraw() noexcept {
        bool noMoves = false;
        if (position_.halfmoveClock() >= 100) {
            MoveList moves;
            MoveGen::legal(position_, moves);
            noMoves = moves.empty();
        }
        return position_.draw(rootPly_, noMoves);
    }

    void makeMove(USize ply, Move move) noexcept {
        assert(move != Move::NULL_MOVE);
        position_.make(move);
        rootPly_++;
        nodes_++;
        searchStack_[ply].playedMove = move;
        searchStack_[ply].movedPiece = position_.pieceAt(move.to());
    }

    void unmakeMove(USize ply) noexcept {
        position_.unmake(searchStack_[ply].playedMove);
        rootPly_--;
        searchStack_[ply].playedMove = Move::NULL_MOVE;
        searchStack_[ply].movedPiece = Piece::NONE;
    }

    void makeNullMove() noexcept {
        position_.makeNull();
        rootPly_++;
    }

    void unmakeNullMove() noexcept {
        position_.unmakeNull();
        rootPly_--;
    }

    void initRootMoves() {
        rootMoves_.clear();
        MoveList moves;
        MoveGen::legal(position_, moves);
        for (const Move move : moves) {
            rootMoves_.push_back(RootMove(move));
        }
    }

    void sortRootMoves() {
        auto compare = [](const RootMove &move1, const RootMove &move2) {
            if (move1.score == move2.score) {
                return move1.prevScore > move2.prevScore;
            }
            return move1.score > move2.score;
        };
        std::stable_sort(rootMoves_.begin(), rootMoves_.end(), compare);
    }

    constexpr RootMove &findRootMove(Move move) {
        auto compare = [move](const RootMove &rootMove) {
            return rootMove.move == move;
        };
        return *std::find_if(rootMoves_.begin(), rootMoves_.end(), compare);
    }

    constexpr void printSearchInfo(Int32 depth) const noexcept {
        SearchInfo info;
        info.depth = depth;
        info.selDepth = rootMoves_[0].selDepth;
        info.time = timeManager_.elapsed();
        info.nodes = nodes_;
        info.hashfull = tTable_.hashfull();
        info.score = rootMoves_[0].displayScore;
        info.lowerBound = rootMoves_[0].lowerBound;
        info.upperBound = rootMoves_[0].upperBound;
        info.pv = rootMoves_[0].pv;

        uciPrintSearchInfo_(info);
    }
};

}
