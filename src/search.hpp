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
#include "tt.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "wdl.hpp"


namespace Sift {

struct SearchStackEntry {
    MoveList pv;

    Move excludedMove;

    Int32 staticEval;
    Int32 eval;

    Int32 failHighCount; // FIXME
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
    Int32 averageScore = Score::NONE;
    Int32 averageSquaredScore = Score::NONE;
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
    std::array<HistoryStackEntry, MAX_PLY + 1> histStack;

    History history;
    SharedHistory *sharedHistory;

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
            stack[i].failHighCount = 0; // FIXME

            histStack[i].move = Move::NULL_MOVE;
            histStack[i].quietMove = false;
            histStack[i].movedPiece = Piece::NONE;
            histStack[i].capturedPiece = Piece::NONE;
            histStack[i].threats = Bitboard();
            histStack[i].pawnHash = 0;
            histStack[i].contHistSubtable = nullptr;
            histStack[i].contCorrHistSubtable = nullptr;
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
    using BestMoveCallback = std::function<void(Move)>;
    using CurrMoveCallback = std::function<void(Move, Int32, Int32)>;

    static constexpr USize MAX_PLY = SearchThread::MAX_PLY;

    Search(SearchInfoCallback uciSearchInfo, BestMoveCallback uciBestMove, CurrMoveCallback uciCurrMove) : tt_(1), contempt_({0, 0}), timeManager_(), uciSearchInfo_(std::move(uciSearchInfo)), uciBestMove_(std::move(uciBestMove)), uciCurrMove_(std::move(uciCurrMove)), printInfo_(true) {
        setTimeUp(false);
        setThreads();
        resizeTT();
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
        tt_.reset(threads_.size());

        for (USize node = 0; node < NUMA::nodeCount(); node++) {
            sharedHistory_.getForNode(node)->reset();
        }
    }

