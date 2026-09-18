ARCH  ?= native
MODE  ?= release
NUMA ?= off

BUILD_DIR := build/$(ARCH)

MAIN_SRCS := src/main.cpp
MAIN_OBJS := $(MAIN_SRCS:%.cpp=$(BUILD_DIR)/%.o)
MAIN_DEPS := $(MAIN_OBJS:%.o=%.d)

PERM_SRCS := tools/permute.cpp
PERM_OBJS := $(PERM_SRCS:%.cpp=$(BUILD_DIR)/%.o)
PERM_DEPS := $(PERM_OBJS:%.o=%.d)

recsearch = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

HEADERS := $(call recsearch,src,*.hpp)

CPP_FLAGS := -MMD -MP -Isrc
CXX_FLAGS := -std=c++20 -pedantic -Wall -Wextra -Wshadow -Wconversion -fdiagnostics-color=always
LD_FLAGS :=

CXX ?= clang++
CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifeq ($(findstring clang,$(CXX_VERSION)),)
	$(info WARNING: unsupported compiler, use clang++ instead!)
endif

ifeq ($(OS),Windows_NT)
	DETECTED_OS := windows
else
	DETECTED_OS := $(shell uname)
endif

$(info Detected OS: $(DETECTED_OS))

ifeq ($(DETECTED_OS),windows)
	VERSION := $(shell type version.txt)
else
	VERSION := $(shell cat version.txt)
endif

CPP_FLAGS += -DVERSION=$(VERSION)

$(info Build version: $(VERSION))
$(info Build architecture: $(ARCH))
$(info Build mode: $(MODE))
$(info NUMA support: $(NUMA))

ifeq ($(DETECTED_OS),windows)
	NETWORK_NAME := $(shell type network.txt)
else
	NETWORK_NAME := $(shell cat network.txt)
endif

NETWORK_FILE := $(NETWORK_NAME).nnue
PERMED_NETWORK_FILE := $(NETWORK_NAME)-$(ARCH).nnue

$(info Network: $(NETWORK_NAME))

CPP_FLAGS += -DNETWORK_FILE=$(PERMED_NETWORK_FILE)

ifeq ($(DETECTED_OS),windows)
	MAIN_EXEC := Sift-$(VERSION)-$(ARCH).exe
	PERM_EXEC := permute-$(NETWORK_NAME)-$(ARCH).exe
	MKDIR = mkdir
	RM_FILE = del /f /q
	RM_DIR = rmdir /s /q
	SEP = \\
else
	MAIN_EXEC := Sift-$(VERSION)-$(ARCH)
	PERM_EXEC := permute-$(NETWORK_NAME)-$(ARCH)
	MKDIR = mkdir -p
	RM_FILE = rm -f
	RM_DIR = rm -rf
	SEP = /
endif

ifneq ($(strip $(EXE)),)
	MAIN_EXEC := $(EXE)
endif

