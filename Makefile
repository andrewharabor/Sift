
BUILD ?= engine
MODE ?= release

SHELL := /bin/zsh
CXX := /usr/bin/clang++

SRC_DIRS :=
BUILD_DIR := build
TARGET_EXEC :=

ifeq ($(BUILD),engine)
	SRC_DIRS := src
	TARGET_EXEC := Clownfish
else ifeq ($(BUILD),benchmark)
	SRC_DIRS := benchmark
	TARGET_EXEC := Clownfish
else
	$(error Invalid BUILD '$(BUILD)'. Use BUILD=engine or BUILD=benchmark)
endif

BASE_CXXFLAGS := -std=c++23 -pedantic -Wall -Wextra -Werror -Wshadow -Wfloat-equal -Wconversion -fdiagnostics-color=always
CXXFLAGS := $(BASE_CXXFLAGS)
LDFLAGS :=
LDLIBS :=

ifeq ($(MODE),release)
	CXXFLAGS += -O3 -DNDEBUG
else ifeq ($(MODE),debug)
	CXXFLAGS += -O0 -g3
else ifeq ($(MODE),sanitize)
	CXXFLAGS += -O1 -g3 -fsanitize=undefined,address -fno-omit-frame-pointer
	LDFLAGS += -fsanitize=undefined,address
else
	$(error Invalid MODE '$(MODE)'. Use MODE=release, MODE=debug, or MODE=sanitize)
endif

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CPPFLAGS := $(INC_FLAGS) -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
