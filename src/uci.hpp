#pragma once

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "bench.hpp"
#include "color.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "move-gen.hpp"
#include "nnue.hpp"
#include "option.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "search.hpp"
#include "time.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "wdl.hpp"


namespace Sift {

class UCI {
public:
    UCI() : position_(), legalMoves_(), search_([this](const SearchInfo &info) { searchInfo(info); }, [this](Move move) { bestMove(move); }, [this](Move move, Int32 moveNum, Int32 depth) { currMove(move, moveNum, depth); }) {
        OPTIONS.add(Option("Hash", SpinOption(OptionList::DEFAULT_HASH_MB, OptionList::DEFAULT_HASH_MB, OptionList::MIN_HASH_MB, OptionList::MAX_HASH_MB), [this](const Option &) { search_.resizeTT(); }));
        OPTIONS.add(Option("ClearHash", [this](const Option &) { search_.newGame(); }));
        OPTIONS.add(Option("Threads", SpinOption(OptionList::DEFAULT_THREADS, OptionList::DEFAULT_THREADS, OptionList::MIN_THREADS, OptionList::MAX_THREADS), [this](const Option &) { search_.setThreads(); }));
        OPTIONS.add(Option("MultiPV", SpinOption(OptionList::DEFAULT_MULTI_PV, OptionList::DEFAULT_MULTI_PV, OptionList::MIN_MULTI_PV, OptionList::MAX_MULTI_PV), [](const Option &) {}));
        OPTIONS.add(Option("Contempt", SpinOption(OptionList::DEFAULT_CONTEMPT, OptionList::DEFAULT_CONTEMPT, OptionList::MIN_CONTEMPT, OptionList::MAX_CONTEMPT), [](const Option &) {}));
        OPTIONS.add(Option("MoveOverhead", SpinOption(OptionList::DEFAULT_MOVE_OVERHEAD_MS, OptionList::DEFAULT_MOVE_OVERHEAD_MS, OptionList::MIN_MOVE_OVERHEAD_MS, OptionList::MAX_MOVE_OVERHEAD_MS), [](const Option &) {}));
        OPTIONS.add(Option("ShowWDL", CheckOption(OptionList::DEFAULT_SHOW_WDL), [](const Option &) {}));

#if defined(OPEN_BENCH_TUNE)
        for (Tunable &tunable : TUNABLES) {
            OPTIONS.add(Option(tunable.name(), SpinOption(tunable.value(), tunable.value(), tunable.min(), tunable.max()), [&tunable](const Option &option) { tunable.update(static_cast<Int32>(option.spinValue())); }));
        }
#endif

        genLegalMoves();
    }

    void run() {
        std::string line;
        while (true) {
            std::getline(std::cin, line);
            if (execute(line)) {
                break;
            }
        }
    }

    bool execute(const std::string &line) {
        std::istringstream stream = std::istringstream(line);
        std::string token;
        stream >> token;
        Command cmd = command(token);

        if (search_.running()) {
            if (cmd == Command::STOP) {
                stop();
            } else if (cmd == Command::QUIT) {
                stop();
                return true;
            }
            return false;
        }

        if (cmd == Command::UCI) {
            uci();
        } else if (cmd == Command::IS_READY) {
            isReady();
        } else if (cmd == Command::NEW_GAME) {
            newGame();
        } else if (cmd == Command::POSITION) {
            position(stream);
        } else if (cmd == Command::GO) {
            go(stream);
        } else if (cmd == Command::STOP) {
            stop();
        } else if (cmd == Command::SET_OPTION) {
            setOption(stream);
        } else if (cmd == Command::QUIT) {
            return true;
        } else if (cmd == Command::BENCH) {
            bench(stream);
        } else if (cmd == Command::PERFT) {
            perft(stream);
        } else if (cmd == Command::PERFT_TESTS) {
            perftTests();
        } else if (cmd == Command::BOARD) {
            board();
        } else if (cmd == Command::MOVES) {
            moves();
        } else if (cmd == Command::EVAL) {
            eval();
        } else if (cmd == Command::RAW_EVAL) {
            rawEval();
        } else if (cmd == Command::FEN) {
            fen();
        } else if (cmd == Command::HASH) {
            hash();
        } else if (cmd == Command::HELP) {
            help();
        }

        return false;
    }

    bool searching() const noexcept { return search_.running(); }

private:
    enum class Command {
        UCI,
        IS_READY,
        NEW_GAME,
        POSITION,
        GO,
        STOP,
        SET_OPTION,
        QUIT,
        BENCH,
        PERFT,
        PERFT_TESTS,
        BOARD,
        MOVES,
        EVAL,
        RAW_EVAL,
        FEN,
        HASH,
        HELP,
        NONE
    };

