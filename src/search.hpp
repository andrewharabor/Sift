#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "eval.hpp"
#include "history.hpp"
#include "move-gen.hpp"
#include "move-order.hpp"
#include "move.hpp"
#include "nnue.hpp"
#include "numa.hpp"
#include "option.hpp"
#include "position.hpp"
#include "score.hpp"
#include "time.hpp"
#include "ttable.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "wdl.hpp"


namespace Sift {

struct SearchStackEntry {
    MoveList pv;

    Move excludedMove;

    Int32 staticEval;
    Int32 eval;

    Int32 failHighCount;
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

    USize id;
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

    std::array<SearchStackEntry, MAX_PLY + 1> stack;

    History history;

    NNUE nnue;

    SearchThread(USize id, std::thread &&thread) : id(id), thread(std::move(thread)), flag(ThreadFlag::START), history(), nnue() { reset(); }

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
            stack[i].staticEval = Score::NONE;
            stack[i].eval = Score::NONE;
            stack[i].failHighCount = 0;

            history.stack[i].playedMove = Move::NULL_MOVE;
            history.stack[i].movedPiece = Piece::NONE;
            history.stack[i].contCorrHistEntry = nullptr;
            history.stack[i].contHistEntry = nullptr;
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

    const RootMove &bestMove() const { return rootMoves[0]; }

    UInt64 loadNodes() const noexcept { return nodes.load(std::memory_order_relaxed); }
    void incNodes() noexcept { nodes.store(loadNodes() + 1, std::memory_order_relaxed); }
};

class Search {
public:
    using SearchInfoCallback = std::function<void(const SearchInfo &)>;
    using BestMoveCallback = std::function<void(const Move)>;
    using CurrMoveCallback = std::function<void(const Move, Int32, Int32)>;

    static constexpr USize MAX_PLY = SearchThread::MAX_PLY;

    Search(SearchInfoCallback uciSearchInfo, BestMoveCallback uciBestMove, CurrMoveCallback uciCurrMove) : tTable_(1), contempt_({0, 0}), timeManager_(), uciSearchInfo_(std::move(uciSearchInfo)), uciBestMove_(std::move(uciBestMove)), uciCurrMove_(std::move(uciCurrMove)), printInfo_(true) {
        setTimeUp(false);
        setThreads();
        resizeTTable();
    }

    ~Search() noexcept {
        if (running()) {
            stop();
        }
        joinThreads();
    }