    void run(const Position &position, const SearchLimits &limits) noexcept {
        for (auto &thread : threads_) {
            thread->wait();
        }

        MoveList legalMoves;
        MoveGen::legal(position, legalMoves);
        if (legalMoves.empty() || position.draw(0, legalMoves.empty())) {
            if (printInfo_) {
                uciBestMove_(Move::NULL_MOVE);
            }
            return;
        }

        tt_.incrementAge();

        multiPV_ = static_cast<USize>(OPTIONS["MultiPV"].spinValue());

        const Int32 contemptVal = WDL::unnormalize(static_cast<Int32>(OPTIONS["Contempt"].spinValue()), position.materialScore());
        contempt_[static_cast<USize>(position.sideToMove())] = contemptVal;
        contempt_[static_cast<USize>(~position.sideToMove())] = -contemptVal;

        setTimeUp(false);
        timeManager_.limits(limits, position.sideToMove(), legalMoves.size());
        timeManager_.start();

        for (auto &thread : threads_) {
            thread->reset();
            thread->sharedHistory = sharedHistory_.get(thread->id);
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

        tt_.incrementAge();

        multiPV_ = static_cast<USize>(OptionList::DEFAULT_MULTI_PV);

        contempt_ = {static_cast<Int32>(OptionList::DEFAULT_CONTEMPT), static_cast<Int32>(OptionList::DEFAULT_CONTEMPT)};

        setTimeUp(false);
        timeManager_.limits(limits, position.sideToMove(), legalMoves.size());
        timeManager_.start();

        UInt64 nodes = 0;
        for (auto &thread : threads_) {
            thread->reset();
            thread->sharedHistory = sharedHistory_.get(thread->id);
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

    void resizeTT() noexcept {
        USize sizeMB = (OPTIONS.has("Hash")) ? static_cast<USize>(OPTIONS["Hash"].spinValue()) : OptionList::DEFAULT_HASH_MB;
        tt_.resize(sizeMB, threads_.size());
    }

private:
    static constexpr MS CURR_MOVE_DELAY_INTERVAL = MS(2500);

    std::vector<std::unique_ptr<SearchThread>> threads_;

    TT tt_;
    NUMAUniqueAllocation<SharedHistory> sharedHistory_;

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
                Int32 delta = WINDOW_INIT_DELTA;
                Int32 freduction = 0;

                if (depth >= WINDOW_MIN_DEPTH) {
                    alpha = std::clamp(rootMove.windowScore - delta, Score::MIN, Score::MAX);
                    beta = std::clamp(rootMove.windowScore + delta, Score::MIN, Score::MAX);
                    delta += static_cast<Int32>(static_cast<Int64>(std::abs(rootMove.averageSquaredScore)) * static_cast<Int64>(WINDOW_SQUARED_SCORE_SCALE) / 1048576);
                }

                while (true) {
                    const Int32 fdepth = std::max(depth * FDEPTH_SCALE - freduction, FDEPTH_SCALE);
                    Int32 score = search<true, true>(thread, fdepth, alpha, beta, false);
                    thread.sortRemainingMoves();

                    if (timeUp()) {
                        break;
                    }

                    if (printInfo_ && thread.main() && (score <= alpha || score >= beta) && multiPV_ == 1) {
                        printSearchInfo(thread, thread.pvIndex, depth);
                    }

                    if (score <= alpha) {
                        beta = (alpha + beta) / 2;
                        alpha = std::clamp(score - delta, Score::MIN, Score::MAX);
                        freduction = 0;
                    } else if (score >= beta) {
                        beta = std::clamp(score + delta, Score::MIN, Score::MAX);
                        freduction = std::min(freduction + WINDOW_FREDUCTION, WINDOW_MAX_FREDUCTION);
                    } else {
                        break;
                    }

                    delta += delta * WINDOW_WIDENING_SCALE / 128;
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

    template<bool PV_NODE, bool ROOT_NODE>
    Int32 search(SearchThread &thread, Int32 fdepth, Int32 alpha, Int32 beta, bool cutNode) noexcept {
        static_assert(PV_NODE || !ROOT_NODE);
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);
        assert(!(PV_NODE && cutNode));

        USize &rootPly = thread.rootPly;
        Position &position = thread.position;
        std::span<const HistoryStackEntry> histStack = thread.histStack;
        History &history = thread.history;
        SharedHistory *sharedHistory = thread.sharedHistory;
        SearchStackEntry &curr = thread.stack[rootPly];

        curr.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.loadNodes(), threads_.size())) {
            setTimeUp(true);
            return 0;
        }

        if (timeUp()) {
            return 0;
        }

        if (static_cast<Int32>(rootPly) + 1 > thread.selDepth) {
            thread.selDepth = static_cast<Int32>(rootPly) + 1;
        }

        fdepth = std::min(fdepth, static_cast<Int32>(MAX_PLY - 1) * FDEPTH_SCALE);

        alpha = std::max(alpha, Score::matedIn(static_cast<Int32>(rootPly)));
        beta = std::min(beta, Score::mateIn(static_cast<Int32>(rootPly)));
        if (alpha >= beta) {
            return alpha;
        }

        const bool inCheck = position.inCheck();

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
            return (inCheck) ? 0 : Eval::adjusted(position, thread.nnue, contempt_, sharedHistory->correction(position, histStack, rootPly));
        }

        if (fdepth <= 0) {
            return qsearch<PV_NODE>(thread, alpha, beta);
        }

        SearchStackEntry empty = SearchStackEntry();
        SearchStackEntry &prev = (!ROOT_NODE) ? thread.stack[rootPly - 1] : empty;
        SearchStackEntry &next = thread.stack[rootPly + 1];

        HistoryStackEntry emptyHist = HistoryStackEntry();
        HistoryStackEntry &prevHist = (!ROOT_NODE) ? thread.histStack[rootPly - 1] : emptyHist;

        const bool excludedMove = curr.excludedMove != Move::NULL_MOVE;

        TTEntry ttEntry = TTEntry();
        bool ttHit = false;

        if (!excludedMove) {
            std::tie(ttEntry, ttHit) = tt_.probe(position.hash(), static_cast<Int32>(rootPly));

            if constexpr (!PV_NODE) {
                if (ttHit && ttEntry.fdepth >= fdepth && (ttEntry.score <= alpha || cutNode) && ((ttEntry.bound == TTBound::EXACT) || (ttEntry.bound == TTBound::LOWER && ttEntry.score >= beta) || (ttEntry.bound == TTBound::UPPER && ttEntry.score <= alpha))) {
                    if (ttEntry.score >= beta && ttEntry.move != Move::NULL_MOVE && position.quiet(ttEntry.move) && position.legal(ttEntry.move)) {
                        const Int32 bonus = History::bonus(fdepth, TT_CUTOFF_BONUS_BASE, TT_CUTOFF_BONUS_DEPTH_SCALE, TT_CUTOFF_BONUS_MAX);
                        history.updateQuietHists(position, histStack, ttEntry.move, rootPly, bonus);
                    }

                    if (static_cast<Int32>(position.halfmoveClock()) < TT_CUTOFF_MAX_HALFMOVES) {
                        return ttEntry.score;
                    }
                }
            }
        }

        const Move ttMove = (ROOT_NODE) ? thread.rootMoves[thread.pvIndex].move : ttEntry.move;
        const bool noisyTTMove = ttMove != Move::NULL_MOVE && position.noisy(ttMove);
        const bool ttPV = PV_NODE || (ttHit && ttEntry.pv);

        if (fdepth >= IIR_MIN_FDEPTH && !excludedMove && (PV_NODE || cutNode) && (!ttHit || (ttEntry.move != Move::NULL_MOVE && ttEntry.fdepth + IIR_TT_FDEPTH_MARGIN < fdepth))) {
            fdepth -= IIR_FREDUCTION;
        }

        Int32 rawStaticEval = Score::NONE;
        Int32 complexity = 0;

        if (!excludedMove) {
            if (inCheck) {
                curr.staticEval = Score::NONE;
                curr.eval = Score::NONE;
            } else {
                rawStaticEval = (ttHit && ttEntry.staticEval != Score::NONE) ? ttEntry.staticEval : Eval::raw(position, thread.nnue, contempt_);
                const Int32 correction = sharedHistory->correction(position, histStack, rootPly);
                complexity = std::abs(correction);
                curr.staticEval = Eval::adjust(rawStaticEval, position, correction);
                curr.eval = curr.staticEval;
                if (ttHit && ((ttEntry.bound == TTBound::EXACT) || (ttEntry.bound == TTBound::LOWER && ttEntry.score >= curr.staticEval) || (ttEntry.bound == TTBound::UPPER && ttEntry.score <= curr.staticEval))) {
                    curr.eval = ttEntry.score;
                }

                if (!ttHit) {
                    tt_.write(position.hash(), 0, Score::NONE, rawStaticEval, Move::NULL_MOVE, 0, ttPV, TTBound::NONE);
                }
            }

            if constexpr (!ROOT_NODE) {
                if (!inCheck && prevHist.move != Move::NULL_MOVE && prevHist.quietMove && prev.staticEval != Score::NONE) {
                    const Int32 gain = -prev.staticEval - curr.staticEval;
                    const Int32 bonus = std::clamp(gain * EVAL_POLICY_BONUS_GAIN_SCALE + EVAL_POLICY_BONUS_BASE, EVAL_POLICY_BONUS_MIN, EVAL_POLICY_BONUS_MAX);
                    history.updateMainPawnHists(prevHist, bonus);
                }
            }
        }

        bool improving = [&]() {
            if (inCheck) {
                return false;
            }
            if (rootPly > 1 && thread.stack[rootPly - 2].staticEval != Score::NONE) {
                return curr.staticEval > thread.stack[rootPly - 2].staticEval;
            }
            if (rootPly > 3 && thread.stack[rootPly - 4].staticEval != Score::NONE) {
                return curr.staticEval > thread.stack[rootPly - 4].staticEval;
            }
            return true;
        }();

        if constexpr (!PV_NODE) {
            if (!inCheck && !excludedMove) {
                // TODO: hindsight reduction

                const Int32 rfpMargin = [&] {
                    Int32 margin = 0;
                    margin += RFP_MARGIN_LINEAR_DEPTH_SCALE * fdepth / FDEPTH_SCALE;
                    margin += RFP_MARGIN_QUADRATIC_DEPTH_SCALE * fdepth / FDEPTH_SCALE * fdepth / FDEPTH_SCALE;
                    margin -= RFP_MARGIN_IMPROVING_SCALE * improving;
                    margin += RFP_MARGIN_COMPLEXITY_SCALE * complexity / 1024;
                    return margin;
                }();

                if (fdepth <= RFP_MAX_FDEPTH && curr.eval - rfpMargin >= beta) {
                    return (!Score::decisive(curr.eval) && !Score::decisive(beta)) ? Utils::linInterp<1024>(curr.eval, beta, RFP_FAIL_FIRM_T) : curr.eval;
                }

                const Int32 razoringMargin = RAZORING_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE;
                if (fdepth <= RAZORING_MAX_FDEPTH && std::abs(alpha) < RAZORING_MAX_ABS_ALPHA && curr.eval + razoringMargin <= alpha) {
                    const Int32 score = qsearch<false>(thread, alpha, beta);
                    if (score <= alpha) {
                        return score;
                    }
                }

                const Int32 nmpBetaMargin = [&] {
                    Int32 margin = NMP_MARGIN_BASE;
                    margin -= NMP_MARGIN_DEPTH_SCALE * fdepth / (FDEPTH_SCALE * 128);
                    margin -= NMP_MARGIN_IMPROVING_SCALE * improving;
                    return std::max(margin, 0);
                }();

                if (fdepth >= NMP_MIN_FDEPTH && rootPly >= thread.nmpMinPly && curr.staticEval >= beta + nmpBetaMargin && position.nullPly() > 0 && !(ttEntry.bound == TTBound::UPPER && ttEntry.score < beta) && position.nonPawnMaterial(position.sideToMove())) {
                    tt_.prefetch(position.hashAfter(Move::NULL_MOVE));

                    const Int32 freduction = [&] {
                        Int32 reduction = NMP_FREDUCTION_BASE;
                        reduction += NMP_FREDUCTION_FDEPTH_SCALE * fdepth / 1024;
                        reduction += std::min((curr.staticEval - beta) * NMP_FREDUCTION_EVAL_SCALE, NMP_FREDUCTION_EVAL_MAX);
                        return reduction;
                    }();

                    const Int32 reducedFdepth = fdepth - freduction;

                    makeNullMove(thread);

                    const Int32 score = -search<false, false>(thread, reducedFdepth, -beta, -beta + 1, !cutNode);

                    unmakeNullMove(thread);

                    if (timeUp()) {
                        return 0;
                    }

                    if (score >= beta) {
                        if (fdepth <= NMP_NO_VERIF_MAX_DEPTH || thread.nmpMinPly > 0) {
                            return (Score::win(score)) ? beta : score;
                        }

                        thread.nmpMinPly = rootPly + static_cast<USize>(NMP_MIN_PLY_FDEPTH_SCALE * reducedFdepth / (FDEPTH_SCALE * 128));
                        const Int32 verifScore = search<false, false>(thread, reducedFdepth, beta - 1, beta, true);
                        thread.nmpMinPly = 0;

                        if (timeUp()) {
                            return 0;
                        }

                        if (verifScore >= beta) {
                            return verifScore;
                        }
                    }
                }

                const Int32 probcutBeta = beta + PROBCUT_BETA_OFFSET - improving * PROBCUT_BETA_IMPROVING_SCALE;
                const Int32 probcutFdepth = std::max(fdepth - PROBCUT_FREDUCTION, FDEPTH_SCALE);

                if (fdepth >= PROBCUT_MIN_FDEPTH && !ttPV && !Score::decisive(beta) && (ttMove == Move::NULL_MOVE || noisyTTMove) && !(ttHit && ttEntry.fdepth >= probcutFdepth && ttEntry.score < probcutBeta)) {
                    const Int32 seeMargin = (probcutBeta - curr.staticEval) * PROBCUT_SEE_EVAL_SCALE / 128;

                    MoveOrder moveOrder = MoveOrder::probcut(position, history, histStack, ttMove, rootPly);
                    Move move;
                    while ((move = moveOrder.next()) != Move::NULL_MOVE) {
                        if (!position.see(move, seeMargin)) {
                            continue;
                        }

                        tt_.prefetch(position.hashAfter(move));

                        makeMove(thread, move);

                        Int32 score = -qsearch<false>(thread, -probcutBeta, -probcutBeta + 1);
                        if (score >= probcutBeta) {
                            score = -search<false, false>(thread, probcutFdepth - FDEPTH_SCALE, -probcutBeta, -probcutBeta + 1, !cutNode);
                        }

                        unmakeMove(thread);

                        if (timeUp()) {
                            return 0;
                        }

                        if (score >= probcutBeta) {
                            tt_.write(position.hash(), static_cast<Int32>(rootPly), score, rawStaticEval, move, probcutFdepth, false, TTBound::LOWER);
                            return score;
                        }
                    }
                }
            }
        }

        next.failHighCount = 0; // FIXME

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = Score::NONE;

        TTBound bound = TTBound::UPPER;

        MoveList quietsTried;
        MoveList noisiesTried;

        Int32 movesTried = 0;

        MoveOrder moveOrder = MoveOrder::search(position, history, histStack, ttMove, rootPly);
        Move move;
        while ((move = moveOrder.next()) != Move::NULL_MOVE) {
            if constexpr (ROOT_NODE) {
                if (thread.findRootMove(move) >= thread.rootMoves.size()) {
                    continue;
                }

                if (printInfo_ && thread.main() && timeManager_.elapsed() > CURR_MOVE_DELAY_INTERVAL) {
                    uciCurrMove_(move, movesTried + 1, thread.rootDepth);
                }
            }

            if (move == curr.excludedMove) {
                continue;
            }

            const bool quiet = position.quiet(move);
            const bool quietOrLosing = moveOrder.stage() > MoveOrderStage::GOOD_NOISY;

            const Int32 historyScore = (quiet) ? history.quietScore(position, histStack, move, rootPly) : history.noisyScore(position, move);

            const Int32 baseLMR = LMR_TABLE[quiet][static_cast<USize>(fdepth / FDEPTH_SCALE)][static_cast<USize>(movesTried + 1)];

            if constexpr (!ROOT_NODE) {
                if (!Score::loss(bestScore)) {
                    const Int32 lmrFdepth = [&] {
                        const Int32 freduction = (baseLMR + ttPV * LMR_FDEPTH_PV_SCALE) * FDEPTH_SCALE / 1024;
                        return std::max(fdepth - freduction, 0);
                    }();

                    if (quiet) {
                        const Int32 lmpMargin = LMP_TABLE[improving][static_cast<USize>(fdepth / FDEPTH_SCALE)] + historyScore * LMP_MARGIN_HISTORY_SCALE / 8388608;
                        if (movesTried >= lmpMargin) {
                            moveOrder.skipQuiets();
                            continue;
                        }

                        const Int32 quietHistoryPruningMargin = QUIET_HISTORY_PRUNING_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE + QUIET_HISTORY_PRUNING_MARGIN_OFFSET;
                        if (lmrFdepth <= QUIET_HISTORY_PRUNING_MAX_FDEPTH && historyScore < quietHistoryPruningMargin) {
                            moveOrder.skipQuiets();
                            continue;
                        }

                        const Int32 quietFPMargin = QUIET_FP_MARGIN_BASE + QUIET_FP_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE + historyScore / QUIET_FP_MARGIN_HISTORY_DIVISOR;
                        if (!inCheck && lmrFdepth <= QUIET_FP_MAX_FDEPTH && std::abs(alpha) < QUIET_FP_MAX_ABS_ALPHA && !position.directCheck(move) && curr.staticEval + quietFPMargin <= alpha) {
                            moveOrder.skipQuiets();
                            continue;
                        }
                    } else {
                        const Int32 noisyHistoryPruningMargin = NOISY_HISTORY_PRUNING_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE * fdepth / FDEPTH_SCALE + NOISY_HISTORY_PRUNING_MARGIN_OFFSET;
                        if (lmrFdepth <= NOISY_HISTORY_PRUNING_MAX_FDEPTH && historyScore < noisyHistoryPruningMargin) {
                            continue;
                        }

                        const Int32 noisyFPMargin = NOISY_FP_MARGIN_BASE + NOISY_FP_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE + historyScore / NOISY_FP_MARGIN_HISTORY_DIVISOR;
                        if (!inCheck && lmrFdepth <= NOISY_FP_MAX_FDEPTH && std::abs(alpha) < NOISY_FP_MAX_ABS_ALPHA && curr.staticEval + noisyFPMargin <= alpha) {
                            if (quietOrLosing) {
                                break;
                            } else {
                                continue;
                            }
                        }
                    }

                    const Int32 seePruningMargin = (quiet) ? (QUIET_SEE_PRUNING_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE * fdepth / FDEPTH_SCALE) : (std::min(NOISY_SEE_PRUNING_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE - historyScore / NOISY_SEE_PRUNING_MARGIN_HISTORY_DIVISOR, 0));
                    if (quietOrLosing && !position.see(move, seePruningMargin)) {
                        continue;
                    }
                }
            }

            Int32 fextension = 0;
            if constexpr (!ROOT_NODE) {
                if (move == ttMove && !excludedMove) {
                    if (fdepth >= SE_MIN_FDEPTH_BASE + ttPV * SE_MIN_FDEPTH_PV_SCALE && ttEntry.fdepth + SE_TT_FDEPTH_MARGIN >= fdepth && ttEntry.bound != TTBound::UPPER && !Score::decisive(ttEntry.score)) {
                        const Int32 seBetaMargin = SE_BETA_MARGIN_BASE + SE_BETA_MARGIN_PREV_PV_SCALE * (ttPV && !PV_NODE);
                        const Int32 seBeta = std::max(ttEntry.score - fdepth * seBetaMargin / (FDEPTH_SCALE * 128), Score::MATED_IN_MAX);
                        const Int32 seFdepth = (fdepth - FDEPTH_SCALE) / 2;

                        curr.excludedMove = move;
                        const Int32 score = search<false, false>(thread, seFdepth, seBeta - 1, seBeta, cutNode);
                        curr.excludedMove = Move::NULL_MOVE;

                        if (score < seBeta) {
                            const Int32 doubleFextMargin = [&] {
                                Int32 margin = SE_DOUBLE_FEXT_MARGIN_BASE;
                                margin += SE_DOUBLE_FEXT_MARGIN_PV_SCALE * PV_NODE;
                                margin += SE_DOUBLE_FEXT_MARGIN_NEW_PV_SCALE * (PV_NODE && !(ttHit && ttEntry.pv));
                                margin += SE_DOUBLE_FEXT_MARGIN_COMPLEXITY_SCALE * complexity / 16777216;
                                return margin;
                            }();

                            const Int32 tripleFextMargin = [&] {
                                Int32 margin = SE_TRIPLE_FEXT_MARGIN_BASE;
                                margin += SE_TRIPLE_FEXT_MARGIN_PV_SCALE * PV_NODE;
                                margin += SE_TRIPLE_FEXT_MARGIN_NEW_PV_SCALE * (PV_NODE && !(ttHit && ttEntry.pv));
                                margin += SE_TRIPLE_FEXT_MARGIN_NOISY_SCALE * noisyTTMove;
                                margin += SE_TRIPLE_FEXT_MARGIN_COMPLEXITY_SCALE * complexity / 16777216;
                                return margin;
                            }();

                            fextension = SE_SINGLE_FEXTENSION + (score < seBeta - doubleFextMargin) * SE_DOUBLE_FEXTENSION + (score < seBeta - tripleFextMargin) * SE_TRIPLE_FEXTENSION;
                        } else if (!PV_NODE && score >= beta) {
                            return (!Score::decisive(score)) ? Utils::linInterp<1024>(score, beta, MULTICUT_FAIL_FIRM_T) : score;
                        } else if (ttEntry.score >= beta) {
                            fextension = -SE_NEG_FEXTENSION;
                        } else if (cutNode) {
                            fextension = -SE_CUTNODE_NEG_FEXTENSION;
                        }
                    } else if (fdepth <= LDSE_MAX_FDEPTH && !inCheck && ttEntry.bound == TTBound::LOWER) {
                        fextension = (curr.staticEval <= alpha - LDSE_SINGLE_FEXT_MARGIN) * LDSE_SINGLE_FEXTENSION + (!PV_NODE && !noisyTTMove && ttEntry.fdepth + LDSE_DOUBLE_FEXT_TT_FDEPTH_MARGIN >= fdepth && curr.staticEval <= alpha - LDSE_DOUBLE_FEXT_MARGIN) * LDSE_DOUBLE_FEXTENSION;
                    }
                }
            }

            tt_.prefetch(position.hashAfter(move));

            const UInt64 nodesBefore = thread.loadNodes();

            makeMove(thread, move);
            movesTried++;

            if (quiet) {
                quietsTried.add(move);
            } else {
                noisiesTried.add(move);
            }

            const bool givesCheck = position.givesCheck();

            Int32 newFdepth = fdepth - FDEPTH_SCALE + fextension;
            Int32 score = 0;

            if (fdepth >= LMR_MIN_FDEPTH && movesTried >= (PV_NODE ? LMR_MIN_MOVES_PV : LMR_MIN_MOVES_NON_PV) && !ttPV) {
                Int32 freduction = baseLMR;
                freduction += LMR_NON_IMPROVING_SCALE * !improving;
                freduction += LMR_NOISY_HASH_MOVE_SCALE * noisyTTMove;
                if (ttPV) {
                    freduction -= LMR_TABLE_PV_SCALE + LMR_TABLE_PV_NON_FAIL_LOW_SCALE * (ttHit && ttEntry.score > alpha);
                }
                freduction -= LMR_GIVES_CHECK_SCALE * givesCheck;
                freduction -= LMR_IN_CHECK_SCALE * inCheck;
                freduction -= LMR_HIGH_COMPLEXITY_SCALE * (complexity > HIGH_COMPLEXITY_MARGIN);
                freduction += LMR_CUTNODE_SCALE * cutNode;
                freduction += LMR_FAIL_HIGH_COUNT_SCALE * (next.failHighCount >= LMR_FAIL_HIGH_COUNT_MARGIN); // FIXME
                freduction *= FDEPTH_SCALE;
                freduction /= 1024;

                const Int32 reducedFdepth = std::min(std::max(newFdepth - freduction, FDEPTH_SCALE), newFdepth);
                score = -search<false, false>(thread, reducedFdepth, -alpha - 1, -alpha, true);
                if (score > alpha && reducedFdepth < newFdepth) {
                    if (score > bestScore + DEEPER_SEARCH_MARGIN_BASE + (DEEPER_SEARCH_MARGIN_DEPTH_SCALE * fdepth / FDEPTH_SCALE) / 64) {
                        newFdepth += DEEPER_SEARCH_FEXTENSION;
                    }
                    if (score < bestScore + SHALLOWER_SEARCH_MARGIN) {
                        newFdepth -= SHALLOWER_SEARCH_FREDUCTION;
                    }
                    score = -search<false, false>(thread, newFdepth, -alpha - 1, -alpha, !cutNode);

                    if (quiet && (score <= alpha || score >= beta)) {
                        Int32 bonus = (score >= beta) ? History::bonus(fdepth, HISTORY_BONUS_BASE, HISTORY_BONUS_DEPTH_SCALE, HISTORY_BONUS_MAX) : -History::bonus(fdepth, HISTORY_PENALTY_BASE, HISTORY_PENALTY_DEPTH_SCALE, HISTORY_PENALTY_MAX);
                        history.updateContHist(position, histStack, move, rootPly, bonus);
                    }
                }
            } else if (!PV_NODE || movesTried > 1) {
                score = -search<false, false>(thread, newFdepth, -alpha - 1, -alpha, !cutNode);
            }

            if constexpr (PV_NODE) {
                if (movesTried == 1 || score > alpha) {
                    score = -search<true, false>(thread, newFdepth, -beta, -alpha, false);
                }
            }

            unmakeMove(thread);

            if (timeUp()) {
                return 0;
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

                    if (score <= alpha) {
                        rootMove.displayScore = alpha;
                        rootMove.upperBound = true;
                    } else if (score >= beta) {
                        rootMove.displayScore = beta;
                        rootMove.lowerBound = true;
                    } else {
                        if (rootMove.averageScore == Score::NONE) {
                            rootMove.averageScore = score;
                        } else {
                            rootMove.averageScore = (rootMove.averageScore + score) / 2;
                        }

                        if (rootMove.averageSquaredScore == Score::NONE) {
                            rootMove.averageSquaredScore = score * std::abs(score);
                        } else {
                            rootMove.averageSquaredScore = (rootMove.averageSquaredScore + score * std::abs(score)) / 2;
                        }
                    }

                    rootMove.pv.clear();
                    rootMove.pv.add(move);
                    for (Move pvMove : next.pv) {
                        rootMove.pv.add(pvMove);
                    }
                } else {
                    rootMove.score = Score::NONE;
                }
            }

            if (score > bestScore) {
                bestScore = score;

                if (bestScore > alpha) {
                    alpha = bestScore;
                    bestMove = move;
                    bound = TTBound::EXACT;

                    if constexpr (PV_NODE) {
                        curr.pv.clear();
                        curr.pv.add(move);
                        for (Move pvMove : next.pv) {
                            curr.pv.add(pvMove);
                        }
                    }
                }

                if (bestScore >= beta) {
                    bound = TTBound::LOWER;
                    curr.failHighCount++; // FIXME

                    const Int32 historyFdepth = fdepth + (!inCheck && curr.staticEval <= alpha) * HISTORY_FDEPTH_EVAL_SCALE;

                    if (quiet) {
                        const Int32 quietBonus = History::bonus(historyFdepth, QUIET_BONUS_BASE, QUIET_BONUS_DEPTH_SCALE, QUIET_BONUS_MAX);
                        history.updateQuietHists(position, histStack, move, rootPly, quietBonus);

                        const Int32 quietPenalty = -History::bonus(historyFdepth, QUIET_PENALTY_BASE, QUIET_PENALTY_DEPTH_SCALE, QUIET_PENALTY_MAX);
                        for (const Move quietMove : quietsTried) {
                            if (quietMove != move) {
                                history.updateQuietHists(position, histStack, quietMove, rootPly, quietPenalty);
                            }
                        }

                        const Int32 noisyPenalty = -History::bonus(historyFdepth, QUIET_MOVE_NOISY_PENALTY_BASE, QUIET_MOVE_NOISY_PENALTY_DEPTH_SCALE, QUIET_MOVE_NOISY_PENALTY_MAX);
                        for (const Move noisyMove : noisiesTried) {
                            if (noisyMove != move) {
                                history.updateNoisyHists(position, noisyMove, noisyPenalty);
                            }
                        }
                    } else {
                        const Int32 noisyBonus = History::bonus(historyFdepth, NOISY_BONUS_BASE, NOISY_BONUS_DEPTH_SCALE, NOISY_BONUS_MAX);
                        history.updateNoisyHists(position, move, noisyBonus);

                        const Int32 noisyPenalty = -History::bonus(historyFdepth, NOISY_MOVE_NOISY_PENALTY_BASE, NOISY_MOVE_NOISY_PENALTY_DEPTH_SCALE, NOISY_MOVE_NOISY_PENALTY_MAX);
                        for (const Move noisyMove : noisiesTried) {
                            if (noisyMove != move) {
                                history.updateNoisyHists(position, noisyMove, noisyPenalty);
                            }
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

        // TODO: pcm

        // TODO: fail firm stuff

        if (!excludedMove) {
            // FIXME
            if (!inCheck && (bestMove == Move::NULL_MOVE || position.quiet(bestMove)) && !(bound == TTBound::LOWER && curr.staticEval >= bestScore) && !(bound == TTBound::UPPER && curr.staticEval <= bestScore)) {
                sharedHistory->updateCorrHist(position, histStack, rootPly, fdepth, bestScore, curr.staticEval);
            }

            if (!ROOT_NODE || thread.pvIndex == 0) {
                tt_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, fdepth, ttPV, bound);
            }
        }

        return bestScore;
    }

    template<bool PV_NODE>
    Int32 qsearch(SearchThread &thread, Int32 alpha, Int32 beta) noexcept {
        assert(Score::MIN <= alpha && alpha <= Score::MAX);
        assert(Score::MIN <= beta && beta <= Score::MAX);

        USize &rootPly = thread.rootPly;
        Position &position = thread.position;
        std::span<const HistoryStackEntry> histStack = thread.histStack;
        History &history = thread.history;
        SharedHistory *sharedHistory = thread.sharedHistory;
        SearchStackEntry &curr = thread.stack[rootPly];

        curr.pv.clear();

        if (thread.main() && timeManager_.stopHard(thread.limits, thread.loadNodes(), threads_.size())) {
            setTimeUp(true);
            return 0;
        }

        if (timeUp()) {
            return 0;
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
            return (inCheck) ? 0 : Eval::adjusted(position, thread.nnue, contempt_, sharedHistory->correction(position, histStack, rootPly));
        }

        SearchStackEntry &next = thread.stack[rootPly + 1];

        auto [ttEntry, ttHit] = tt_.probe(position.hash(), static_cast<Int32>(rootPly));
        const bool ttPV = PV_NODE || (ttHit && ttEntry.pv);
        const Move ttMove = ttEntry.move;

        if constexpr (!PV_NODE) {
            if (ttHit && ((ttEntry.bound == TTBound::EXACT) || (ttEntry.bound == TTBound::LOWER && ttEntry.score >= beta) || (ttEntry.bound == TTBound::UPPER && ttEntry.score <= alpha))) {
                return ttEntry.score;
            }
        }

        Int32 rawStaticEval = Score::NONE;

        if (inCheck) {
            curr.staticEval = Score::NONE;
            curr.eval = Score::NONE;
        } else {
            rawStaticEval = (ttHit && ttEntry.staticEval != Score::NONE) ? ttEntry.staticEval : Eval::raw(position, thread.nnue, contempt_);
            curr.staticEval = Eval::adjust(rawStaticEval, position, sharedHistory->correction(position, histStack, rootPly));
            curr.eval = curr.staticEval;
            if (ttHit && ((ttEntry.bound == TTBound::EXACT) || (ttEntry.bound == TTBound::LOWER && ttEntry.score >= curr.staticEval) || (ttEntry.bound == TTBound::UPPER && ttEntry.score <= curr.staticEval))) {
                curr.eval = ttEntry.score;
            }

            if (!ttHit) {
                tt_.write(position.hash(), 0, Score::NONE, rawStaticEval, Move::NULL_MOVE, 0, ttPV, TTBound::NONE);
            }

            if (curr.eval >= beta) {
                return (!Score::decisive(curr.eval) && !Score::decisive(beta)) ? Utils::linInterp<1024>(curr.eval, beta, QSEARCH_FAIL_FIRM_T) : curr.eval;
            }

            if (curr.eval > alpha) {
                alpha = curr.eval;
            }
        }

        const Int32 fpMargin = (inCheck) ? Score::MIN : curr.eval + QSEARCH_FP_MARGIN;

        TTBound bound = TTBound::UPPER;

        Int32 movesTried = 0;

        Move bestMove = Move::NULL_MOVE;
        Int32 bestScore = (inCheck) ? Score::MIN : curr.eval;

        MoveOrder moveOrder = MoveOrder::qsearch(position, history, histStack, ttMove, rootPly, inCheck);
        Move move;
        while ((move = moveOrder.next()) != Move::NULL_MOVE) {
            if (!inCheck && movesTried >= QSEARCH_MAX_MOVES) {
                break;
            }

            if (bestScore > Score::LOSS && !position.see(move, 0)) {
                continue;
            }

            if (!inCheck && fpMargin <= alpha && !position.see(move, 1)) {
                bestScore = std::max(bestScore, fpMargin);
                continue;
            }

            tt_.prefetch(position.hashAfter(move));

            makeMove(thread, move);
            movesTried++;

            const Int32 score = -qsearch<PV_NODE>(thread, -beta, -alpha);

            unmakeMove(thread);

            if (timeUp()) {
                return 0;
            }

            if (score > bestScore) {
                bestScore = score;

                if (bestScore > alpha) {
                    alpha = bestScore;
                    bestMove = move;

                    curr.pv.clear();
                    curr.pv.add(move);
                    for (Move pvMove : next.pv) {
                        curr.pv.add(pvMove);
                    }
                }

                if (bestScore >= beta) {
                    bound = TTBound::LOWER;
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

        tt_.write(position.hash(), static_cast<Int32>(rootPly), bestScore, rawStaticEval, bestMove, 0, ttPV, bound);

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

    void makeMove(SearchThread &thread, Move move) noexcept {
        assert(move != Move::NULL_MOVE);

        HistoryStackEntry &currHist = thread.histStack[thread.rootPly];
        currHist.move = move;
        currHist.quietMove = thread.position.quiet(move);
        currHist.movedPiece = thread.position.movedPiece(move);
        currHist.capturedPiece = thread.position.capturedPiece(move);
        currHist.threats = thread.position.threats();
        currHist.pawnHash = thread.position.pawnHash();
        currHist.contHistSubtable = &thread.history.contHistSubtable(thread.position, move);
        currHist.contCorrHistSubtable = &thread.history.contCorrHistSubtable(thread.position, move);

        thread.position.makeMove(move, thread.nnue.state());
        thread.incNodes();
        thread.rootPly++;
    }

    void unmakeMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmakeMove(thread.nnue.state());

        HistoryStackEntry &currHist = thread.histStack[thread.rootPly];
        currHist.move = Move::NULL_MOVE;
        currHist.quietMove = false;
        currHist.movedPiece = Piece::NONE;
        currHist.capturedPiece = Piece::NONE;
        currHist.threats = Bitboard();
        currHist.pawnHash = 0;
        currHist.contHistSubtable = nullptr;
        currHist.contCorrHistSubtable = nullptr;
    }

    void makeNullMove(SearchThread &thread) noexcept {
        HistoryStackEntry &currHist = thread.histStack[thread.rootPly];
        currHist.move = Move::NULL_MOVE;
        currHist.quietMove = false;
        currHist.movedPiece = Piece::NONE;
        currHist.capturedPiece = Piece::NONE;
        currHist.threats = thread.position.threats();
        currHist.pawnHash = thread.position.pawnHash();
        currHist.contHistSubtable = nullptr;
        currHist.contCorrHistSubtable = &thread.history.contCorrHistSubtable(thread.position, Move::NULL_MOVE);

        thread.position.makeNullMove();
        thread.rootPly++;
    }

    void unmakeNullMove(SearchThread &thread) noexcept {
        assert(thread.rootPly != 0);
        thread.rootPly--;
        thread.position.unmakeMove();

        HistoryStackEntry &currHist = thread.histStack[thread.rootPly];
        currHist.move = Move::NULL_MOVE;
        currHist.quietMove = false;
        currHist.movedPiece = Piece::NONE;
        currHist.capturedPiece = Piece::NONE;
        currHist.threats = Bitboard();
        currHist.pawnHash = 0;
        currHist.contHistSubtable = nullptr;
        currHist.contCorrHistSubtable = nullptr;
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
        info.hashfull = tt_.hashfull();
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