    Position position_;
    MoveList legalMoves_;
    Search search_;

    mutable std::mutex stdoutMutex_;

    std::unique_lock<std::mutex> lockStdout() const {
        return std::unique_lock<std::mutex>(stdoutMutex_);
    }

    constexpr Command command(const std::string &token) const noexcept {
        if (token == "uci") {
            return Command::UCI;
        } else if (token == "isready") {
            return Command::IS_READY;
        } else if (token == "ucinewgame") {
            return Command::NEW_GAME;
        } else if (token == "position") {
            return Command::POSITION;
        } else if (token == "go") {
            return Command::GO;
        } else if (token == "stop") {
            return Command::STOP;
        } else if (token == "setoption") {
            return Command::SET_OPTION;
        } else if (token == "quit") {
            return Command::QUIT;
        } else if (token == "bench") {
            return Command::BENCH;
        } else if (token == "perft") {
            return Command::PERFT;
        } else if (token == "perfttests") {
            return Command::PERFT_TESTS;
        } else if (token == "board" || token == "d") {
            return Command::BOARD;
        } else if (token == "moves") {
            return Command::MOVES;
        } else if (token == "eval") {
            return Command::EVAL;
        } else if (token == "raweval") {
            return Command::RAW_EVAL;
        } else if (token == "fen") {
            return Command::FEN;
        } else if (token == "hash") {
            return Command::HASH;
        } else if (token == "help" || token == "about") {
            return Command::HELP;
        }

        return Command::NONE;
    }

    void uci() const {
        std::unique_lock<std::mutex> lock = lockStdout();

        std::cout << "id name Sift " << TOSTRING(BUILD_VERSION) << std::endl;
        std::cout << "id author andrewharabor" << std::endl;

        for (const auto &option : OPTIONS) {
            std::cout << "option name " << option.name() << " type ";
            if (option.type() == OptionType::CHECK) {
                std::cout << "check default " << std::boolalpha << option.checkValue() << std::noboolalpha << std::endl;
            } else if (option.type() == OptionType::SPIN) {
                std::cout << "spin default " << option.spinData().defaultValue << " min " << option.spinData().min << " max " << option.spinData().max << std::endl;
            } else if (option.type() == OptionType::STRING) {
                std::cout << "string default " << option.stringValue() << std::endl;
            } else if (option.type() == OptionType::BUTTON) {
                std::cout << "button" << std::endl;
            }
        }

        std::cout << "uciok" << std::endl;
    }

    void isReady() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "readyok" << std::endl;
    }

    void newGame() { search_.newGame(); }

    void position(std::istringstream &stream) {
        position_ = Position();

        std::string token;
        stream >> token;
        if (token == "startpos") {
            position_ = Position();

            stream >> token;
            if (token != "moves") {
                return;
            }
        } else if (token == "fen") {
            std::string fen;
            stream >> fen;
            while (stream >> token) {
                if (token == "moves") {
                    break;
                }
                fen += " " + token;
            }

            if (!position_.set(fen)) {
                return;
            }

        } else {
            return;
        }

        genLegalMoves();
        while (stream >> token) {
            auto compare = [&token](Move move) { return std::string(move) == token; };
            USize index = legalMoves_.findIf(compare);
            if (index >= legalMoves_.size()) {
                return;
            }
            position_.makeMove(legalMoves_[index]);
            genLegalMoves();
        }
    }

    void go(std::istringstream &stream) {
        std::string token;
        SearchLimits limits = SearchLimits();
        limits.overhead = MS(OPTIONS["MoveOverhead"].spinValue());

        while (stream >> token) {
            if (token == "wtime") {
                Int32 whiteTime;
                stream >> whiteTime;
                limits.clock.time[static_cast<USize>(Color::WHITE)] = MS(whiteTime);
                limits.clock.enabled = true;
            } else if (token == "btime") {
                Int32 blackTime;
                stream >> blackTime;
                limits.clock.time[static_cast<USize>(Color::BLACK)] = MS(blackTime);
                limits.clock.enabled = true;
            } else if (token == "winc") {
                Int32 whiteIncrement;
                stream >> whiteIncrement;
                limits.clock.increment[static_cast<USize>(Color::WHITE)] = MS(whiteIncrement);
                limits.clock.enabled = true;
            } else if (token == "binc") {
                Int32 blackIncrement;
                stream >> blackIncrement;
                limits.clock.increment[static_cast<USize>(Color::BLACK)] = MS(blackIncrement);
                limits.clock.enabled = true;
            } else if (token == "movestogo") {
                Int32 movesToGo;
                stream >> movesToGo;
                limits.clock.movesToGo = movesToGo;
                limits.clock.enabled = true;
            } else if (token == "depth") {
                Int32 depth;
                stream >> depth;
                limits.depth = depth;
            } else if (token == "nodes") {
                UInt64 nodes;
                stream >> nodes;
                limits.nodes = nodes;
            } else if (token == "movetime") {
                Int32 moveTime;
                stream >> moveTime;
                limits.time = MS(moveTime);
            } else if (token == "searchmoves") {
                genLegalMoves();
                while (stream >> token) {
                    auto compare = [&token](Move move) { return std::string(move) == token; };
                    USize index = legalMoves_.findIf(compare);
                    if (index >= legalMoves_.size()) {
                        continue;
                    }
                    limits.moves.add(legalMoves_[index]);
                }
            } else if (token == "infinite") {
                limits.infinite = true;
            }
        }

        search_.run(position_, limits);
    }

