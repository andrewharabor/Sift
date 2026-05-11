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


namespace Syft {

struct SearchStack {
    MoveList pv;

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

    USize pvIndex;

    MoveList pv;
};

struct RootMove {
    Move move = Move::NULL_MOVE;
    MoveList pv;

    Int32 score = Score::NONE;
    Int32 prevScore = Score::NONE;
    Int32 windowScore = Score::NONE;
    Int32 displayScore = Score::NONE;
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
    USize nmpMinPly;
    Int32 rootDepth;
    Int32 selDepth;

    USize pvIndex;

    std::vector<RootMove> rootMoves;

    std::array<SearchStack, MAX_PLY + 1> stack;

    History history;

    SearchThread(Int32 id, std::thread &&thread) : id(id), thread(std::move(thread)), flag(ThreadFlag::START), history() { reset(); }

    SearchThread(const SearchThread &) = delete;
    SearchThread &operator=(const SearchThread &) = delete;

    void reset() noexcept {
        nodes.store(0, std::memory_order_relaxed);

        rootPly = 0;
        rootDepth = 0;
        selDepth = 0;
        nmpMinPly = 0;

        pvIndex = 0;

        for (USize i = 0; i <= MAX_PLY; i++) {
            stack[i].pv.clear();
            stack[i].excludedMove = Move::NULL_MOVE;
            stack[i].killerMoves[0] = stack[i].killerMoves[1] = Move::NULL_MOVE;
            stack[i].staticEval = Score::NONE;
            stack[i].eval = Score::NONE;
            stack[i].failHighCount = 0;

            history.stack[i].playedMove = Move::NULL_MOVE;
            history.stack[i].movedPiece = Piece::NONE;
            history.stack[i].contCorrEntry = nullptr;
            history.stack[i].contEntry = nullptr;
            history.stack[i].score = 0;
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

    void initMoves() {
        rootMoves.clear();
        MoveList moves;
        if (limits.moves.empty()) {
            MoveGen::legal(position, moves);
        } else {
            moves = limits.moves;
        }

        for (const Move move : moves) {
            rootMoves.push_back(RootMove(move));
        }
    }

    void sortSearchedMoves() {
        auto compare = [](const RootMove &move1, const RootMove &move2) {
            if (move1.score == move2.score) {
                return move1.prevScore > move2.prevScore;
            }
            return move1.score > move2.score;
        };
        std::stable_sort(rootMoves.begin(), rootMoves.begin() + static_cast<Int64>(pvIndex) + 1, compare);
    }

    void sortRemainingMoves() {
        auto compare = [](const RootMove &move1, const RootMove &move2) {
            if (move1.score == move2.score) {
                return move1.prevScore > move2.prevScore;
            }
            return move1.score > move2.score;
        };
        std::stable_sort(rootMoves.begin() + static_cast<Int64>(pvIndex), rootMoves.end(), compare);
    }

    USize findRootMove(Move move) {
        auto compare = [move](const RootMove &rootMove) {
            return rootMove.move == move;
        };
        auto it = std::find_if(rootMoves.begin() + static_cast<Int64>(pvIndex), rootMoves.end(), compare);
        if (it != rootMoves.end()) {
            return static_cast<USize>(it - rootMoves.begin());
        }
        return rootMoves.size();
    }
};

class Search {
public:
    using SearchInfoCallback = std::function<void(const SearchInfo &)>;
    using BestMoveCallback = std::function<void(const Move)>;
    using CurrMoveCallback = std::function<void(const Move, Int32, Int32)>;

    static constexpr USize MAX_PLY = SearchThread::MAX_PLY;

    static constexpr MS CURR_MOVE_UPDATE_INTERVAL = MS(2500);

    Search(USize hashSizeMB, USize multiPV, SearchInfoCallback uciSearchInfo, BestMoveCallback uciBestMove, CurrMoveCallback uciCurrMove) : tTable_(hashSizeMB), multiPV_(multiPV), timeManager_(), uciSearchInfo_(std::move(uciSearchInfo)), uciBestMove_(std::move(uciBestMove)), uciCurrMove_(std::move(uciCurrMove)) {
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
            thread->history.reset();
        }
        tTable_.reset(threads_.size());
    }

    void run(const Position &position, const SearchLimits &limits) noexcept {
        for (auto &thread : threads_) {
            thread->wait();
        }

        MoveList legalMoves;
        MoveGen::legal(position, legalMoves);
        if (legalMoves.empty()) {
            uciBestMove_(Move::NULL_MOVE);
            return;
        }

        tTable_.incrementAge();

        stop_.store(false, std::memory_order_relaxed);

        timeManager_.limits(limits, position.sideToMove(), legalMoves.size());
        timeManager_.start();

        for (auto &thread : threads_) {
            thread->reset();
            thread->position = position;
            thread->limits = limits;
            thread->initMoves();
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

    void multiPV(USize multiPV) noexcept { multiPV_ = multiPV; }

private:
    static constexpr Int32 WINDOW_INIT_DELTA = 10;
    static constexpr Int32 WINDOW_MIN_DEPTH = 6;
    static constexpr Int32 WINDOW_MAX_DEPTH_REDUCTION = 5;
    static constexpr Int32 WINDOW_WIDENING_COEFF = 58;
    static constexpr Int32 WINDOW_WIDENING_SCALE = 256;

    static constexpr Int32 RFP_MAX_DEPTH = 8;
    static constexpr Int32 RFP_IMPROVING_MARGIN = 27;
    static constexpr Int32 RFP_NON_IMPROVING_MARGIN = 78;
    static constexpr Int32 RFP_WINNING_THREATS = 21;
    static constexpr Int32 RFP_OPPONENT_WORSENING = 14;
    static constexpr Int32 RFP_HISTORY_DIVISOR = 410;
    static constexpr Int32 RFP_MIN_MARGIN = 20;

    static constexpr Int32 RAZORING_MAX_DEPTH = 3;
    static constexpr Int32 RAZORING_MARGIN = 456;
    static constexpr Int32 RAZORING_MAX_ALPHA = 2000;

    static constexpr Int32 NMP_MIN_DEPTH = 2;
    static constexpr Int32 NMP_EVAL_MARGIN = 30;
    static constexpr Int32 NMP_STATIC_EVAL_BASE_MARGIN = 184;
    static constexpr Int32 NMP_STATIC_EVAL_DEPTH_MARGIN = 19;
    static constexpr Int32 NMP_BASE_REDUCTION = 1320;
    static constexpr Int32 NMP_DEPTH_REDUCTION_SCALE = 74;
    static constexpr Int32 NMP_REDUCTION_DIVISOR = 256;
    static constexpr Int32 NMP_EVAL_REDUCTION_SCALE = 215;
    static constexpr Int32 NMP_MAX_EVAL_REDUCTION = 4;
    static constexpr Int32 NMP_NO_VERIFICATION_MAX_DEPTH = 15;
    static constexpr USize NMP_MIN_PLY_DEPTH_SCALE = 3;
    static constexpr USize NMP_MIN_PLY_DEPTH_DIVISOR = 4;

    static constexpr Int32 HIGH_COMPLEXITY_MARGIN = 87;

    static constexpr Int32 HISTORY_PRUNING_MAX_DEPTH = 7;
    static constexpr Int32 HISTORY_PRUNING_MARGIN = 1743;
    static constexpr Int32 HISTORY_BETA_MARGIN = 39;

    static constexpr Int32 LMR_BASE = 775;
    static constexpr Int32 LMR_SCALE = 427;
    static constexpr Int32 LMR_HISTORY_SCALE = 1024;
    static constexpr Int32 LMR_QUIET_HISTORY_DIVISOR = 9043;
    static constexpr Int32 LMR_NOISY_HISTORY_DIVISOR = 6598;
    static constexpr Int32 LMR_MIN_DEPTH = 3;
    static constexpr Int32 LMR_MIN_MOVES_PV = 4;
    static constexpr Int32 LMR_MIN_MOVES_NON_PV = 3;
    static constexpr Int32 LMR_NON_IMPROVING_SCALE = 1478;
    static constexpr Int32 LMR_NOISY_HASH_MOVE_SCALE = 1082;
    static constexpr Int32 LMR_TABLE_PV_SCALE = 954;
    static constexpr Int32 LMR_TABLE_PV_NON_FAIL_LOW_SCALE = 484;
    static constexpr Int32 LMR_GIVES_CHECK_SCALE = 573;
    static constexpr Int32 LMR_IN_CHECK_SCALE = 592;
    static constexpr Int32 LMR_HIGH_COMPLEXITY_SCALE = 593;
    static constexpr Int32 LMR_CUTNODE_SCALE = 1612;
    static constexpr Int32 LMR_FAIL_HIGH_COUNT_SCALE = 1042;
    static constexpr UInt32 LMR_FAIL_HIGH_COUNT_MARGIN = 2;
    static constexpr Int32 LMR_REDUCTION_DIVISOR = 1024;

    static constexpr USize LMR_TABLE_SIZE_DEPTH = 64;
    static constexpr USize LMR_TABLE_SIZE_MOVES = 64;

    static inline MultiArray<Int32, LMR_TABLE_SIZE_DEPTH, LMR_TABLE_SIZE_MOVES> LMR_TABLE = []() {
        MultiArray<Int32, LMR_TABLE_SIZE_DEPTH, LMR_TABLE_SIZE_MOVES> table = {};
        for (USize depth = 1; depth < LMR_TABLE_SIZE_DEPTH; depth++) {
            for (USize moves = 1; moves < LMR_TABLE_SIZE_MOVES; moves++) {
                Float64 base = static_cast<Float64>(LMR_BASE);
                Float64 scale = static_cast<Float64>(LMR_SCALE);
                table[depth][moves] = static_cast<Int32>(base + scale * std::log(static_cast<Float64>(depth)) * std::log(static_cast<Float64>(moves)));
            }
        }
        return table;
    }();

    static constexpr Int32 DEEPER_SEARCH_MARGIN_BASE = 38;
    static constexpr Int32 DEEPER_SEARCH_MARGIN_DEPTH_SCALE = 143;
    static constexpr Int32 DEEPER_SEARCH_MARGIN_DEPTH_DIVISOR = 64;
    static constexpr Int32 SHALLOWER_SEARCH_MARGIN = 8;

    static constexpr Int32 FP_MAX_DEPTH = 8;
    static constexpr Int32 FP_BASE_MARGIN = 146;
    static constexpr Int32 FP_DEPTH_SCALE = 128;
    static constexpr Int32 FP_HISTORY_DIVISOR = 393;
    static constexpr Int32 FP_MARGIN_MIN = 20;

    static constexpr Int32 NOISY_FP_MAX_DEPTH = 5;
    static constexpr Int32 NOISY_FP_BASE_MARGIN = 4;
    static constexpr Int32 NOISY_FP_DEPTH_SCALE = 113;
    static constexpr Int32 NOISY_FP_HIST_DIVISOR = 253;
    static constexpr Int32 NOISY_FP_MARGIN_MIN = 20;

    static constexpr Int32 LMP_MARGIN_IMPROVING_BASE = 553;
    static constexpr Int32 LMP_MARGIN_IMPROVING_DEPTH_SCALE = 333;
    static constexpr Int32 LMP_MARGIN_NON_IMPROVING_BASE = 566;
    static constexpr Int32 LMP_MARGIN_NON_IMPROVING_DEPTH_SCALE = 103;
    static constexpr Int32 LMP_MARGIN_DIVISOR = 256;

    TTable tTable_;

    USize multiPV_;

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
        for (Int32 depth = 1; depth <= maxDepth; depth++) {
            thread.rootDepth = depth;
            thread.selDepth = 0;

            for (RootMove &rootMove : thread.rootMoves) {
                rootMove.prevScore = rootMove.score;
            }

            for (thread.pvIndex = 0; thread.pvIndex < std::min(multiPV_, thread.rootMoves.size()); thread.pvIndex++) {

                const RootMove &rootMove = thread.rootMoves[thread.pvIndex];

                Int32 alpha = Score::MIN;
                Int32 beta = Score::MAX;
                Int32 delta = WINDOW_INIT_DELTA + (rootMove.windowScore * rootMove.windowScore / ((Score::MAX + 1) / 2));
                Int32 searchDepth = depth;

                if (depth >= WINDOW_MIN_DEPTH) {
                    alpha = std::max(rootMove.windowScore - delta, Score::MIN);
                    beta = std::min(rootMove.windowScore + delta, Score::MAX);
                }

                while (true) {
                    Int32 score = search<true, true>(thread, searchDepth, alpha, beta, false);
                    thread.sortRemainingMoves();

                    if (stop_.load(std::memory_order_relaxed)) {
                        break;
                    }

                    if (thread.main() && (score <= alpha || score >= beta) && multiPV_ == 1) {
                        printSearchInfo(thread, thread.pvIndex, depth);
                    }

                    if (score <= alpha) {
                        beta = (alpha + beta) / 2;
                        alpha = std::max(alpha - delta, Score::MIN);
                        searchDepth = depth;
                    } else if (score >= beta) {
                        beta = std::min(beta + delta, Score::MAX);
                        searchDepth = std::max(searchDepth - 1, depth - WINDOW_MAX_DEPTH_REDUCTION);
                        searchDepth = std::max(searchDepth, 1);
                    } else {
                        break;
                    }

                    delta += delta * WINDOW_WIDENING_COEFF / WINDOW_WIDENING_SCALE;
                }

                thread.sortSearchedMoves();

                if (stop_.load(std::memory_order_relaxed)) {
                    break;
                }
            }

            if (thread.main()) {
                for (USize idx = 0; idx < std::min(multiPV_, thread.rootMoves.size()); idx++) {
                    printSearchInfo(thread, idx, depth);
                }
            }

            if (stop_.load(std::memory_order_relaxed)) {
                break;
            }

            if (thread.main() && timeManager_.stopSoft(thread.limits, depth, thread.rootMoves[0].move, thread.rootMoves[0].score, thread.rootMoves[0].nodes, thread.nodes.load(std::memory_order_relaxed))) {
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
        History &history = thread.history;

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
            if (position.halfmoveClock() >= 3 && alpha < Score::DRAW && position.upcomingRepetition(rootPly)) {
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
            return (inCheck) ? 0 : Eval::evaluate(position);
        }

        SearchStack &nextStack = thread.stack[rootPly + 1];

        if (depth <= 0) {
            return quiescenceSearch<PV_NODE>(thread, alpha, beta);
        }

        TTableEntry tableEntry = TTableEntry();
        bool tableHit = false;

        Int32 rawStaticEval = Score::NONE;
        Int32 complexity = 0;

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
                rawStaticEval = (tableHit) ? tableEntry.staticEval : Eval::evaluate(position);
                stack.staticEval = history.correct(position, rawStaticEval, rootPly);
                complexity = std::abs(stack.staticEval - rawStaticEval);

                stack.eval = stack.staticEval;
                if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= stack.eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= stack.eval))) {
                    stack.eval = tableEntry.score;
                }
            }
        }

