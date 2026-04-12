# Makefile para Bchaves
# Usage: make address | make bsgs | make kangaroo | make clean

CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
ROOT := $(shell pwd)
BUILD_DIR := $(ROOT)/build

ENGINE_SOURCES = \
	engine/address.cpp \
	engine/bsgs.cpp \
	engine/kangaroo.cpp

COMMON_SOURCES = \
	core/address.cpp \
	core/base58.cpp \
	core/secp256k1.cpp \
	system/checkpoint.cpp \
	system/cli.cpp \
	system/format.cpp \
	system/hardware.cpp \
	system/targets.cpp

COMMON_FLAGS = -I$(ROOT)

# modulos/ - empty templates for address, bsgs, kangaroo (to be implemented)

.PHONY: all address bsgs kangaroo clean

all: address bsgs kangaroo

address: $(BUILD_DIR)/address
bsgs: $(BUILD_DIR)/bsgs
kangaroo: $(BUILD_DIR)/kangaroo

$(BUILD_DIR)/address: modulos/address.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/address.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR)/bsgs: modulos/bsgs.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/bsgs.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR)/kangaroo: modulos/kangaroo.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/kangaroo.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)