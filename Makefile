SHELL := /bin/zsh
CXX := /usr/bin/clang++
CXXFLAGS := -std=c++23 -g -pedantic -Wall -Wextra -Werror -Wshadow -Wfloat-equal -Wconversion -fdiagnostics-color=always -fsanitize=undefined,address
LDFLAGS :=
LDLIBS :=

TARGET_EXEC := main
BUILD_DIR := build
SRC_DIRS := src

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