    void stop() {
        if (search_.running()) {
            search_.stop();
        }
    }

    void searchInfo(const SearchInfo &info) {
        std::unique_lock<std::mutex> lock = lockStdout();

        const Int32 material = position_.materialScore();

        std::cout << "info depth " << info.depth;
        std::cout << " seldepth " << info.selDepth;
        std::cout << " multipv " << (info.pvIdx + 1);

        std::cout << " score ";
        if (Score::mate(info.score)) {
            if (info.score > 0) {
                std::cout << "mate " << ((Score::MATE - info.score) + 1) / 2;
            } else {
                std::cout << "mate -" << (info.score + Score::MATE) / 2;
            }
        } else if (Score::draw(info.score)) {
            std::cout << "cp 0";
        } else {
            const Int32 normedScore = WDL::normalize(info.score, material);
            std::cout << "cp " << normedScore;
        }
        if (info.lowerBound) {
            std::cout << " lowerbound";
        }
        if (info.upperBound) {
            std::cout << " upperbound";
        }

        if (OPTIONS["ShowWDL"].checkValue()) {
            std::cout << " wdl ";

            if (Score::mate(info.score)) {
                if (info.score > 0) {
                    std::cout << "1000 0 0";
                } else {
                    std::cout << "0 0 1000";
                }
            } else {
                const auto [win, loss] = WDL::model(info.score, material);
                const Int32 draw = 1000 - win - loss;
                std::cout << win << " " << draw << " " << loss;
            }
        }

        std::cout << " time " << info.time.count();
        std::cout << " nodes " << info.nodes;
        std::cout << " nps " << (info.nodes * 1000ULL) / (static_cast<UInt64>(info.time.count()) + 1);
        std::cout << " hashfull " << info.hashfull;

        std::cout << " pv ";
        for (const Move move : info.pv) {
            std::cout << std::string(move) << " ";
        }

        std::cout << std::endl;
    }

