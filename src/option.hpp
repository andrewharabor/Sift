#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "types.hpp"
#include "utils.hpp"

namespace Sift {

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

    Option() noexcept : type_(OptionType::NONE), name_(), id_(), data_(), callback_() {}
    Option(std::string_view name, CheckOption data, Callback callback) : type_(OptionType::CHECK), name_(name), id_(name), data_(std::in_place_type<CheckOption>, data), callback_(std::move(callback)) { Utils::stringToLower(id_); }
    Option(std::string_view name, SpinOption data, Callback callback) : type_(OptionType::SPIN), name_(name), id_(name), data_(std::in_place_type<SpinOption>, data), callback_(std::move(callback)) { Utils::stringToLower(id_); }
    Option(std::string_view name, StringOption data, Callback callback) : type_(OptionType::STRING), name_(name), id_(name), data_(std::in_place_type<StringOption>, data), callback_(std::move(callback)) { Utils::stringToLower(id_); }
    Option(std::string_view name, Callback callback) : type_(OptionType::BUTTON), name_(name), id_(name), data_(std::in_place_type<std::monostate>), callback_(std::move(callback)) { Utils::stringToLower(id_); }

    constexpr OptionType type() const noexcept { return type_; }
    constexpr const std::string &name() const noexcept { return name_; }
    constexpr const std::string &id() const noexcept { return id_; }

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
    std::string id_;
    std::variant<std::monostate, CheckOption, SpinOption, StringOption> data_;
    Callback callback_;
};

class OptionList {
public:
    static constexpr Int64 DEFAULT_THREADS = 1;
    static constexpr Int64 MIN_THREADS = 1;
    static constexpr Int64 MAX_THREADS = 2048;

    static constexpr Int64 DEFAULT_HASH_MB = 64;
    static constexpr Int64 MIN_HASH_MB = 1;
    static constexpr Int64 MAX_HASH_MB = 33554432;

    static constexpr Int64 DEFAULT_MULTI_PV = 1;
    static constexpr Int64 MIN_MULTI_PV = 1;
    static constexpr Int64 MAX_MULTI_PV = 256;

    static constexpr Int64 DEFAULT_CONTEMPT = 0;
    static constexpr Int64 MIN_CONTEMPT = -1000;
    static constexpr Int64 MAX_CONTEMPT = 1000;

    static constexpr Int64 DEFAULT_MOVE_OVERHEAD_MS = 10;
    static constexpr Int64 MIN_MOVE_OVERHEAD_MS = 0;
    static constexpr Int64 MAX_MOVE_OVERHEAD_MS = 5000;

    static constexpr bool DEFAULT_SOFT_NODES = false;

    static constexpr bool DEFAULT_SHOW_WDL = true;

    constexpr OptionList() noexcept : options_() {}

    constexpr std::vector<Option>::iterator begin() noexcept { return options_.begin(); }
    constexpr std::vector<Option>::const_iterator begin() const noexcept { return options_.begin(); }
    constexpr std::vector<Option>::iterator end() noexcept { return options_.end(); }
    constexpr std::vector<Option>::const_iterator end() const noexcept { return options_.end(); }

    Option &operator[](const std::string &name) noexcept {
        std::string id = name;
        Utils::stringToLower(id);
        auto it = std::find_if(options_.begin(), options_.end(), [&id](const Option &option) { return option.id() == id; });
        assert(it != options_.end());
        return *it;
    }

    bool has(const std::string &name) noexcept {
        std::string id = name;
        Utils::stringToLower(id);
        auto it = std::find_if(options_.begin(), options_.end(), [&id](const Option &option) { return option.id() == id; });
        return it != options_.end();
    }

    void add(const Option &option) { options_.push_back(option); }

private:
    std::vector<Option> options_;
};

inline OptionList OPTIONS;

}