PROPERTIES = $(shell echo | $(CXX) -march=native -E -dM -)
ifeq ($(ARCH),native)
	CXX_FLAGS += -march=native
	ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
		ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
			CPP_FLAGS += -DUSE_AVX512
		else ifneq ($(findstring __AVX512VNNI__, $(PROPERTIES)),)
			CPP_FLAGS += -DUSE_AVX512
		endif
	endif
	ifneq ($(findstring __AVX512VNNI__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_VNNI512
	endif
	ifneq ($(findstring __AVX512VBMI2__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_VBMI2
	endif
	ifneq ($(findstring __AVX512VBMI__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_VBMI
	endif
	ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_AVX2
	endif
	ifneq ($(findstring __ARM_NEON, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_NEON
	endif
	ifneq ($(findstring __ARM_FEATURE_DOTPROD, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_NEON_DOTPROD
	endif
	ifneq ($(findstring __BMI2__, $(PROPERTIES)),)
		ifeq ($(findstring __znver1, $(PROPERTIES)),)
			ifeq ($(findstring __znver2, $(PROPERTIES)),)
				CPP_FLAGS += -DUSE_BMI2 -DUSE_PEXT
			endif
		endif
	endif
	ifneq ($(findstring __POPCNT__, $(PROPERTIES)),)
		CPP_FLAGS += -DUSE_POPCNT
	endif
else ifeq ($(ARCH),avx512)
	CXX_FLAGS += -march=icelake-client -mtune=znver4
	CPP_FLAGS += -DUSE_AVX512 -DUSE_VNNI512 -DUSE_VBMI2 -DUSE_VBMI -DUSE_AVX2 -DUSE_BMI2 -DUSE_PEXT -DUSE_POPCNT
else ifeq ($(ARCH),avx2_bmi2)
	CXX_FLAGS += -march=haswell -mtune=znver3
	CPP_FLAGS +=  -DUSE_AVX2 -DUSE_BMI2 -DUSE_PEXT -DUSE_POPCNT
else ifeq ($(ARCH),zen2)
	CXX_FLAGS += -march=bdver4 -mno-tbm -mno-sse4a -mtune=znver2
	CPP_FLAGS += -DUSE_AVX2 -DUSE_POPCNT
else ifeq ($(ARCH),armv8_4)
	CXX_FLAGS += -march=armv8.4-a
	CPP_FLAGS += -DUSE_NEON -DUSE_NEON_DOTPROD
else ifeq ($(ARCH),apple_m1)
	CXX_FLAGS += -mcpu=apple-m1 --target=arm64-apple-macos11
	CPP_FLAGS += -DUSE_NEON -DUSE_NEON_DOTPROD
endif

ifeq ($(DETECTED_OS),windows)
	CXX_FLAGS += -static
else
	CXX_FLAGS += -pthread
	LD_FLAGS  += -pthread
endif

ifeq ($(DETECTED_OS),darwin)
	LD_FLAGS += -fuse-ld=lld
endif

ifeq ($(MODE),release)
	CXX_FLAGS += -O3 -DNDEBUG -funroll-loops
	ifneq ($(DETECTED_OS),windows)
		CXX_FLAGS += -flto
		LD_FLAGS  += -flto
	endif
else ifeq ($(MODE),tune)
	CXX_FLAGS += -O3 -DNDEBUG -funroll-loops
	CPP_FLAGS += -DEXTERNAL_TUNE
	ifneq ($(DETECTED_OS),windows)
		CXX_FLAGS += -flto
		LD_FLAGS  += -flto
	endif
else ifeq ($(MODE),sparsity)
	CXX_FLAGS += -O3 -DNDEBUG -funroll-loops
	CPP_FLAGS += -DMEASURE_SPARSITY
	ifneq ($(DETECTED_OS),windows)
		CXX_FLAGS += -flto
		LD_FLAGS  += -flto
	endif
else ifeq ($(MODE),debug)
	CXX_FLAGS += -O0 -g3 -fsanitize=undefined,address -fno-omit-frame-pointer
	LD_FLAGS  += -fsanitize=undefined,address
endif

ifeq ($(NUMA),on)
	CPP_FLAGS += -DUSE_NUMA
	LD_FLAGS  += -lnuma
endif

-include $(MAIN_DEPS)
-include $(PERM_DEPS)

$(MAIN_EXEC): $(PERMED_NETWORK_FILE) $(MAIN_OBJS)
	$(info Building engine...)
	$(CXX) $(CXX_FLAGS) $(MAIN_OBJS) -o $@ $(LD_FLAGS)

$(PERM_EXEC): $(NETWORK_FILE) $(PERM_OBJS)
	$(info Building tools...)
	$(CXX) $(CXX_FLAGS) $(PERM_OBJS) -o $@ $(LD_FLAGS)

$(PERMED_NETWORK_FILE): $(PERM_EXEC)
	$(info Permuting network...)
	./$(PERM_EXEC) $(NETWORK_FILE) $(PERMED_NETWORK_FILE)

$(NETWORK_FILE):
	$(info Downloading default network...)
	curl -sOL https://github.com/andrewharabor/Sift-Nets/releases/download/$(NETWORK_NAME)/$(NETWORK_FILE)

$(BUILD_DIR)/%.o: %.cpp
	$(MKDIR) "$(subst /,$(SEP),$(dir $@))"
	$(CXX) $(CPP_FLAGS) $(CXX_FLAGS) -c $< -o $@

.DEFAULT_GOAL := main

.PHONY: main
main: $(MAIN_EXEC)

.PHONY: format
format: $(HEADERS) $(MAIN_SRCS) $(PERM_SRCS)
	clang-format -i $^

.PHONY: clean
clean:
	$(RM_FILE) Sift*
	$(RM_FILE) permute-*
	$(RM_DIR) build

.PHONY: help
help:
	@echo "Usage: make <TARGET> <ARCH=[native|avx512|avx2_bmi2|zen2|armv8_4|apple_m1]> <MODE=[release|tune|sparsity|debug]> <NUMA=[off|on]>"
	@echo "Targets:"
	@echo "  main"
	@echo "  format"
	@echo "  clean"
	@echo "  help"
