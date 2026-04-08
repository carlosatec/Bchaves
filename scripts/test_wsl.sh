#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/wsl"
mkdir -p "${BUILD_DIR}"

CXX="${CXX:-g++}"
CXXFLAGS=(
  -std=c++17
  -O2
  -Wall
  -Wextra
  -Wpedantic
  -DQCHAVES_SOURCE_DIR="\"${ROOT_DIR}\""
  -I"${ROOT_DIR}/src"
)

COMMON_SOURCES=(
  "${ROOT_DIR}/src/core/address.cpp"
  "${ROOT_DIR}/src/core/base58.cpp"
  "${ROOT_DIR}/src/core/secp256k1.cpp"
  "${ROOT_DIR}/src/system/checkpoint.cpp"
  "${ROOT_DIR}/src/system/cli.cpp"
  "${ROOT_DIR}/src/system/format.cpp"
  "${ROOT_DIR}/src/system/hardware.cpp"
  "${ROOT_DIR}/src/system/targets.cpp"
  "${ROOT_DIR}/src/engine/app.cpp"
)

"${CXX}" "${CXXFLAGS[@]}" "${ROOT_DIR}/tests/test_main.cpp" "${COMMON_SOURCES[@]}" -o "${BUILD_DIR}/qchaves_tests"
"${BUILD_DIR}/qchaves_tests"
