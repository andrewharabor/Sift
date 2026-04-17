#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "eval.hpp"
#include "history.hpp"
#include "move-gen.hpp"
#include "move-order.hpp"
#include "move.hpp"
#include "position.hpp"
#include "score.hpp"
#include "time.hpp"
#include "ttable.hpp"
#include "types.hpp"


namespace Clownfish {

struct SearchStack {
    MoveList pv;

    Move excludedMove;

    std::array<Move, 2> killerMoves;

    Int32 staticEval;
    Int32 eval;

    UInt32 failHighCount;

    HistoryStack history;
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

enum class ThreadFlag : UInt8 {
    IDLE,
    START,
    STOP
};

struct SearchThread {
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    Int32 id;
    std::thread thread;

    std::mutex mutex;
    std::condition_variable condition;
    ThreadFlag flag;

    Position position;
    std::atomic_uint64_t nodes;

    SearchLimits limits;

    USize rootPly;
    Int32 rootDepth;
    Int32 selDepth;

    std::vector<RootMove> rootMoves;

    std::array<SearchStack, MAX_PLY + 1> stack;

    SearchThread(Int32 id, std::thread &&thread) : id(id), thread(std::move(thread)), flag(ThreadFlag::START) { reset(); }

    SearchThread(const SearchThread &) = delete;
    SearchThread &operator=(const SearchThread &) = delete;

    void reset() noexcept {
        nodes.store(0, std::memory_order_relaxed);

        rootPly = 0;
        rootDepth = 0;
        selDepth = 0;

        for (USize i = 0; i <= MAX_PLY; i++) {
            stack[i].pv.clear();
            stack[i].excludedMove = Move::NULL_MOVE;
            stack[i].killerMoves[0] = stack[i].killerMoves[1] = Move::NULL_MOVE;
            stack[i].staticEval = Score::NONE;
            stack[i].eval = Score::NONE;
            stack[i].failHighCount = 0;

            stack[i].history.playedMove = Move::NULL_MOVE;
            stack[i].history.movedPiece = Piece::NONE;
            stack[i].history.score = 0;
        }
    }

    constexpr bool main() const noexcept { return id == 0; }

    void start() noexcept {
        mutex.lock();
        flag = ThreadFlag::START;
        mutex.unlock();
        condition.notify_one();
    }

    void wait() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { return flag == ThreadFlag::IDLE; });
    }

    void join() noexcept {
        mutex.lock();
        flag = ThreadFlag::STOP;
        mutex.unlock();
        condition.notify_one();
        thread.join();
    }

    void initRootMoves() {
        rootMoves.clear();
        MoveList moves;
        MoveGen::legal(position, moves);
        for (const Move move : moves) {
            rootMoves.push_back(RootMove(move));
        }
    }

    void sortRootMoves() {
        auto compare = [](const RootMove &move1, const RootMove &move2) {
            if (move1.score == move2.score) {
                return move1.prevScore > move2.prevScore;
            }
            return move1.score > move2.score;
        };
        std::stable_sort(rootMoves.begin(), rootMoves.end(), compare);
    }

    RootMove &findRootMove(Move move) {
        auto compare = [move](const RootMove &rootMove) {
            return rootMove.move == move;
        };
        return *std::find_if(rootMoves.begin(), rootMoves.end(), compare);
    }
};

class Search {
public:
    using SearchInfoCallback = std::function<void(const SearchInfo &)>;
    using BestMoveCallback = std::function<void(const Move)>;
    using CurrMoveCallback = std::function<void(const Move, Int32, Int32)>;

    static constexpr USize MAX_PLY = SearchThread::MAX_PLY;

    static constexpr MS WINDOW_WIDEN_UPDATE_INTERVAL = MS(1000);
    static constexpr MS CURR_MOVE_UPDATE_INTERVAL = MS(2500);

    Search(USize hashSizeMB, SearchInfoCallback uciSearchInfo, BestMoveCallback uciBestMove, CurrMoveCallback uciCurrMove) : tTable_(hashSizeMB), timeManager_(), uciSearchInfo_(std::move(uciSearchInfo)), uciBestMove_(std::move(uciBestMove)), uciCurrMove_(std::move(uciCurrMove)) {
        stop_.store(false, std::memory_order_relaxed);
        threadCount(1);
    }

