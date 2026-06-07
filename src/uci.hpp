#pragma once

#include <cassert>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "bench.hpp"
#include "color.hpp"
#include "move.hpp"
#include "move-gen.hpp"
#include "nnue.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "search.hpp"
#include "time.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "wdl.hpp"

namespace Syft {

enum class OptionType {
    CHECK,
    SPIN,
    STRING,
    BUTTON,
    NONE
};

struct CheckOption {
    bool value;
};

struct SpinOption {
    Int64 value;
    Int64 defaultValue;
    Int64 min;
    Int64 max;
};

struct StringOption {
    std::string value;
    std::string defaultValue;
};

class Option {
public:
    using Callback = std::function<void(const Option &)>;

    static constexpr Int64 DEFAULT_THREADS = 1;
    static constexpr Int64 MIN_THREADS = 1;
    static constexpr Int64 MAX_THREADS = 2048;

    static constexpr Int64 DEFAULT_HASH_MB = 64;
    static constexpr Int64 MIN_HASH_MB = 1;
    static constexpr Int64 MAX_HASH_MB = 33554432;

    static constexpr Int64 DEFAULT_MULTI_PV = 1;
    static constexpr Int64 MIN_MULTI_PV = 1;
    static constexpr Int64 MAX_MULTI_PV = 256;

    static constexpr bool DEFAULT_SHOW_WDL = true;

    static constexpr Int64 DEFAULT_MOVE_OVERHEAD_MS = 10;
    static constexpr Int64 MIN_MOVE_OVERHEAD_MS = 0;
    static constexpr Int64 MAX_MOVE_OVERHEAD_MS = 1000;

    static constexpr bool DEFAULT_SOFT_NODES = false;

    static constexpr std::string_view DEFAULT_EVAL_FILE = "<internal>";

    Option() noexcept : type_(OptionType::NONE), name_(), data_(), callback_() {}
    Option(std::string_view name, CheckOption data, Callback callback) : type_(OptionType::CHECK), name_(name), data_(std::in_place_type<CheckOption>, data), callback_(std::move(callback)) {}
    Option(std::string_view name, SpinOption data, Callback callback) : type_(OptionType::SPIN), name_(name), data_(std::in_place_type<SpinOption>, data), callback_(std::move(callback)) {}
    Option(std::string_view name, StringOption data, Callback callback) : type_(OptionType::STRING), name_(name), data_(std::in_place_type<StringOption>, data), callback_(std::move(callback)) {}
    Option(std::string_view name, Callback callback) : type_(OptionType::BUTTON), name_(name), data_(std::in_place_type<std::monostate>), callback_(std::move(callback)) {}

    constexpr OptionType type() const noexcept { return type_; }
    constexpr const std::string &name() const noexcept { return name_; }

    void setCheck(bool value) {
        assert(type_ == OptionType::CHECK);
        std::get<CheckOption>(data_).value = value;
        callback_(*this);
    }

    void setSpin(Int64 value) {
        assert(type_ == OptionType::SPIN);
        SpinOption &spinData = std::get<SpinOption>(data_);
        if (value < spinData.min) {
            value = spinData.min;
        } else if (value > spinData.max) {
            value = spinData.max;
        }
        spinData.value = value;
        callback_(*this);
    }

    void setString(const std::string &value) {
        assert(type_ == OptionType::STRING);
        std::get<StringOption>(data_).value = value;
        callback_(*this);
    }

    void pressButton() {
        assert(type_ == OptionType::BUTTON);
        callback_(*this);
    }

    constexpr bool checkValue() const noexcept {
        assert(type_ == OptionType::CHECK);
        return std::get<CheckOption>(data_).value;
    }

    constexpr Int64 spinValue() const noexcept {
        assert(type_ == OptionType::SPIN);
        return std::get<SpinOption>(data_).value;
    }

    constexpr SpinOption spinData() const noexcept {
        assert(type_ == OptionType::SPIN);
        return std::get<SpinOption>(data_);
    }