        const Move hashMove = ROOT_NODE ? thread.rootMoves[thread.pvIndex].move : tableEntry.move;
        const bool noisyHashMove = hashMove != Move::NULL_MOVE && position.noisy(hashMove);
        const bool tablePV = PV_NODE || (tableHit && tableEntry.pv);

        nextStack.killerMoves[0] = nextStack.killerMoves[1] = Move::NULL_MOVE;

        // const Bitboard threats = position.threats(); // FIXME
        const bool winningThreats = bool(position.winningThreats());

        bool improving = [&]() {
            if (inCheck) {
                return false;
            }
            if (rootPly > 1 && thread.stack[rootPly - 2].staticEval != Score::NONE) {
                return stack.staticEval > thread.stack[rootPly - 2].staticEval;
            }
            if (rootPly > 3 && thread.stack[rootPly - 4].staticEval != Score::NONE) {
                return stack.staticEval > thread.stack[rootPly - 4].staticEval;
            }
            return true;
        }();

        bool opponentWorsening = !inCheck && rootPly > 0 && (thread.stack[rootPly - 1].staticEval != Score::NONE) && (stack.staticEval > -thread.stack[rootPly - 1].staticEval + 1);

        if constexpr (!PV_NODE) {
            if (!inCheck && !excludedMove) {
                Int32 rfpMargin = ((improving) ? (RFP_IMPROVING_MARGIN + RFP_WINNING_THREATS * winningThreats) : RFP_NON_IMPROVING_MARGIN) * depth - (RFP_OPPONENT_WORSENING * opponentWorsening) + (thread.history.stack[rootPly - 1].score / RFP_HISTORY_DIVISOR);
                if (depth <= RFP_MAX_DEPTH && std::abs(stack.eval) < Score::KNOWN_WIN && stack.eval >= std::max(rfpMargin, RFP_MIN_MARGIN) + beta) {
                    return stack.eval;
                }
            }

            if (depth <= RAZORING_MAX_DEPTH && stack.eval <= alpha - RAZORING_MARGIN * depth && alpha < RAZORING_MAX_ALPHA) {
                Int32 score = quiescenceSearch<PV_NODE>(thread, alpha, beta);
                if (score <= alpha) {
                    return score;
                }
            }

            if (position.nullPly() > 0 && rootPly >= thread.nmpMinPly && depth >= NMP_MIN_DEPTH && stack.eval >= beta + NMP_EVAL_MARGIN && stack.staticEval >= beta + NMP_STATIC_EVAL_BASE_MARGIN - NMP_STATIC_EVAL_DEPTH_MARGIN * depth && position.nonPawnMaterial(position.sideToMove())) {
                Int32 reduction = (NMP_BASE_REDUCTION + NMP_DEPTH_REDUCTION_SCALE * depth) / NMP_REDUCTION_DIVISOR + std::min((stack.eval - beta) / NMP_EVAL_REDUCTION_SCALE, NMP_MAX_EVAL_REDUCTION);

                makeNullMove(thread);
                Int32 nullMoveScore = -search<false, false>(thread, depth - reduction, -beta, -beta + 1, !cutNode);
                unmakeNullMove(thread);

                if (nullMoveScore >= beta) {
                    if ((depth <= NMP_NO_VERIFICATION_MAX_DEPTH && std::abs(beta) < Score::KNOWN_WIN) || thread.nmpMinPly > 0) {
                        return Score::mate(nullMoveScore) ? beta : nullMoveScore;
                    }

                    thread.nmpMinPly = rootPly + static_cast<USize>(depth - reduction) * NMP_MIN_PLY_DEPTH_SCALE / NMP_MIN_PLY_DEPTH_DIVISOR;
                    Int32 verificationScore = search<false, false>(thread, depth - reduction, beta - 1, beta, true);
                    thread.nmpMinPly = 0;

                    if (verificationScore >= beta) {
                        return verificationScore;
                    }
                }
            }
        }

