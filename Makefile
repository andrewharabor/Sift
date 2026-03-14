ARCH  ?= auto
BUILD ?= engine
MODE  ?= release


ifeq ($(OS),Windows_NT)
    HOST_OS := windows
else
    HOST_OS := $(shell uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]')
    ifeq ($(HOST_OS),darwin)
        HOST_OS := macos
    endif
endif

ifeq ($(ARCH),auto)
    ifeq ($(OS),Windows_NT)
        _RAW_ARCH := $(PROCESSOR_ARCHITECTURE)
    else
        _RAW_ARCH := $(shell uname -m 2>/dev/null)
    endif
    ifeq ($(_RAW_ARCH),x86_64)
        ARCH := x86-64-modern
    else ifeq ($(_RAW_ARCH),amd64)
        ARCH := x86-64-modern
    else ifeq ($(_RAW_ARCH),aarch64)
        ARCH := aarch64
    else ifeq ($(_RAW_ARCH),arm64)
        ARCH := aarch64
    else
        ARCH := generic
    endif
endif

ifeq ($(OS),Windows_NT)
    CXX ?= g++
    TARGET_EXEC := Clownfish.exe
    MKDIR = mkdir
    RM_FILE = del /f /q
    RM_DIR = rmdir /s /q
    SEP = \\
else
    CXX ?= c++
    TARGET_EXEC := Clownfish
    MKDIR = mkdir -p
    RM_FILE = rm -f
    RM_DIR = rm -rf
    SEP = /
endif


CPPFLAGS := -MMD -MP
CXXFLAGS := -std=c++23 -pedantic -Wall -Wextra -Werror -Wshadow -Wfloat-equal -Wconversion -fdiagnostics-color=always

ifeq ($(BUILD),engine)
    SRCS := src/main.cpp
    CPPFLAGS += -Isrc
else ifeq ($(BUILD),benchmark)
    SRCS:= benchmark/main.cpp
    CPPFLAGS += -Ibenchmark
else
    $(error Invalid BUILD '$(BUILD)'. Use BUILD=engine or BUILD=benchmark)
endif

ifeq ($(ARCH),x86-64-avx2)
    CPPFLAGS += -DUSE_AVX2
    CXXFLAGS += -m64 -mavx2 -mpopcnt
    LDFLAGS += -m64
else ifeq ($(ARCH),x86-64-modern)
    CPPFLAGS += -DUSE_SSE
    CXXFLAGS += -m64 -msse4.1 -mpopcnt
    LDFLAGS += -m64
else ifeq ($(ARCH),aarch64)
    CPPFLAGS += -DUSE_NEON
else ifeq ($(ARCH),generic)
    CPPFLAGS += -DUSE_GENERIC
    CXXFLAGS += -m64
    LDFLAGS += -m64
else
    $(error Invalid ARCH '$(ARCH)'. Use ARCH=auto|generic|x86-64-modern|x86-64-avx2|aarch64)
endif

ifneq ($(HOST_OS),windows)
    CXXFLAGS += -pthread
    LDFLAGS  += -pthread
endif

ifeq ($(MODE),release)
    CXXFLAGS += -O3 -DNDEBUG -funroll-loops -fomit-frame-pointer
    ifneq ($(HOST_OS),windows)
        CXXFLAGS += -flto
        LDFLAGS  += -flto
    endif
else ifeq ($(MODE),debug)
    CXXFLAGS += -O0 -g3 -fsanitize=undefined,address -fno-omit-frame-pointer
    LDFLAGS  += -fsanitize=undefined,address
else
    $(error Invalid MODE '$(MODE)'. Use MODE=release or MODE=debug)
endif


BUILD_DIR := build
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)

-include $(DEPS)

$(BUILD_DIR)/%.o: %.cpp
	$(MKDIR) "$(subst /,$(SEP),$(dir $@))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


$(TARGET_EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM_FILE) $(TARGET_EXEC)
	$(RM_DIR) $(BUILD_DIR)

.PHONY: help
help:
	@echo "Usage: make [TARGET] [ARCH=...] [BUILD=...] [MODE=...]"
	@echo "Targets:"
	@echo "  $(TARGET_EXEC)"
	@echo "  clean"
	@echo "  help"
	@echo "Options:"
	@echo "  ARCH=[auto|generic|x86-64-modern|x86-64-avx2|aarch64]"
	@echo "  BUILD=[engine|benchmark]"
	@echo "  MODE=[release|debug]"