    void currMove(Move move, Int32 moveNum, Int32 depth) const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info depth " << depth << " currmove " << std::string(move) << " currmovenumber " << moveNum << std::endl;
    }

    void bestMove(Move bestMove) const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "bestmove " << std::string(bestMove) << std::endl;
    }

    void setOption(std::istringstream &stream) {
        std::string token;
        std::string name;

        stream >> token;
        if (token != "name") {
            return;
        }

        stream >> name;

        if (!OPTIONS.has(name)) {
            return;
        }

        Option &option = OPTIONS[name];

        stream >> token;
        if (option.type() != OptionType::BUTTON && token != "value") {
            return;
        }

        if (option.type() == OptionType::CHECK) {
            std::string value;
            stream >> value;
            if (value == "true") {
                option.setCheck(true);
            } else if (value == "false") {
                option.setCheck(false);
            }
        } else if (option.type() == OptionType::SPIN) {
            Int64 value;
            stream >> value;
            option.setSpin(value);
        } else if (option.type() == OptionType::STRING) {
            std::string value;
            stream >> value;
            option.setString(value);
        } else if (option.type() == OptionType::BUTTON) {
            option.pressButton();
        }
    }

    void bench(std::istringstream &stream) {
        std::unique_lock<std::mutex> lock = lockStdout();

        Int32 depth;
        stream >> depth;

        UInt64 nodes = 0;
        UInt64 time = 0;
        for (USize i = 0; i < Bench::TEST_COUNT; i++) {
            Position position = Position(Bench::TEST_CASES[i]);

            SearchLimits limits = SearchLimits();
            limits.depth = depth;

            search_.newGame();

            const auto start = std::chrono::high_resolution_clock::now();
            const UInt64 testNodes = search_.bench(position, limits);
            const auto end = std::chrono::high_resolution_clock::now();
            const UInt64 testTime = static_cast<UInt64>(std::chrono::duration_cast<MS>(end - start).count());

            std::cout << "fen " << Bench::TEST_CASES[i];
            std::cout << " depth " << depth;
            std::cout << " nodes " << testNodes;
            std::cout << " time " << testTime;
            std::cout << " nps " << (testNodes * 1000ULL) / (testTime + 1);
            std::cout << std::endl;

            nodes += testNodes;
            time += testTime;
        }

        std::cout << "nodes " << nodes;
        std::cout << " time " << time;
        std::cout << " nps " << (nodes * 1000ULL) / (time + 1);
#if defined(MEASURE_SPARSITY)
        std::cout << " sparsity " << NNUE::saveFTActs("ft-acts.json");
#endif
        std::cout << std::endl;
    }

    void perft(std::istringstream &stream) {
        std::unique_lock<std::mutex> lock = lockStdout();

        Int32 depth;
        stream >> depth;

        UInt64 nodes = 0;
        UInt64 time = 0;

        MoveList legalMoves;
        MoveGen::legal(position_, legalMoves);
        for (Move move : legalMoves) {
            Position newPosition = position_;
            newPosition.makeMove(move);

            const auto start = std::chrono::high_resolution_clock::now();
            const UInt64 moveNodes = Perft::run(newPosition, std::max(0, depth - 1));
            const auto end = std::chrono::high_resolution_clock::now();
            const UInt64 moveTime = static_cast<UInt64>(std::chrono::duration_cast<MS>(end - start).count());

            std::cout << "move " << std::string(move);
            std::cout << " nodes " << moveNodes;
            std::cout << " time " << moveTime;
            std::cout << " nps " << (moveNodes * 1000ULL) / (moveTime + 1);
            std::cout << std::endl;

            nodes += moveNodes;
            time += moveTime;
        }

        std::cout << "depth " << depth;
        std::cout << " nodes " << nodes;
        std::cout << " time " << time;
        std::cout << " nps " << (nodes * 1000ULL) / (time + 1);
        std::cout << std::endl;
    }

    void perftTests() {
        std::unique_lock<std::mutex> lock = lockStdout();

        USize testsPassed = 0;
        for (USize i = 0; i < Perft::TEST_COUNT; i++) {
            const PerftTest &testCase = Perft::TEST_CASES[i];
            Position position = Position(testCase.fen);

            const auto start = std::chrono::high_resolution_clock::now();
            const UInt64 nodes = Perft::run(position, testCase.depth);
            const auto end = std::chrono::high_resolution_clock::now();
            const UInt64 time = static_cast<UInt64>(std::chrono::duration_cast<MS>(end - start).count());

            if (nodes == testCase.expectedNodes) {
                testsPassed++;
            }

            std::cout << "fen " << testCase.fen;
            std::cout << " depth " << testCase.depth;
            std::cout << " expected " << testCase.expectedNodes;
            std::cout << " nodes " << nodes;
            std::cout << " time " << time;
            std::cout << " nps " << (nodes * 1000ULL) / (time + 1);
            std::cout << " result " << (nodes == testCase.expectedNodes ? "pass" : "fail");
            std::cout << std::endl;
        }
        std::cout << "passed " << testsPassed << " failed " << (Perft::TEST_COUNT - testsPassed) << std::endl;
    }

    void board() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << std::string(position_) << std::endl;
    }

    void moves() {
        std::unique_lock<std::mutex> lock = lockStdout();

        genLegalMoves();
        for (const Move move : legalMoves_) {
            std::cout << std::string(move) << " ";
        }
        std::cout << std::endl;
    }

    void eval() const {
        std::unique_lock<std::mutex> lock = lockStdout();

        if (position_.inCheck()) {
            std::cout << "(none)";
        } else {
            NNUE nnue = NNUE();
            nnue.state().set(position_);
            const Int32 normedScore = WDL::normalize(Eval::adjusted(position_, nnue, {0, 0}, {0, 0}), position_.materialScore());
            std::cout << normedScore << " cp";
        }
        std::cout << std::endl;
    }

    void rawEval() const {
        std::unique_lock<std::mutex> lock = lockStdout();

        NNUE nnue = NNUE();
        nnue.state().set(position_);
        std::cout << nnue.forward(position_) << std::endl;
    }

    void fen() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << position_.fen() << std::endl;
    }

    void hash() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << std::showbase << std::uppercase << std::hex << position_.hash() << std::dec << std::nouppercase << std::noshowbase << std::endl;
    }

    void help() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "Sift " << TOSTRING(BUILD_VERSION) << ", a strong UCI chess engine by andrewharabor" << std::endl;
        std::cout << "https://github.com/andrewharabor/Sift" << std::endl;
    }

    void infoString(const std::string &info) const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info string " << info << std::endl;
    }

    void genLegalMoves() noexcept {
        legalMoves_.clear();
        MoveGen::legal(position_, legalMoves_);
    }
};

}
