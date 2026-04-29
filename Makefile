# Bchaves: Bitcoin Performance Engine
# 
# Descrição: Sistema de build multi-alvo para Windows e Linux.
# 
# Repository: https://github.com/carlosatec/Bchaves
# Author:     Carlos
# License:    MIT (c) 2026

CXX ?= g++
CXXFLAGS = -std=c++17 -O3 -flto -Wall -Wextra
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),aarch64)
  CXXFLAGS += -march=armv8.2-a+crypto
else
  CXXFLAGS += -march=native
endif
ROOT := $(shell pwd)
BUILD_DIR := $(ROOT)/build

ENGINE_SOURCES = \
	engine/address.cpp \
	engine/bsgs.cpp \
	engine/kangaroo.cpp \
	engine/app.cpp

COMMON_SOURCES = \
	core/address.cpp \
	core/base58.cpp \
	core/hash.cpp \
	core/secp256k1.cpp \
	system/checkpoint.cpp \
	system/cli.cpp \
	system/format.cpp \
	system/hardware.cpp \
	system/targets.cpp \
	system/io.cpp

COMMON_FLAGS = -I$(ROOT)

# modulos/ - empty templates for address, bsgs, kangaroo (to be implemented)

.PHONY: all address bsgs kangaroo test clean

all: address bsgs kangaroo

address: $(BUILD_DIR)/address
bsgs: $(BUILD_DIR)/bsgs
kangaroo: $(BUILD_DIR)/kangaroo
test: $(BUILD_DIR)/crypto_test
	$(BUILD_DIR)/crypto_test

$(BUILD_DIR)/address: modulos/address.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/address.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR)/bsgs: modulos/bsgs.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/bsgs.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR)/kangaroo: modulos/kangaroo.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) modulos/kangaroo.cpp $(ENGINE_SOURCES) $(COMMON_SOURCES) -o $@

$(BUILD_DIR)/crypto_test: tests/crypto_test.cpp $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(COMMON_FLAGS) tests/crypto_test.cpp $(COMMON_SOURCES) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
