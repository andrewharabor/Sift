ARCH  ?= native
BUILD ?= engine
MODE  ?= release

SRCS := src/main.cpp
CPP_FLAGS := -MMD -MP -Isrc
CXX_FLAGS := -std=c++20 -pedantic -Wall -Wextra -Werror -Wshadow -Wconversion -fdiagnostics-color=always
LD_FLAGS :=

BUILD_DIR := build
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)

ifeq ($(OS),Windows_NT)
	DETECTED_OS := windows
else
	DETECTED_OS := $(shell uname)
endif

CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifneq ($(findstring clang,$(CXX_VERSION)),)
    CXX := clang++
else
    CXX := g++
endif

ifeq ($(DETECTED_OS),windows)
	VERSION := $(shell type version.txt)
else
	VERSION := $(shell cat version.txt)
endif

ifneq (,$(findstring dev,$(VERSION)))
	COMMIT_HASH := $(shell git rev-parse --short HEAD)
	VERSION := $(VERSION)-$(COMMIT_HASH)
endif

CPP_FLAGS += -DBUILD_VERSION=$(VERSION)

ifeq ($(DETECTED_OS),windows)
	EVAL_FILE := $(shell type network.txt)
else
	EVAL_FILE := $(shell cat network.txt)
endif

CPP_FLAGS += -DEVAL_FILE=nets/$(EVAL_FILE).nnue

ifeq ($(DETECTED_OS),windows)
	TARGET_EXEC := Syft$(VERSION).exe
	MKDIR = mkdir
	RM_FILE = del /f /q
	RM_DIR = rmdir /s /q
	SEP = \\
else
	TARGET_EXEC := Syft$(VERSION)
	MKDIR = mkdir -p
	RM_FILE = rm -f
	RM_DIR = rm -rf
	SEP = /
endif

ifeq ($(ARCH),native)
	PROPERTIES = $(shell echo | $(CXX) -march=native -E -dM -)
	CXX_FLAGS += -march=native
	ifneq ($(findstring __POPCNT__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_POPCNT
	endif
	ifneq ($(findstring __BMI2__, $(PROPERTIES)),)
		ifeq ($(findstring __znver1, $(PROPERTIES)),)
			ifeq ($(findstring __znver2, $(PROPERTIES)),)
				CPP_
			FLAGS += -DUSE_PEXT
			endif
		endif
	endif
	ifneq ($(findstring __SSE4_2__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_SSE4
	endif
	ifneq ($(findstring __AVX__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_AVX
	endif
	ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_AVX2
	endif
	ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_AVX512
	endif
	ifneq ($(findstring __AVX512VNNI__, $(PROPERTIES)),)
		ifeq ($(findstring __znver4, $(PROPERTIES)),)
			CPP_
		FLAGS += -DUSE_AVX512_VNNI
		endif
	endif
	ifneq ($(findstring __ARM_NEON, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_NEON
		CPP_FLAGS += -DUSE_NEON_DOTPROD
	endif
else ifeq ($(ARCH),sse4)
	CPP_FLAGS += -DUSE_SSE4
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2
else ifeq ($(ARCH),avx)
	CPP_FLAGS += -DUSE_SSE4 -DUSE_AVX
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mfma
else ifeq ($(ARCH),avx2)
	CPP_FLAGS += -DUSE_SSE4 -DUSE_AVX -DUSE_AVX2 -DUSE_POPCNT
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mfma -mavx2 -mpopcnt
else ifeq ($(ARCH),avx2-pext)
	CPP_FLAGS += -DUSE_SSE4 -DUSE_AVX -DUSE_AVX2 -DUSE_POPCNT -DUSE_PEXT
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mfma -mavx2 -mpopcnt -mbmi -mbmi2
else ifeq ($(ARCH),avx512)
	CPP_FLAGS += -DUSE_SSE4 -DUSE_AVX -DUSE_AVX2 -DUSE_AVX512 -DUSE_POPCNT -DUSE_PEXT
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mfma -mavx2 -mpopcnt -mbmi -mbmi2 -mavx512f -mavx512cd -mavx512vl -mavx512dq -mavx512bw
else ifeq ($(ARCH),avx512vnni)
	CPP_FLAGS += -DUSE_SSE4 -DUSE_AVX -DUSE_AVX2 -DUSE_AVX512 -DUSE_AVX512_VNNI -DUSE_POPCNT -DUSE_PEXT
	CXX_FLAGS += -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -mfma -mavx2 -mpopcnt -mbmi -mbmi2 -mavx512f -mavx512cd -mavx512vl -mavx512dq -mavx512bw -mavx512ifma -mavx512vbmi -mavx512vbmi2 -mavx512bitalg -mavx512vnni -mavx512vpopcntdq
else ifeq ($(ARCH),neon)
	CPP_FLAGS += -DUSE_NEON
	CXX_FLAGS += -march=armv8-a+simd
else ifeq ($(ARCH),neon-dotprod)
	CPP_FLAGS += -DUSE_NEON -DUSE_NEON_DOTPROD
	CXX_FLAGS += -march=armv8.2-a+dotprod
else ifeq ($(ARCH),generic)
	CPP_FLAGS += -DUSE_GENERIC
endif

ifeq ($(DETECTED_OS),windows)
	CXX_FLAGS += -static
else
	CXX_FLAGS += -pthread
	LD_FLAGS  += -pthread
endif

ifeq ($(MODE),release)
	CXX_FLAGS += -O3 -DNDEBUG -funroll-loops -fomit-frame-pointer
	ifneq ($(DETECTED_OS),windows)
		CXX_FLAGS += -flto
		LD_FLAGS  += -flto
	endif
else ifeq ($(MODE),tune)
	CXX_FLAGS += -O3 -DNDEBUG -funroll-loops -fomit-frame-pointer
	CPP_FLAGS += -DOPEN_BENCH_TUNE
	ifneq ($(DETECTED_OS),windows)
		CXX_FLAGS += -flto
		LD_FLAGS  += -flto
	endif
else ifeq ($(MODE),debug)
	CXX_FLAGS += -O0 -g3 -fsanitize=undefined,address -fno-omit-frame-pointer
	LD_FLAGS  += -fsanitize=undefined,address
endif

-include $(DEPS)

$(TARGET_EXEC): info $(OBJS)
	$(CXX) $(CXX_FLAGS) $(OBJS) -o $@ $(LD_FLAGS)

$(BUILD_DIR)/%.o: %.cpp
	$(MKDIR) "$(subst /,$(SEP),$(dir $@))"
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $< -o $@

.DEFAULT_GOAL := engine

.PHONY: engine
engine: $(TARGET_EXEC)

.PHONY: info
info:
	@echo Detected OS: $(DETECTED_OS)
	@echo Detected compiler: $(CXX)
	@echo Build architecture: $(ARCH)
	@echo Build mode: $(MODE)

.PHONY: clean
clean:
	$(RM_FILE) $(TARGET_EXEC)
	$(RM_DIR) $(BUILD_DIR)

.PHONY: help
help:
	@echo "Usage: make <TARGET> <ARCH=[native|sse4|avx|avx2|avx2-pext|avx512|avx512vnni|neon|neon-dotprod|generic]> <MODE=[release|tune|debug]>"
	@echo "Targets:"
	@echo "  engine"
	@echo "  info"
	@echo "  clean"
	@echo "  help"