    void setThreads() noexcept {
        USize count = (OPTIONS.has("Threads")) ? static_cast<USize>(OPTIONS["Threads"].spinValue()) : OptionList::DEFAULT_THREADS;
        if (threads_.size() != count) {
            joinThreads();
            threads_.clear();
            threads_.reserve(count);
            for (USize i = 0; i < count; i++) {
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
            if (printInfo_) {
                uciBestMove_(Move::NULL_MOVE);
            }
            return;
        }

        tTable_.incrementAge();

        multiPV_ = static_cast<USize>(OPTIONS["MultiPV"].spinValue());

        const Int32 contemptVal = WDL::unnormalize(static_cast<Int32>(OPTIONS["Contempt"].spinValue()), position.materialScore());
        contempt_[static_cast<USize>(position.sideToMove())] = contemptVal;
        contempt_[static_cast<USize>(~position.sideToMove())] = -contemptVal;

        setTimeUp(false);

        timeManager_.limits(limits, position.sideToMove(), legalMoves.size());
        timeManager_.start();

        for (auto &thread : threads_) {
            thread->reset();
            thread->position = position;
            thread->nnue.state().set(position);
            thread->limits = limits;
            thread->initMoves();
            thread->start();
        }
    }


    UInt64 bench(const Position &position, const SearchLimits &limits) noexcept {
        printInfo_ = false;

        for (auto &thread : threads_) {
            thread->wait();
        }

        MoveList legalMoves;
        MoveGen::legal(position, legalMoves);

        tTable_.reset(threads_.size());
        tTable_.incrementAge();

        multiPV_ = static_cast<USize>(OptionList::DEFAULT_MULTI_PV);

        contempt_ = {static_cast<Int32>(OptionList::DEFAULT_CONTEMPT), static_cast<Int32>(OptionList::DEFAULT_CONTEMPT)};

        setTimeUp(false);
        timeManager_.limits(limits, position.sideToMove(), legalMoves.size());
        timeManager_.start();

        UInt64 nodes = 0;
        for (auto &thread : threads_) {
            thread->reset();
            thread->history.reset();
            thread->position = position;
            thread->nnue.state().set(position);
            thread->limits = limits;
            thread->initMoves();
            thread->start();
            thread->wait();
            nodes += thread->loadNodes();
        }

        printInfo_ = true;

        return nodes;
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
        setTimeUp(true);
        for (auto &thread : threads_) {
            thread->wait();
        }
    }

    void resizeTTable() noexcept {
        USize sizeMB = (OPTIONS.has("Hash")) ? static_cast<USize>(OPTIONS["Hash"].spinValue()) : OptionList::DEFAULT_HASH_MB;
        tTable_.resize(sizeMB, threads_.size());
    }

private:
    static constexpr MS CURR_MOVE_UPDATE_INTERVAL = MS(2500);

    static constexpr Int32 WINDOW_MIN_DEPTH = 6;
    static constexpr Int32 WINDOW_MAX_DEPTH_REDUCTION = 5;

    static constexpr Int32 RFP_MAX_DEPTH = 8;

    static constexpr Int32 RAZORING_MAX_DEPTH = 3;

    static constexpr Int32 NMP_MIN_DEPTH = 2;
    static constexpr Int32 NMP_NO_VERIFICATION_MAX_DEPTH = 15;
    static constexpr Int32 NMP_MIN_PLY_DEPTH_SCALE = 3;
    static constexpr Int32 NMP_MIN_PLY_DEPTH_DIVISOR = 4;

    static constexpr Int32 PROBCUT_MIN_DEPTH = 5;
    static constexpr Int32 PROBCUT_TTABLE_DEPTH_MARGIN = 3;
    static constexpr Int32 PROBCUT_REDUCTION = 4;

    static constexpr Int32 IIR_MIN_DEPTH = 4;
    static constexpr Int32 IIR_TTABLE_DEPTH_MARGIN = 5;

    static constexpr Int32 LMR_MIN_DEPTH = 3;
    static constexpr Int32 LMR_MIN_MOVES_PV = 4;
    static constexpr Int32 LMR_MIN_MOVES_NON_PV = 3;

    static constexpr Int32 FP_MAX_DEPTH = 8;

    static constexpr Int32 NOISY_FP_MAX_DEPTH = 5;

    static constexpr Int32 HISTORY_PRUNING_MAX_DEPTH = 7;

    static constexpr Int32 SE_ROOT_DEPTH_SCALE = 2;
    static constexpr Int32 SE_MIN_DEPTH = 5;
    static constexpr Int32 SE_TABLE_DEPTH_MARGIN = 3;

    static constexpr Int32 QSEARCH_MAX_MOVES = 2;

    std::vector<std::unique_ptr<SearchThread>> threads_;

    TTable tTable_;

    USize multiPV_;

    std::array<Int32, 2> contempt_;

    TimeManager timeManager_;
    std::atomic_bool timeUp_;

    SearchInfoCallback uciSearchInfo_;
    BestMoveCallback uciBestMove_;
    CurrMoveCallback uciCurrMove_;

    bool printInfo_;

    void joinThreads() noexcept {
        for (auto &thread : threads_) {
            thread->join();
        }
    }

    void threadLoop(SearchThread &thread) noexcept {
        NUMA::bindThread(thread.id);

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

                    if (timeUp()) {
                        break;
                    }

                    if (printInfo_ && thread.main() && (score <= alpha || score >= beta) && multiPV_ == 1) {
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

                if (timeUp()) {
                    break;
                }
            }

            if (printInfo_ && thread.main()) {
                for (USize idx = 0; idx < std::min(multiPV_, thread.rootMoves.size()); idx++) {
                    printSearchInfo(thread, idx, depth);
                }
            }

            if (thread.main() && timeManager_.stopSoft(thread.limits, depth, thread.bestMove().move, thread.bestMove().score, thread.bestMove().nodes, thread.loadNodes())) {
                setTimeUp(true);
            }

            if (timeUp()) {
                break;
            }
        }

        if (thread.main()) {
            if (thread.limits.infinite) {
                while (!timeUp()) {
                    std::this_thread::yield();
                }
            }

            setTimeUp(true);

            for (auto &otherThread : threads_) {
                if (!otherThread->main()) {
                    otherThread->wait();
                }
            }

            if (printInfo_) {
                const SearchThread &bestThread = selectThread();

                if (!bestThread.main()) {
                    printSearchInfo(bestThread, 0, bestThread.rootDepth);
                }

                uciBestMove_(bestThread.bestMove().move);
            }
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
        SearchStackEntry &stack = thread.stack[rootPly];
        History &history = thread.history;

        stack.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.loadNodes(), threads_.size())) {
            setTimeUp(true);
            return alpha;
        }