        // TODO
        // probcut
        // IIR

        nextStack.failHighCount = 0;

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        MoveList quietsTried;
        MoveList noisiesTried;
        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::MIN;

        MoveOrder moveOrder = MoveOrder(position, history, hashMove, stack.killerMoves, rootPly);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            const auto [move, moveScore] = scoredMove;
            if (move == stack.excludedMove) {
                continue;
            }

            if constexpr (ROOT_NODE) {
                if (thread.findRootMove(move) >= thread.rootMoves.size()) {
                    continue;
                }

                if (thread.main() && timeManager_.elapsed() > CURR_MOVE_UPDATE_INTERVAL) {
                    uciCurrMove_(move, movesTried + 1, thread.rootDepth);
                }
            }

            const bool quiet = position.quiet(move);
            Int32 historyScore = (quiet) ? history.quietStats(position, move, rootPly) : history.noisyStats(position, move);
            Int32 baseLMR = LMR_TABLE[std::min(static_cast<USize>(depth), LMR_TABLE_SIZE_DEPTH - 1)][std::min(static_cast<USize>(movesTried), LMR_TABLE_SIZE_MOVES - 1)];
            baseLMR -= LMR_HISTORY_SCALE * historyScore / (quiet ? LMR_QUIET_HISTORY_DIVISOR : LMR_NOISY_HISTORY_DIVISOR);