    ~Search() noexcept {
        if (running()) {
            stop();
        }
        joinThreads();
    }

    void threadCount(Int32 count) noexcept {
        if (threads_.size() != static_cast<USize>(count)) {
            joinThreads();
            threads_.clear();
            threads_.reserve(static_cast<USize>(count));
            for (Int32 i = 0; i < count; i++) {
                threads_.push_back(std::make_unique<SearchThread>(i, std::thread()));
                auto &thread = threads_.back();
                thread->thread = std::thread([this, &thread] { threadLoop(*thread); });
                thread->wait();
            }
        }
    }

    void newGame() noexcept {
        for (auto &thread : threads_) {
            thread->reset();
        }
        tTable_.reset(threads_.size());
    }

    void run(const Position &position, const SearchLimits &limits) noexcept {
        for (auto &thread : threads_) {
            thread->wait();
        }

        MoveList moves;
        MoveGen::legal(position, moves);
        if (moves.empty()) {
            uciBestMove_(Move::NULL_MOVE);
            return;
        }

        tTable_.incrementAge();

        stop_.store(false, std::memory_order_relaxed);

        timeManager_.limits(limits, position.sideToMove(), moves.size());
        timeManager_.start();

        for (auto &thread : threads_) {
            thread->reset();
            thread->position = position;
            thread->limits = limits;
            thread->initRootMoves();
            thread->start();
        }
    }

    bool running() const noexcept {
        for (const auto &thread : threads_) {
            std::lock_guard<std::mutex> lock(thread->mutex);
            if (thread->flag == ThreadFlag::START) {
                return true;
            }
        }
        return false;
    }

    void stop() noexcept {
        stop_.store(true, std::memory_order_relaxed);
        for (auto &thread : threads_) {
            thread->wait();
        }
    }

    void resizeTTable(USize sizeMB) noexcept { tTable_.resize(sizeMB, threads_.size()); }

private:
    static constexpr Int32 WINDOW_INIT_DELTA = 10;
    static constexpr Int32 WINDOW_MIN_DEPTH = 6;
    static constexpr Int32 WINDOW_MAX_DEPTH_BACKOFF = 5;
    static constexpr Int32 WINDOW_WIDENING_FACTOR = 58;
    static constexpr Int32 WINDOW_WIDENING_SCALE = 256;

    TTable tTable_;

    TimeManager timeManager_;
    std::atomic_bool stop_;

    std::vector<std::unique_ptr<SearchThread>> threads_;

    SearchInfoCallback uciSearchInfo_;
    BestMoveCallback uciBestMove_;
    CurrMoveCallback uciCurrMove_;

    void joinThreads() noexcept {
        for (auto &thread : threads_) {
            thread->join();
        }
    }

    void threadLoop(SearchThread &thread) noexcept {
        while (true) {
            std::unique_lock<std::mutex> lock(thread.mutex);
            thread.condition.notify_one();

            thread.flag = ThreadFlag::IDLE;
            thread.condition.wait(lock, [&thread] { return thread.flag != ThreadFlag::IDLE; });
            ThreadFlag flag = thread.flag;
            lock.unlock();

            if (flag == ThreadFlag::STOP) {
                return;
            } else if (flag == ThreadFlag::START) {
                searchRoot(thread);
            } else {
                assert(false);
            }
        }
    }