        if (timeUp()) {
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

        const Int32 drawScore = Score::drawScore(thread.loadNodes());

        if constexpr (!ROOT_NODE) {
            if (position.halfmoveClock() >= 3 && alpha < drawScore && position.upcomingRepetition(rootPly)) {
                alpha = drawScore;
                if (alpha >= beta) {
                    return alpha;
                }
            }
        }

        if (positionDraw(thread)) {
            return drawScore;
        }

        if (rootPly >= MAX_PLY) {
            return (inCheck) ? 0 : Eval::adjusted(position, thread.nnue, contempt_);
        }

        SearchStackEntry &nextStack = thread.stack[rootPly + 1];

        if (depth <= 0) {
            return quiescenceSearch<PV_NODE>(thread, alpha, beta);
        }

        TTableEntry tTableEntry = TTableEntry();
        bool tTableHit = false;

        Int32 rawStaticEval = Score::NONE;
        Int32 complexity = 0;

        if (!excludedMove) {
            std::tie(tTableEntry, tTableHit) = tTable_.probe(position.hash(), static_cast<Int32>(rootPly));

            if constexpr (!PV_NODE) {
                if (tTableHit && tTableEntry.depth >= depth && ((tTableEntry.bound == TTableEntry::Bound::EXACT) || (tTableEntry.bound == TTableEntry::Bound::LOWER && tTableEntry.score >= beta) || (tTableEntry.bound == TTableEntry::Bound::UPPER && tTableEntry.score <= alpha))) {
                    return tTableEntry.score;
                }
            }

            if (inCheck) {
                stack.staticEval = Score::NONE;
                stack.eval = Score::NONE;
            } else {
                rawStaticEval = (tTableHit) ? tTableEntry.staticEval : Eval::raw(position, thread.nnue, contempt_);
                stack.staticEval = history.correctStaticEval(position, Eval::adjust(rawStaticEval, position), rootPly);
                complexity = std::abs(stack.staticEval - rawStaticEval);
                stack.eval = stack.staticEval;
                if (tTableHit && ((tTableEntry.bound == TTableEntry::Bound::EXACT) || (tTableEntry.bound == TTableEntry::Bound::LOWER && tTableEntry.score >= stack.eval) || (tTableEntry.bound == TTableEntry::Bound::UPPER && tTableEntry.score <= stack.eval))) {
                    stack.eval = tTableEntry.score;
                }
            }
        }

        const Move tTableMove = ROOT_NODE ? thread.rootMoves[thread.pvIndex].move : tTableEntry.move;
        const bool noisyTTableMove = tTableMove != Move::NULL_MOVE && position.noisy(tTableMove);
        const bool tTablePV = PV_NODE || (tTableHit && tTableEntry.pv);

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
                const Int32 rfpMargin = ((improving) ? (RFP_IMPROVING_MARGIN + RFP_WINNING_THREATS * winningThreats) : RFP_NON_IMPROVING_MARGIN) * depth - (RFP_OPPONENT_WORSENING * opponentWorsening) + (thread.history.stack[rootPly - 1].score / RFP_HISTORY_DIVISOR);
                if (depth <= RFP_MAX_DEPTH && std::abs(stack.eval) < Score::KNOWN_WIN && stack.eval >= std::max(rfpMargin, RFP_MIN_MARGIN) + beta) {
                    return stack.eval;
                }


                if (depth <= RAZORING_MAX_DEPTH && stack.eval <= alpha - RAZORING_MARGIN * depth && alpha < RAZORING_MAX_ALPHA) {
                    const Int32 score = quiescenceSearch<PV_NODE>(thread, alpha, beta);
                    if (score <= alpha) {
                        return score;
                    }
                }

                if (position.nullPly() > 0 && rootPly >= thread.nmpMinPly && depth >= NMP_MIN_DEPTH && stack.eval >= beta + NMP_EVAL_MARGIN && stack.staticEval >= beta + NMP_STATIC_EVAL_BASE_MARGIN - NMP_STATIC_EVAL_DEPTH_MARGIN * depth && position.nonPawnMaterial(position.sideToMove())) {
                    const Int32 reduction = (NMP_BASE_REDUCTION + NMP_DEPTH_REDUCTION_SCALE * depth) / NMP_REDUCTION_DIVISOR + std::min((stack.eval - beta) / NMP_EVAL_REDUCTION_SCALE, NMP_MAX_EVAL_REDUCTION);

                    makeNullMove(thread);
                    const Int32 nullMoveScore = -search<false, false>(thread, depth - reduction, -beta, -beta + 1, !cutNode);
                    unmakeNullMove(thread);

                    if (nullMoveScore >= beta) {
                        if ((depth <= NMP_NO_VERIFICATION_MAX_DEPTH && std::abs(beta) < Score::KNOWN_WIN) || thread.nmpMinPly > 0) {
                            return Score::mate(nullMoveScore) ? beta : nullMoveScore;
                        }

                        thread.nmpMinPly = rootPly + static_cast<USize>((depth - reduction) * NMP_MIN_PLY_DEPTH_SCALE / NMP_MIN_PLY_DEPTH_DIVISOR);
                        const Int32 verificationScore = search<false, false>(thread, depth - reduction, beta - 1, beta, true);
                        thread.nmpMinPly = 0;

                        if (verificationScore >= beta) {
                            return verificationScore;
                        }
                    }
                }


                Int32 probcutBeta = beta + PROBCUT_BETA_MARGIN;
                if (depth >= PROBCUT_MIN_DEPTH && !Score::mate(beta) && (!tTableHit || tTableEntry.score >= probcutBeta || tTableEntry.depth + PROBCUT_TTABLE_DEPTH_MARGIN < depth)) {
                    MoveOrder moveOrder = MoveOrder(position, history, tTableMove);
                    ScoredMove scoredMove;
                    Int32 seeMargin = probcutBeta - stack.staticEval;
                    Int32 probcutDepth = depth - PROBCUT_REDUCTION;
                    while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
                        auto [move, moveScore] = scoredMove;

                        if (!position.see(move, seeMargin)) {
                            continue;
                        }

                        tTable_.prefetch(position.hashAfter(move));

                        makeMove(thread, move, history.noisyScore(position, move));

                        Int32 score = -quiescenceSearch<false>(thread, -probcutBeta, -probcutBeta + 1);
                        if (score >= probcutBeta && probcutDepth >= 0) {
                            score = -search<false, false>(thread, probcutDepth, -probcutBeta, -probcutBeta + 1, !cutNode);
                        }

                        unmakeMove(thread);

                        if (timeUp()) {
                            return alpha;
                        }

                        if (score >= probcutBeta) {
                            tTable_.write(position.hash(), static_cast<Int32>(rootPly), score, rawStaticEval, move, probcutDepth + 1, tTablePV, TTableEntry::Bound::LOWER);
                            return score;
                        }
                    }
                }
            }
        }

        if (depth >= IIR_MIN_DEPTH && !inCheck && !excludedMove && (!tTableHit || (tTableEntry.move != Move::NULL_MOVE && tTableEntry.depth <= depth - IIR_TTABLE_DEPTH_MARGIN))) {
            depth--;
        }

        nextStack.failHighCount = 0;

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        MoveList quietsTried;
        MoveList noisiesTried;
        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::MIN;

        MoveOrder moveOrder = MoveOrder(position, history, tTableMove, rootPly);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            const auto [move, moveScore] = scoredMove;

            if constexpr (ROOT_NODE) {
                if (thread.findRootMove(move) >= thread.rootMoves.size()) {
                    continue;
                }

                if (printInfo_ && thread.main() && timeManager_.elapsed() > CURR_MOVE_UPDATE_INTERVAL) {
                    uciCurrMove_(move, movesTried + 1, thread.rootDepth);
                }
            }

            if (move == stack.excludedMove) {
                continue;
            }

            const bool quiet = position.quiet(move);
            const Int32 historyScore = (quiet) ? history.quietScore(position, move, rootPly) : history.noisyScore(position, move);
            const Int32 baseLMR = LMR_TABLE[std::min(static_cast<USize>(depth), LMR_TABLE_SIZE_DEPTH - 1)][std::min(static_cast<USize>(movesTried), LMR_TABLE_SIZE_MOVES - 1)] - (LMR_HISTORY_SCALE * historyScore / (quiet ? LMR_QUIET_HISTORY_DIVISOR : LMR_NOISY_HISTORY_DIVISOR));

            if constexpr (!ROOT_NODE) {
                if (moveScore <= MoveScore::BAD_NOISY && bestScore > Score::LOSS) {
                    const Int32 lmrDepth = std::max(depth - baseLMR / LMR_BASE_DIVISOR, 0);
                    const Int32 fpMargin = std::max(FP_BASE_MARGIN + FP_DEPTH_SCALE * lmrDepth + historyScore / FP_HISTORY_DIVISOR, FP_MARGIN_MIN);
                    if (lmrDepth <= FP_MAX_DEPTH && quiet && !inCheck && alpha < Score::WIN && stack.staticEval + fpMargin <= alpha) {
                        continue;
                    }

                    const Int32 noisyFPMargin = std::max(NOISY_FP_BASE_MARGIN + NOISY_FP_DEPTH_SCALE * depth + historyScore / NOISY_FP_HISTORY_DIVISOR, NOISY_FP_MARGIN_MIN);
                    if (depth <= NOISY_FP_MAX_DEPTH && !quiet && !inCheck && alpha < Score::WIN && stack.staticEval + noisyFPMargin <= alpha) {
                        break;
                    }

                    const Int32 lmpMargin = ((improving || complexity > HIGH_COMPLEXITY_MARGIN) ? (LMP_MARGIN_IMPROVING_BASE + LMP_MARGIN_IMPROVING_DEPTH_SCALE * depth * depth) : (LMP_MARGIN_NON_IMPROVING_BASE + LMP_MARGIN_NON_IMPROVING_DEPTH_SCALE * depth * depth)) / LMP_MARGIN_DIVISOR;
                    if (!inCheck && movesTried >= lmpMargin) {
                        break;
                    }

                    const Int32 seeCaptHistoryMax = SEE_CAPT_HISTORY_DEPTH_SCALE * depth;
                    const Int32 seeMargin = quiet ? (SEE_PRUNING_MARGIN_QUIET * depth) : (SEE_PRUNING_MARGIN_NOISY * depth - std::clamp(historyScore / SEE_CAPT_HISTORY_DIVISOR, -seeCaptHistoryMax, seeCaptHistoryMax));
                    if (!position.see(move, seeMargin)) {
                        continue;
                    }

                    if (quiet && depth <= HISTORY_PRUNING_MAX_DEPTH && historyScore < HISTORY_PRUNING_MARGIN * depth) {
                        break;
                    }
                }
            }

            const bool doSE = !ROOT_NODE && rootPly < static_cast<USize>(SE_ROOT_DEPTH_SCALE * thread.rootDepth) && !excludedMove && depth >= SE_MIN_DEPTH + tTablePV && tTableMove == move && tTableEntry.depth >= depth - SE_TABLE_DEPTH_MARGIN && tTableEntry.bound != TTableEntry::Bound::UPPER && std::abs(tTableEntry.score) < Score::KNOWN_WIN;
            Int32 extension = 0;

            if (doSE) {
                const Int32 seBeta = std::max(Score::MATED, tTableEntry.score - (SE_BETA_SCALE + SE_BETA_SCALE_PV * (tTablePV && !PV_NODE)) * depth / SE_BETA_DEPTH_DIVISOR);
                const Int32 seDepth = (depth - 1) / 2;

                stack.excludedMove = move;
                const Int32 score = search<false, false>(thread, seDepth, seBeta - 1, seBeta, cutNode);
                stack.excludedMove = Move::NULL_MOVE;

                if (score < seBeta) {
                    extension = (!PV_NODE && score < seBeta - SE_DOUBLE_EXT_MARGIN) ? (2 + (quiet && score < seBeta - SE_TRIPLE_EXT_MARGIN)) : 1;
                } else if (seBeta >= beta) {
                    return seBeta;
                } else if (tTableEntry.score >= beta) {
                    extension = -2 + PV_NODE;
                } else if (tTableEntry.score <= alpha && cutNode) {
                    extension = -1;
                }
            }

            tTable_.prefetch(position.hashAfter(move));

            const UInt64 nodesBefore = thread.loadNodes();

            makeMove(thread, move, historyScore);
            movesTried++;

            if (quiet) {
                quietsTried.add(move);
            } else {
                noisiesTried.add(move);
            }

            const bool givesCheck = position.givesCheck();
            if (!doSE && givesCheck) {
                extension = 1;
            }

            Int32 newDepth = depth - 1 + extension;
            Int32 score = 0;

            if (depth >= LMR_MIN_DEPTH && movesTried >= (PV_NODE ? LMR_MIN_MOVES_PV : LMR_MIN_MOVES_NON_PV) && (!tTablePV || moveScore <= MoveScore::BAD_NOISY)) {
                Int32 reduction = baseLMR;
                reduction += LMR_NON_IMPROVING_SCALE * !improving;
                reduction += LMR_NOISY_HASH_MOVE_SCALE * noisyTTableMove;
                if (tTablePV) {
                    reduction -= LMR_TABLE_PV_SCALE + LMR_TABLE_PV_NON_FAIL_LOW_SCALE * (tTableHit && tTableEntry.score > alpha);
                }
                reduction -= LMR_GIVES_CHECK_SCALE * givesCheck;
                reduction -= LMR_IN_CHECK_SCALE * inCheck;
                reduction -= LMR_HIGH_COMPLEXITY_SCALE * (complexity > HIGH_COMPLEXITY_MARGIN);
                reduction += LMR_CUTNODE_SCALE * cutNode;
                reduction += LMR_FAIL_HIGH_COUNT_SCALE * (nextStack.failHighCount >= LMR_FAIL_HIGH_COUNT_MARGIN);

                const Int32 reducedDepth = std::min(std::max(newDepth - reduction / LMR_REDUCTION_DIVISOR, 1), newDepth);
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
                        history.updateContHist(position, move, rootPly, bonus);
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

            if (timeUp()) {
                return alpha;
            }

            if constexpr (ROOT_NODE) {
                const USize rootMoveIndex = thread.findRootMove(move);
                if (rootMoveIndex >= thread.rootMoves.size()) {
                    continue;
                }

                RootMove &rootMove = thread.rootMoves[rootMoveIndex];
                rootMove.windowScore = rootMove.score;
                rootMove.nodes += thread.loadNodes() - nodesBefore;

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

                    Int32 historyDepth = depth + ((bestScore > beta) + HISTORY_BETA_MARGIN);
                    Int32 bonus = history.bonus(historyDepth);
                    Int32 penalty = history.penalty(historyDepth);

                    if (quiet) {
                        history.updateQuietHists(position, move, rootPly, bonus);
                        for (const Move quietMove : quietsTried) {
                            if (quietMove != move) {
                                history.updateQuietHists(position, quietMove, rootPly, penalty);
                            }
                        }
                    } else {
                        history.updateNoisyHists(position, move, bonus);
                    }

                    for (const Move noisyMove : noisiesTried) {
                        if (noisyMove != move) {
                            history.updateNoisyHists(position, noisyMove, penalty);
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

            return (inCheck) ? Score::matedIn(static_cast<Int32>(rootPly)) : Score::STALEMATE;
        }

        if (!excludedMove) {
            if (!inCheck && (bestMove == Move::NULL_MOVE || position.quiet(bestMove)) && !(bound == TTableEntry::Bound::LOWER && stack.staticEval >= bestScore) && !(bound == TTableEntry::Bound::UPPER && stack.staticEval <= bestScore)) {
                history.updateCorrHist(position, depth, rootPly, bestScore - stack.staticEval);
            }

            if (!ROOT_NODE || thread.pvIndex == 0) {
                tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, depth, tTablePV, bound);
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
        SearchStackEntry &stack = thread.stack[rootPly];
        History &history = thread.history;

        stack.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.loadNodes(), threads_.size())) {
            setTimeUp(true);
            return alpha;
        }

        if (timeUp()) {
            return alpha;
        }

        if (static_cast<Int32>(rootPly) + 1 > thread.selDepth) {
            thread.selDepth = static_cast<Int32>(rootPly) + 1;
        }

        const bool inCheck = position.inCheck();

        const Int32 drawScore = Score::drawScore(thread.loadNodes());

        if (position.halfmoveClock() >= 3 && alpha < drawScore && position.upcomingRepetition(rootPly)) {
            alpha = drawScore;
            if (alpha >= beta) {
                return alpha;
            }
        }

        if (positionDraw(thread)) {
            return drawScore;
        }

        if (rootPly >= MAX_PLY) {
            return (inCheck) ? 0 : Eval::adjusted(position, thread.nnue, contempt_);
        }

        auto [tTableEntry, tTableHit] = tTable_.probe(position.hash(), static_cast<Int32>(rootPly));
        const bool tTablePV = PV_NODE || (tTableHit && tTableEntry.pv);
        const Move tTableMove = tTableEntry.move;

        if constexpr (!PV_NODE) {
            if (tTableHit && ((tTableEntry.bound == TTableEntry::Bound::EXACT) || (tTableEntry.bound == TTableEntry::Bound::LOWER && tTableEntry.score >= beta) || (tTableEntry.bound == TTableEntry::Bound::UPPER && tTableEntry.score <= alpha))) {
                return tTableEntry.score;
            }
        }

        Int32 rawStaticEval = Score::NONE;

        if (inCheck) {
            stack.staticEval = Score::NONE;
            stack.eval = Score::NONE;
        } else {
            rawStaticEval = (tTableHit) ? tTableEntry.staticEval : Eval::raw(position, thread.nnue, contempt_);
            stack.staticEval = history.correctStaticEval(position, Eval::adjust(rawStaticEval, position), rootPly);
            stack.eval = stack.staticEval;
            if (tTableHit && ((tTableEntry.bound == TTableEntry::Bound::EXACT) || (tTableEntry.bound == TTableEntry::Bound::LOWER && tTableEntry.score >= stack.eval) || (tTableEntry.bound == TTableEntry::Bound::UPPER && tTableEntry.score <= stack.eval))) {
                stack.eval = tTableEntry.score;
            }
        }

        if (stack.eval >= beta) {
            if (!tTableHit) {
                tTable_.write(position.hash(), static_cast<Int32>(rootPly), stack.eval, rawStaticEval, Move::NULL_MOVE, 0, tTablePV, TTableEntry::Bound::LOWER);
            }
            return stack.eval;
        }

        if (stack.eval > alpha) {
            alpha = stack.eval;
        }

        SearchStackEntry &nextStack = thread.stack[rootPly + 1];

        const Int32 fpMargin = (inCheck) ? Score::MIN : stack.eval + QSEARCH_FP_MARGIN;

        TTableEntry::Bound bound = TTableEntry::Bound::UPPER;

        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = (inCheck) ? Score::MIN : stack.eval;

        MoveOrder moveOrder = (inCheck) ? MoveOrder(position, history, tTableMove, rootPly) : MoveOrder(position, history, tTableMove);

        ScoredMove scoredMove;
        while ((scoredMove = moveOrder.next()).score != MoveScore::NONE) {
            if (!inCheck && movesTried >= QSEARCH_MAX_MOVES) {
                break;
            }

            const auto [move, moveScore] = scoredMove;

            if (bestScore > Score::LOSS && !position.see(move, 0)) {
                continue;
            }

            if (!inCheck && fpMargin <= alpha && !position.see(move, 1)) {
                bestScore = std::max(bestScore, fpMargin);
                continue;
            }

            tTable_.prefetch(position.hashAfter(move));

            makeMove(thread, move, 0);
            movesTried++;

            const Int32 score = -quiescenceSearch<PV_NODE>(thread, -beta, -alpha);

            unmakeMove(thread);

            if (timeUp()) {
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

            if (position.quiet(move) && inCheck && bestScore > Score::LOSS) {
                break;
            }
        }

        if (inCheck && movesTried == 0) {
            return Score::matedIn(static_cast<Int32>(rootPly));
        }

        tTable_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, 0, tTablePV, bound);

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

        HistoryStackEntry &historyStack = thread.history.stack[thread.rootPly];
        historyStack.playedMove = move;
        historyStack.movedPiece = thread.position.moved(move);
        historyStack.contCorrHistEntry = &thread.history.contCorrHistEntry(thread.position, move);
        historyStack.contHistEntry = &thread.history.contHistEntry(thread.position, move);
        historyStack.score = historyScore;

        thread.position.makeMove(move, thread.nnue.state());
        thread.incNodes();
        thread.rootPly++;
    }

    void unmakeMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmakeMove(thread.nnue.state());

        HistoryStackEntry &historyStack = thread.history.stack[thread.rootPly];
        historyStack.playedMove = Move::NULL_MOVE;
        historyStack.movedPiece = Piece::NONE;
        historyStack.contCorrHistEntry = nullptr;
        historyStack.contHistEntry = nullptr;
        historyStack.score = 0;

    }

    void makeNullMove(SearchThread &thread) noexcept {
        HistoryStackEntry &historyStack = thread.history.stack[thread.rootPly];
        historyStack.contCorrHistEntry = &thread.history.contCorrHistEntry(thread.position, Move::NULL_MOVE);

        thread.position.makeNullMove();
        thread.rootPly++;
    }

    void unmakeNullMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmakeMove();

        HistoryStackEntry &historyStack = thread.history.stack[thread.rootPly];
        historyStack.contCorrHistEntry = nullptr;
    }

    const SearchThread &selectThread() const noexcept {
        if (threads_.size() == 1) {
            return *threads_[0];
        }

        Int32 lowestRootScore = Score::MAX;
        for (const auto &thread : threads_) {
            const Int32 score = thread->bestMove().score;
            if (score != Score::NONE && score < lowestRootScore) {
                lowestRootScore = score;
            }
        }

        const auto threadWeight = [&](const SearchThread &thread) { return (thread.bestMove().score - lowestRootScore + THREAD_WEIGHT_SCORE_OFFSET) * thread.rootDepth; };

        std::unordered_map<UInt16, Int32> moveVotes = {};

        const auto bestMoveVotes = [&](const SearchThread &thread) -> Int32 & { return moveVotes[thread.bestMove().move.internal()]; };

        for (const auto &thread : threads_) {
            if (thread->bestMove().score != Score::NONE) {
                bestMoveVotes(*thread) += threadWeight(*thread);
            }
        }

        const auto *bestThread = threads_[0].get();
        Int32 bestScore = bestThread->bestMove().score;
        Int32 bestVotes = bestMoveVotes(*bestThread);

        const auto updateBestThread = [&](const SearchThread *thread) {
            bestThread = thread;
            bestScore = thread->bestMove().score;
            bestVotes = bestMoveVotes(*thread);
        };

        for (USize i = 1; i < threads_.size(); i++) {
            const auto *thread = threads_[i].get();
            const Int32 score = thread->bestMove().score;

            if (score == Score::NONE) {
                continue;
            }

            if (bestScore == Score::NONE) {
                updateBestThread(thread);
                continue;
            }

            if (Score::win(bestScore)) {
                if (score > bestScore) {
                    updateBestThread(thread);
                }
                continue;
            }

            if (Score::loss(bestScore)) {
                if (Score::loss(score) && score < bestScore) {
                    updateBestThread(thread);
                }
                continue;
            }

            if (Score::decisive(score)) {
                updateBestThread(thread);
                continue;
            }

            const Int32 votes = bestMoveVotes(*thread);
            if (votes > bestVotes) {
                updateBestThread(thread);
                continue;
            }

            if (votes == bestVotes && (threadWeight(*thread) * (thread->bestMove().pv.size() > 2) > threadWeight(*bestThread) * (bestThread->bestMove().pv.size() > 2))) {
                updateBestThread(thread);
                continue;
            }
        }

        return *bestThread;
    }

    void printSearchInfo(const SearchThread &thread, USize pvIndex, Int32 depth) const noexcept {
        SearchInfo info;
        info.depth = depth;
        info.selDepth = thread.rootMoves[pvIndex].selDepth;
        info.time = timeManager_.elapsed();
        info.nodes = 0;
        for (auto &searchThread : threads_) {
            info.nodes += searchThread->loadNodes();
        }
        info.hashfull = tTable_.hashfull();
        info.score = thread.rootMoves[pvIndex].displayScore;
        info.lowerBound = thread.rootMoves[pvIndex].lowerBound;
        info.upperBound = thread.rootMoves[pvIndex].upperBound;
        info.pvIndex = pvIndex;
        info.pv = thread.rootMoves[pvIndex].pv;

        uciSearchInfo_(info);
    }

    bool timeUp() const noexcept { return timeUp_.load(std::memory_order_relaxed); }
    void setTimeUp(bool value) noexcept { timeUp_.store(value, std::memory_order_relaxed); }
};

}