            if constexpr (!ROOT_NODE) {
                if (moveScore < MoveScore::KILLER2 && bestScore > Score::LOSS) {
                    Int32 lmrDepth = std::max(depth - baseLMR / LMR_REDUCTION_DIVISOR, 0);
                    Int32 fpMargin = std::max(FP_BASE_MARGIN + FP_DEPTH_SCALE * lmrDepth + historyScore / FP_HISTORY_DIVISOR, FP_MARGIN_MIN);
                    if (lmrDepth <= FP_MAX_DEPTH && quiet && !inCheck && alpha < Score::WIN && stack.staticEval + fpMargin <= alpha) {
                        break;
                    }

                    Int32 noisyFPMargin = std::max(NOISY_FP_BASE_MARGIN + NOISY_FP_DEPTH_SCALE * depth + historyScore / NOISY_FP_HIST_DIVISOR, NOISY_FP_MARGIN_MIN);
                    if (depth <= NOISY_FP_MAX_DEPTH && !quiet && !inCheck && alpha < Score::WIN && stack.staticEval + noisyFPMargin <= alpha) {
                        break;
                    }

                    Int32 lmpMargin = ((improving || complexity > HIGH_COMPLEXITY_MARGIN) ? (LMP_MARGIN_IMPROVING_BASE + LMP_MARGIN_IMPROVING_DEPTH_SCALE * depth * depth) : (LMP_MARGIN_NON_IMPROVING_BASE + LMP_MARGIN_NON_IMPROVING_DEPTH_SCALE * depth * depth)) / LMP_MARGIN_DIVISOR;
                    if (!inCheck && movesTried >= lmpMargin) {
                        break;
                    }

                    // TODO:
                    // SEE pruning
                    // history pruning:
                }
            }