    void searchRoot(SearchThread &thread) noexcept {
        const Int32 maxDepth = std::min(thread.limits.depth, static_cast<Int32>(MAX_PLY - 1));
        Int32 score = 0;

        for (Int32 depth = 1; depth <= maxDepth; depth++) {
            thread.rootDepth = depth;
            thread.selDepth = 0;

            Int32 alpha = Score::MIN;
            Int32 beta = Score::MAX;
            Int32 delta = WINDOW_INIT_DELTA + (score * score / ((Score::MAX + 1) / 2));
            Int32 searchDepth = depth;

            if (depth >= WINDOW_MIN_DEPTH) {
                alpha = std::max(score - delta, Score::MIN);
                beta = std::min(score + delta, Score::MAX);
            }

            Int32 searchScore = 0;

            while (true) {
                searchScore = search<true, true>(thread, searchDepth, alpha, beta, false);
                thread.sortRootMoves();

                if (stop_.load(std::memory_order_relaxed)) {
                    break;
                }

                if (thread.main() && (searchScore <= alpha || searchScore >= beta) && timeManager_.elapsed() > WINDOW_WIDEN_UPDATE_INTERVAL) {
                    printSearchInfo(thread, depth);
                }

                if (searchScore <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(alpha - delta, Score::MIN);
                    searchDepth = depth;
                } else if (searchScore >= beta) {
                    beta = std::min(beta + delta, Score::MAX);
                    searchDepth = std::max(searchDepth - 1, depth - WINDOW_MAX_DEPTH_BACKOFF);
                    searchDepth = std::max(searchDepth, 1);
                } else {
                    break;
                }

                delta += (delta * WINDOW_WIDENING_FACTOR) / WINDOW_WIDENING_SCALE;
            }

            thread.sortRootMoves();

            if (thread.main()) {
                printSearchInfo(thread, depth);
            }

            if (stop_.load(std::memory_order_relaxed)) {
                break;
            }

            score = searchScore;

            if (thread.main() && timeManager_.stopSoft(thread.limits, depth, thread.rootMoves[0].move, score, thread.rootMoves[0].nodes, thread.nodes.load(std::memory_order_relaxed))) {
                break;
            }
        }

        if (thread.main()) {
            stop_.store(true, std::memory_order_relaxed);
            uciBestMove_(thread.rootMoves[0].move);
        }
    }

    template<bool PV_NODE = false, bool ROOT_NODE = false>
    Int32 search(SearchThread &thread, Int32 depth, Int32 alpha, Int32 beta, bool cutNode) noexcept {
        static_assert(PV_NODE || !ROOT_NODE);
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);
        assert(!(PV_NODE && cutNode));

        USize &rootPly = thread.rootPly;
        Position &position = thread.position;
        SearchStack &stack = thread.stack[rootPly];