    constexpr const std::string &stringValue() const noexcept {
        assert(type_ == OptionType::STRING);
        return std::get<StringOption>(data_).value;
    }

private:
    OptionType type_;
    std::string name_;
    std::variant<std::monostate, CheckOption, SpinOption, StringOption> data_;
    Callback callback_;
};

class UCI {
public:
    UCI() : position_(), legalMoves_(), search_(Option::DEFAULT_HASH_MB, Option::DEFAULT_MULTI_PV, [this](const SearchInfo &info) { searchInfo(info); }, [this](const Move move) { bestMove(move); }, [this](const Move move, Int32 moveNum, Int32 depth) { currMove(move, moveNum, depth); }) {
        options_.push_back(Option("Hash", SpinOption(Option::DEFAULT_HASH_MB, Option::DEFAULT_HASH_MB, Option::MIN_HASH_MB, Option::MAX_HASH_MB), [this](const Option &option) { search_.resizeTTable(static_cast<USize>(option.spinValue())); }));
        options_.push_back(Option("ClearHash", [this]([[maybe_unused]] const Option &option) { search_.newGame(); }));
        options_.push_back(Option("Threads", SpinOption(Option::DEFAULT_THREADS, Option::DEFAULT_THREADS, Option::MIN_THREADS, Option::MAX_THREADS), [this](const Option &option) { search_.threadCount(static_cast<Int32>(option.spinValue())); }));
        options_.push_back(Option("MultiPV", SpinOption(Option::DEFAULT_MULTI_PV, Option::DEFAULT_MULTI_PV, Option::MIN_MULTI_PV, Option::MAX_MULTI_PV), [this](const Option &option) { search_.multiPV(static_cast<USize>(option.spinValue())); }));
        options_.push_back(Option("ShowWDL", CheckOption(Option::DEFAULT_SHOW_WDL), []([[maybe_unused]] const Option &option) {}));
        options_.push_back(Option("MoveOverhead", SpinOption(Option::DEFAULT_MOVE_OVERHEAD_MS, Option::DEFAULT_MOVE_OVERHEAD_MS, Option::MIN_MOVE_OVERHEAD_MS, Option::MAX_MOVE_OVERHEAD_MS), []([[maybe_unused]] const Option &option) {}));
        options_.push_back(Option("SoftNodes", CheckOption(Option::DEFAULT_SOFT_NODES), []([[maybe_unused]] const Option &option) {}));

        auto evalFileCallback = [this](const Option &option) {
            if (option.stringValue() != Option::DEFAULT_EVAL_FILE) {
                search_.loadEvalFile(option.stringValue());
            } else {
                search_.loadInternalEvalFile();
            }
        };

        options_.push_back(Option("EvalFile", StringOption(std::string(Option::DEFAULT_EVAL_FILE), std::string(Option::DEFAULT_EVAL_FILE)), evalFileCallback));

#if defined(OPEN_BENCH_TUNE)
        for (Tunable &tunable : TUNABLES) {
            options_.push_back(Option(tunable.name(), SpinOption(tunable.value(), tunable.value(), tunable.min(), tunable.max()), [&tunable](const Option &option) { tunable.update(static_cast<Int32>(option.spinValue())); }));
        }
#endif

        legalMoves();
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
        FEN,
        HASH,
        NONE
    };

    Position position_;
    MoveList legalMoves_;
    Search search_;

    std::vector<Option> options_;

    mutable std::mutex stdoutMutex_;

    std::unique_lock<std::mutex> lockStdout() const {
        return std::unique_lock<std::mutex>(stdoutMutex_);
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
        } else if (cmd == Command::FEN) {
            fen();
        } else if (cmd == Command::HASH) {
            hash();
        }

        return false;
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
        } else if (token == "board") {
            return Command::BOARD;
        } else if (token == "moves") {
            return Command::MOVES;
        } else if (token == "eval") {
            return Command::EVAL;
        } else if (token == "fen") {
            return Command::FEN;
        } else if (token == "hash") {
            return Command::HASH;
        }