            // TODO:
            // SE

            tTable_.prefetch(position.zobristAfter(move));

            const UInt64 nodesBefore = thread.nodes.load(std::memory_order_relaxed);

            makeMove(thread, move, historyScore);
            movesTried++;

            if (quiet) {
                quietsTried.add(move);
            } else {
                noisiesTried.add(move);
            }

            const bool givesCheck = position.givesCheck();

            // TODO:
            // check extensions

            Int32 newDepth = depth - 1;
            Int32 score = 0;

            if (depth >= LMR_MIN_DEPTH && movesTried >= (PV_NODE ? LMR_MIN_MOVES_PV : LMR_MIN_MOVES_NON_PV) && (!tablePV || moveScore <= MoveScore::KILLER1)) {
                Int32 reduction = baseLMR;
                reduction += LMR_NON_IMPROVING_SCALE * !improving;
                reduction += LMR_NOISY_HASH_MOVE_SCALE * noisyHashMove;
                if (tablePV) {
                    reduction -= LMR_TABLE_PV_SCALE + LMR_TABLE_PV_NON_FAIL_LOW_SCALE * (tableHit && tableEntry.score > alpha);
                }
                reduction -= LMR_GIVES_CHECK_SCALE * givesCheck;
                reduction -= LMR_IN_CHECK_SCALE * inCheck;
                reduction -= LMR_HIGH_COMPLEXITY_SCALE * (complexity > HIGH_COMPLEXITY_MARGIN);
                reduction += LMR_CUTNODE_SCALE * cutNode;
                reduction += LMR_FAIL_HIGH_COUNT_SCALE * (nextStack.failHighCount >= LMR_FAIL_HIGH_COUNT_MARGIN);

                Int32 reducedDepth = std::min(std::max(newDepth - reduction / LMR_REDUCTION_DIVISOR, 1), newDepth);
                score = -search<false, false>(thread, reducedDepth, -alpha - 1, -alpha, true);
                if (score > alpha && reducedDepth < newDepth) {
                    if (score > bestScore + DEEPER_SEARCH_MARGIN_BASE + (DEEPER_SEARCH_MARGIN_DEPTH_SCALE * depth) / DEEPER_SEARCH_MARGIN_DEPTH_DIVISOR) {
                        newDepth++;
                    }
                    if (score < bestScore + SHALLOWER_SEARCH_MARGIN) {
                        newDepth--;
                    }
                    score = -search<false, false>(thread, newDepth, -alpha - 1, -alpha, !cutNode);

                    if (quiet && (score <= alpha || score >= beta)) {
                        Int32 bonus = (score >= beta) ? history.bonus(depth) : history.penalty(depth);
                        history.updateCont(position, move, rootPly, bonus);
                    }
                }
            } else if (!PV_NODE || movesTried > 1) {
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
                USize rootMoveIndex = thread.findRootMove(move);
                if (rootMoveIndex >= thread.rootMoves.size()) {
                    continue;
                }

                RootMove &rootMove = thread.rootMoves[rootMoveIndex];
                rootMove.windowScore = rootMove.score;
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

                    Int32 historyDepth = depth + ((bestScore > beta) + HISTORY_BETA_MARGIN);
                    Int32 bonus = history.bonus(historyDepth);
                    Int32 penalty = history.penalty(historyDepth);

                    if (quiet) {
                        history.updateQuietStats(position, move, rootPly, bonus);
                        for (const Move quietMove : quietsTried) {
                            if (quietMove != move) {
                                history.updateQuietStats(position, quietMove, rootPly, penalty);
                            }
                        }
                    } else {
                        history.updateNoisyStats(position, move, bonus);
                    }

                    for (const Move noisyMove : noisiesTried) {
                        if (noisyMove != move) {
                            history.updateNoisyStats(position, noisyMove, penalty);
                        }
                    }

                    break;
                }
            }
        }

        if (movesTried == 0) {
            if (excludedMove) {
                return alpha;
            }

            return (inCheck) ? Score::matedIn(static_cast<Int32>(rootPly)) : Score::DRAW;
        }

        if (!excludedMove) {
            if (!inCheck && (bestMove == Move::NULL_MOVE || position.quiet(bestMove)) && !(bound == TTableEntry::Bound::LOWER && stack.staticEval >= bestScore) && !(bound == TTableEntry::Bound::UPPER && stack.staticEval <= bestScore)) {
                history.updateCorr(position, depth, rootPly, bestScore - stack.staticEval);
            }

            if (!ROOT_NODE || thread.pvIndex == 0) {
                tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, depth, tablePV, bound);
            }
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
        History &history = thread.history;

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

        if (position.halfmoveClock() >= 3 && alpha < Score::DRAW && position.upcomingRepetition(rootPly)) {
            alpha = draw;
            if (alpha >= beta) {
                return alpha;
            }
        }

        if (positionDraw(thread)) {
            return draw;
        }

        if (rootPly >= MAX_PLY) {
            return (inCheck) ? 0 : Eval::evaluate(position);
        }

        auto [tableEntry, tableHit] = tTable_.probe(position.hash(), static_cast<Int32>(rootPly));
        const bool tablePV = PV_NODE || (tableHit && tableEntry.pv);
        const Move hashMove = tableEntry.move;

        if constexpr (!PV_NODE) {
            if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= beta) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= alpha))) {
                return tableEntry.score;
            }
        }

        Int32 rawStaticEval = Score::NONE;

        if (inCheck) {
            stack.staticEval = Score::NONE;
            stack.eval = Score::NONE;
        } else {
            rawStaticEval = (tableHit) ? tableEntry.staticEval : Eval::evaluate(position);
            stack.staticEval = history.correct(position, rawStaticEval, rootPly);

            stack.eval = stack.staticEval;
            if (tableHit && ((tableEntry.bound == TTableEntry::Bound::EXACT) || (tableEntry.bound == TTableEntry::Bound::LOWER && tableEntry.score >= stack.eval) || (tableEntry.bound == TTableEntry::Bound::UPPER && tableEntry.score <= stack.eval))) {
                stack.eval = tableEntry.score;
            }
        }

        if (stack.eval >= beta) {
            if (!tableHit) {
                tTable_.write(position.hash(), static_cast<Int32>(rootPly), stack.eval, rawStaticEval, Move::NULL_MOVE, 0, tablePV, TTableEntry::Bound::LOWER);
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

        MoveOrder moveOrder = (inCheck) ? MoveOrder(position, history, hashMove, stack.killerMoves, rootPly) : MoveOrder(position, history, hashMove);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            // TODO: try this:
            // if (!inCheck && movesTried >= 2) {
            //     break;
            // }

            const auto [move, moveScore] = scoredMove;

            // TODO
            // SEE pruning
            // FP

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

            // TODO: try whatever this is
            // if (position.quiet(move) && inCheck && bestScore > Score::LOSS) {
            //     break;
            // }
        }

        if (inCheck && movesTried == 0) {
            return Score::matedIn(static_cast<Int32>(rootPly));
        }

        tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, 0, tablePV, bound);

        return bestScore;
    }

    bool positionDraw(SearchThread &thread) const noexcept {
        bool noMoves = false;
        if (thread.position.halfmoveClock() >= 100) {
            MoveList moves;
            MoveGen::legal(thread.position, moves);
            noMoves = moves.empty();
        }
        return thread.position.draw(thread.rootPly, noMoves);
    }

    void makeMove(SearchThread &thread, Move move, Int32 historyScore) noexcept {
        assert(move != Move::NULL_MOVE);

        HistoryStack &historyStack = thread.history.stack[thread.rootPly];
        historyStack.playedMove = move;
        historyStack.movedPiece = thread.position.moved(move);
        historyStack.contCorrEntry = &thread.history.contCorrEntry(thread.position, move);
        historyStack.contEntry = &thread.history.contEntry(thread.position, move);
        historyStack.score = historyScore;

        thread.position.make(move);
        thread.nodes.store(thread.nodes.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        thread.rootPly++;
    }

    void unmakeMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);

        thread.rootPly--;
        thread.position.unmake();

        HistoryStack &historyStack = thread.history.stack[thread.rootPly];
        historyStack.playedMove = Move::NULL_MOVE;
        historyStack.movedPiece = Piece::NONE;
        historyStack.contCorrEntry = nullptr;
        historyStack.contEntry = nullptr;
        historyStack.score = 0;

    }

    void makeNullMove(SearchThread &thread) noexcept {
        HistoryStack &historyStack = thread.history.stack[thread.rootPly];
        historyStack.contCorrEntry = &thread.history.contCorrEntry(thread.position, Move::NULL_MOVE);

        thread.position.make();
        thread.rootPly++;
    }

    void unmakeNullMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmake();

        HistoryStack &historyStack = thread.history.stack[thread.rootPly];
        historyStack.contCorrEntry = nullptr;
    }

    void printSearchInfo(SearchThread &thread, USize pvIndex, Int32 depth) const noexcept {
        SearchInfo info;
        info.depth = depth;
        info.selDepth = thread.rootMoves[pvIndex].selDepth;
        info.time = timeManager_.elapsed();
        info.nodes = 0;
        for (auto &searchThread : threads_) {
            info.nodes += searchThread->nodes.load(std::memory_order_relaxed);
        }
        info.hashfull = tTable_.hashfull();
        info.score = thread.rootMoves[pvIndex].displayScore;
        info.lowerBound = thread.rootMoves[pvIndex].lowerBound;
        info.upperBound = thread.rootMoves[pvIndex].upperBound;
        info.pvIndex = pvIndex;
        info.pv = thread.rootMoves[pvIndex].pv;

        uciSearchInfo_(info);
    }
};

}