        stack.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.nodes.load(std::memory_order_relaxed), static_cast<Int32>(threads_.size()))) {
            stop_.store(true, std::memory_order_relaxed);
            return alpha;
        }

        if (stop_.load(std::memory_order_relaxed)) {
            return alpha;
        }

        if (static_cast<Int32>(rootPly) + 1 > thread.selDepth) {
            thread.selDepth = static_cast<Int32>(rootPly) + 1;
        }

        depth = std::min(depth, static_cast<Int32>(MAX_PLY - 1));

        alpha = std::max(alpha, Score::matedIn(static_cast<Int32>(rootPly)));
        beta = std::min(beta, Score::mateIn(static_cast<Int32>(rootPly)));
        if (alpha >= beta) {
            return alpha;
        }

        const bool inCheck = position.inCheck();
        const bool excludedMove = stack.excludedMove != Move::NULL_MOVE;

        const Int32 draw = Score::draw(thread.nodes.load(std::memory_order_relaxed));

        if constexpr (!ROOT_NODE) {
            if (position.halfmoveClock() >= 3 && alpha < Score::DRAW && position.upcomingRepetition(static_cast<Int32>(rootPly))) {
                alpha = draw;
                if (alpha >= beta) {
                    return alpha;
                }
            }
        }

        if (positionDraw(thread)) {
            return draw;
        }

        if (rootPly >= MAX_PLY) {
            if (inCheck) {
                return 0;
            }
            return Eval::evaluate(position);
        }

        SearchStack &nextStack = thread.stack[rootPly + 1];

        if (depth <= 0) {
            return quiescenceSearch<PV_NODE>(thread, alpha, beta);
        }

        TTableEntry tableEntry = TTableEntry();
        bool tableHit = false;

        Int32 staticEval = Score::NONE;

        // FIXME: correplexity?

        if (!excludedMove) {
            std::tie(tableEntry, tableHit) = tTable_.probe(position.hash(), static_cast<Int32>(rootPly));

            if constexpr (!PV_NODE) {
                if (tableHit && tableEntry.depth >= depth && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= beta) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= alpha))) {
                    return tableEntry.score;
                }
            }

            if (inCheck) {
                stack.staticEval = Score::NONE;
                stack.eval = Score::NONE;
            } else {
                if (tableHit) {
                    staticEval = tableEntry.staticEval;
                } else {
                    staticEval = Eval::evaluate(position);
                }

                // FIXME: history
                stack.staticEval = staticEval;
                stack.eval = stack.staticEval;
                if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= stack.eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= stack.eval))) {
                    stack.eval = tableEntry.score;
                }
            }
        }

        const bool tablePV = PV_NODE || (tableHit && tableEntry.pv);

        nextStack.killerMoves[0] = nextStack.killerMoves[1] = Move::NULL_MOVE;

        // TODO: more pruning

        nextStack.failHighCount = 0;

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        MoveList quietsTried;
        MoveList noisiesTried;
        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::MIN;

        MoveOrder moveOrder = MoveOrder(position, tableEntry.move, stack.killerMoves);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            const auto [move, moveScore] = scoredMove;
            if (move == stack.excludedMove) {
                continue;
            }

            if constexpr (ROOT_NODE) {
                if (thread.main() && timeManager_.elapsed() > CURR_MOVE_UPDATE_INTERVAL) {
                    uciCurrMove_(move, movesTried + 1, thread.rootDepth);
                }
            }

            const bool quiet = position.quiet(move);

            tTable_.prefetch(position.zobristAfter(move));

            const UInt64 nodesBefore = thread.nodes.load(std::memory_order_relaxed);

            makeMove(thread, move, 0); // FIXME: histScore
            movesTried++;

            if (quiet) {
                quietsTried.add(move);
            } else {
                noisiesTried.add(move);
            }

            const Int32 newDepth = depth - 1;
            Int32 score = 0;

            if (!PV_NODE || movesTried > 1) {
                score = -search<false, false>(thread, newDepth, -alpha - 1, -alpha, !cutNode);
            }

            if constexpr (PV_NODE) {
                if (movesTried == 1 || score > alpha) {
                    score = -search<true, false>(thread, newDepth, -beta, -alpha, false);
                }
            }

            unmakeMove(thread);

            if (stop_.load(std::memory_order_relaxed)) {
                return alpha;
            }

            if constexpr (ROOT_NODE) {
                RootMove &rootMove = thread.findRootMove(move);
                rootMove.prevScore = rootMove.score;
                rootMove.nodes += thread.nodes.load(std::memory_order_relaxed) - nodesBefore;

                if (movesTried == 1 || score > alpha) {
                    rootMove.score = score;
                    rootMove.displayScore = score;
                    rootMove.selDepth = thread.selDepth;
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
                    for (Move pvMove : nextStack.pv) {
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
                    if constexpr (PV_NODE) {
                        stack.pv.clear();
                        stack.pv.add(move);
                        for (Move pvMove : nextStack.pv) {
                            stack.pv.add(pvMove);
                        }
                    }
                }

                if (bestScore >= beta) {
                    bound = TTableEntry::Bound::LOWER;
                    stack.failHighCount++;

                    if (quiet && stack.killerMoves[0] != move) {
                        stack.killerMoves[1] = stack.killerMoves[0];
                        stack.killerMoves[0] = move;
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
                return Score::matedIn(static_cast<Int32>(rootPly));
            } else {
                return Score::DRAW;
            }
        }

        if (!excludedMove) {
            // TODO: history update

            tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, staticEval, bestMove, depth, tablePV, bound);
        }

        return bestScore;
    }

    template<bool PV_NODE = false>
    Int32 quiescenceSearch(SearchThread &thread, Int32 alpha, Int32 beta) noexcept {
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);

        USize &rootPly = thread.rootPly;
        Position &position = thread.position;
        SearchStack &stack = thread.stack[rootPly];

        stack.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.nodes.load(std::memory_order_relaxed), static_cast<Int32>(threads_.size()))) {
            stop_.store(true, std::memory_order_relaxed);
            return alpha;
        }

        if (stop_.load(std::memory_order_relaxed)) {
            return alpha;
        }

        if (static_cast<Int32>(rootPly) + 1 > thread.selDepth) {
            thread.selDepth = static_cast<Int32>(rootPly) + 1;
        }

        const bool inCheck = position.inCheck();

        const Int32 draw = Score::draw(thread.nodes.load(std::memory_order_relaxed));

        if (position.halfmoveClock() >= 3 && alpha < Score::DRAW && position.upcomingRepetition(static_cast<Int32>(rootPly))) {
            alpha = draw;
            if (alpha >= beta) {
                return alpha;
            }
        }

        if (positionDraw(thread)) {
            return draw;
        }

        if (rootPly >= MAX_PLY) {
            if (inCheck) {
                return 0;
            }
            return Eval::evaluate(position);
        }

        auto [tableEntry, tableHit] = tTable_.probe(position.hash(), static_cast<Int32>(rootPly));
        const bool tablePV = PV_NODE || (tableHit && tableEntry.pv);

        if constexpr (!PV_NODE) {
            if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= beta) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= alpha))) {
                return tableEntry.score;
            }
        }

        Int32 staticEval = Score::NONE;

        if (inCheck) {
            stack.staticEval = Score::NONE;
            stack.eval = Score::NONE;
        } else {
            staticEval = (tableHit) ? tableEntry.staticEval : Eval::evaluate(position);

            // TODO: history stuff

            stack.staticEval = staticEval;
            stack.eval = stack.staticEval;
            if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= stack.eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= stack.eval))) {
                stack.eval = tableEntry.score;
            }
        }

        if (stack.eval >= beta) {
            if (!tableHit) {
                tTable_.write(position.hash(), static_cast<Int32>(rootPly), stack.eval, staticEval, Move::NULL_MOVE, 0, tablePV, TTableEntry::Bound::LOWER);
            }
            return stack.eval;
        }

        if (stack.eval > alpha) {
            alpha = stack.eval;
        }

        SearchStack &nextStack = thread.stack[rootPly + 1];

        // TODO: futility =

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = (inCheck) ? Score::MIN : stack.eval;

        MoveOrder moveOrder = [&]() {
            if (inCheck) {
                return MoveOrder(position, tableEntry.move, stack.killerMoves);
            } else {
                return MoveOrder(position, tableEntry.move);
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

            tTable_.prefetch(position.zobristAfter(move));

            makeMove(thread, move, 0);
            movesTried++;

            const Int32 score = -quiescenceSearch<PV_NODE>(thread, -beta, -alpha);

            unmakeMove(thread);

            if (stop_.load(std::memory_order_relaxed)) {
                return alpha;
            }

            if (score > bestScore) {
                bestScore = score;

                if (bestScore > alpha) {
                    alpha = bestScore;
                    bestMove = move;

                    stack.pv.clear();
                    stack.pv.add(move);
                    for (Move pvMove : nextStack.pv) {
                        stack.pv.add(pvMove);
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
            return Score::matedIn(static_cast<Int32>(rootPly));
        }

        tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, staticEval, bestMove, 0, tablePV, bound);

        return bestScore;
    }

    bool positionDraw(SearchThread &thread) const noexcept {
        bool noMoves = false;
        if (thread.position.halfmoveClock() >= 100) {
            MoveList moves;
            MoveGen::legal(thread.position, moves);
            noMoves = moves.empty();
        }
        return thread.position.draw(static_cast<Int32>(thread.rootPly), noMoves);
    }

    void makeMove(SearchThread &thread, Move move, Int32 historyScore) noexcept {
        assert(move != Move::NULL_MOVE);

        SearchStack &stack = thread.stack[thread.rootPly];

        stack.history.playedMove = move;
        stack.history.movedPiece = thread.position.pieceAt(move.from());
        stack.history.score = historyScore;

        thread.position.make(move);
        thread.nodes.store(thread.nodes.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        thread.rootPly++;
    }

    void unmakeMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);

        thread.rootPly--;

        SearchStack &stack = thread.stack[thread.rootPly];

        thread.position.unmake(stack.history.playedMove);

        stack.history.playedMove = Move::NULL_MOVE;
        stack.history.movedPiece = Piece::NONE;
        stack.history.score = 0;

    }

    void makeNullMove(SearchThread &thread) noexcept {
        thread.position.makeNull();
        thread.rootPly++;
    }

    void unmakeNullMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmakeNull();
    }

    void printSearchInfo(SearchThread &thread, Int32 depth) const noexcept {
        SearchInfo info;
        info.depth = depth;
        info.selDepth = thread.rootMoves[0].selDepth;
        info.time = timeManager_.elapsed();
        info.nodes = 0;
        for (auto &searchThread : threads_) {
            info.nodes += searchThread->nodes.load(std::memory_order_relaxed);
        }
        info.hashfull = tTable_.hashfull();
        info.score = thread.rootMoves[0].displayScore;
        info.lowerBound = thread.rootMoves[0].lowerBound;
        info.upperBound = thread.rootMoves[0].upperBound;
        info.pv = thread.rootMoves[0].pv;

        uciSearchInfo_(info);
    }
};

}