        return Command::NONE;
    }

    void uci() const {
        std::unique_lock<std::mutex> lock = lockStdout();

        std::cout << "id name Syft " << TOSTRING(BUILD_VERSION) << std::endl;
        std::cout << "id author andrewharabor" << std::endl;

        for (const auto &option : options_) {
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

    void newGame() {
        search_.newGame();
    }

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

        legalMoves();
        while (stream >> token) {
            auto compare = [&token](const Move move) { return std::string(move) == token; };
            USize index = legalMoves_.findIf(compare);
            if (index >= legalMoves_.size()) {
                return;
            }
            position_.make(legalMoves_[index]);
            legalMoves();
        }
    }

    void go(std::istringstream &stream) {
        std::string token;
        SearchLimits limits = SearchLimits();
        limits.overhead = MS(options_[optionIndex("MoveOverhead")].spinValue());
        limits.softNodes = options_[optionIndex("SoftNodes")].checkValue();

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
                legalMoves();
                while (stream >> token) {
                    auto compare = [&token](const Move move) { return std::string(move) == token; };
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

        std::cout << "info depth " << info.depth;
        std::cout << " seldepth " << info.selDepth;
        std::cout << " multipv " << (info.pvIndex + 1);

        std::cout << " score ";
        if (Score::mate(info.score)) {
            if (info.score > 0) {
                std::cout << "mate " << ((Score::MATE - info.score) + 1) / 2;
            } else {
                std::cout << "mate -" << (info.score + Score::MATE) / 2;
            }
        } else if (info.score >= Score::DRAW_MIN && info.score <= Score::DRAW_MAX) {
            std::cout << "cp 0";
        } else {
            std::cout << "cp " << info.score;
        }
        if (info.lowerBound) {
            std::cout << " lowerbound";
        }
        if (info.upperBound) {
            std::cout << " upperbound";
        }

        if (options_[optionIndex("ShowWDL")].checkValue()) {
            std::cout << " wdl ";

            if (Score::mate(info.score)) {
                if (info.score > 0) {
                    std::cout << "1000 0 0";
                } else {
                    std::cout << "0 0 1000";
                }
            } else {
                const auto [winProb, lossProb] = WDL::winLoss(position_.materialScore(), info.score);
                const Int32 win = static_cast<Int32>(std::round(winProb * 1000.0));
                const Int32 loss = static_cast<Int32>(std::round(lossProb * 1000.0));
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

    void currMove(const Move move, Int32 moveNum, Int32 depth) const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info depth " << depth << " currmove " << std::string(move) << " currmovenumber " << moveNum << std::endl;
    }

    void bestMove(const Move bestMove) const {
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

        if (optionIndex(name) >= options_.size()) {
            return;
        }

        Option &option = options_[optionIndex(name)];

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

            const auto start = std::chrono::high_resolution_clock::now();
            const UInt64 testNodes = search_.bench(position, limits);
            const auto end = std::chrono::high_resolution_clock::now();
            const UInt64 testTime = static_cast<UInt64>(std::chrono::duration_cast<MS>(end - start).count());

            std::cout << "info fen " << Bench::TEST_CASES[i];
            std::cout << " depth " << depth;
            std::cout << " nodes " << testNodes;
            std::cout << " time " << testTime;
            std::cout << " nps " << (testNodes * 1000ULL) / (testTime + 1);
            std::cout << std::endl;

            nodes += testNodes;
            time += testTime;
        }

        std::cout << "info nodes " << nodes;
        std::cout << " time " << time;
        std::cout << " nps " << (nodes * 1000ULL) / (time + 1);
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
            newPosition.make(move);

            const auto start = std::chrono::high_resolution_clock::now();
            const UInt64 moveNodes = Perft::run(newPosition, std::max(0, depth - 1));
            const auto end = std::chrono::high_resolution_clock::now();
            const UInt64 moveTime = static_cast<UInt64>(std::chrono::duration_cast<MS>(end - start).count());

            std::cout << "info move " << std::string(move);
            std::cout << " nodes " << moveNodes;
            std::cout << " time " << moveTime;
            std::cout << " nps " << (moveNodes * 1000ULL) / (moveTime + 1);
            std::cout << std::endl;

            nodes += moveNodes;
            time += moveTime;
        }

        std::cout << "info depth " << depth;
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

            std::cout << "info fen " << testCase.fen;
            std::cout << " depth " << testCase.depth;
            std::cout << " expected " << testCase.expectedNodes;
            std::cout << " nodes " << nodes;
            std::cout << " time " << time;
            std::cout << " nps " << (nodes * 1000ULL) / (time + 1);
            std::cout << " result " << (nodes == testCase.expectedNodes ? "pass" : "fail");
            std::cout << std::endl;
        }
        std::cout << "info passed " << testsPassed << " failed " << (Perft::TEST_COUNT - testsPassed) << std::endl;
    }

    void board() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        const std::string positionString = std::string(position_);
        const std::vector<std::string_view> lines = Utils::splitStringView(positionString, '\n');
        for (const std::string_view &line : lines) {
            std::cout << "info board " << line << std::endl;
        }
    }

    void moves() {
        std::unique_lock<std::mutex> lock = lockStdout();

        legalMoves();
        std::cout << "info moves ";
        for (const Move move : legalMoves_) {
            std::cout << std::string(move) << " ";
        }
        std::cout << std::endl;
    }

    void eval() const {
        std::unique_lock<std::mutex> lock = lockStdout();

        std::cout << "info eval ";
        if (position_.inCheck()) {
            std::cout << "(none)";
        } else {
            NNUE nnue;
            std::string evalFilePath = options_[optionIndex("EvalFile")].stringValue();
            if (evalFilePath != Option::DEFAULT_EVAL_FILE) {
                nnue.load(evalFilePath);
            }
            nnue.set(position_);
            std::cout << nnue.evaluate(position_) << " cp";
        }
        std::cout << std::endl;
    }

    void fen() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info fen " << position_.fen() << std::endl;
    }

    void hash() const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info hash " << std::showbase << std::uppercase << std::hex << position_.hash() << std::dec << std::nouppercase << std::noshowbase << std::endl;
    }

    void infoString(const std::string &info) const {
        std::unique_lock<std::mutex> lock = lockStdout();
        std::cout << "info string " << info << std::endl;
    }

    void legalMoves() noexcept {
        legalMoves_.clear();
        MoveGen::legal(position_, legalMoves_);
    }

    USize optionIndex(const std::string &name) const noexcept {
        auto it = std::find_if(options_.begin(), options_.end(), [&name](const Option &option) { return option.name() == name; });
        if (it != options_.end()) {
            return static_cast<USize>(it - options_.begin());
        }
        return options_.size();
    }
};

}
