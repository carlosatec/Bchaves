cmake_minimum_required(VERSION 3.20)
project(Bchaves VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

option(QCHAVES_ENABLE_NATIVE "Enable aggressive native CPU flags" OFF)
option(QCHAVES_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(QCHAVES_ENABLE_LIBSECP256K1 "Enable optional libsecp256k1 backend" OFF)

add_library(qchaves_common
    src/core/address.cpp
    src/core/base58.cpp
    src/core/secp256k1.cpp
    src/system/checkpoint.cpp
    src/system/cli.cpp
    src/system/format.cpp
    src/system/hardware.cpp
    src/system/targets.cpp
    src/engine/app.cpp
)

target_include_directories(qchaves_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(qchaves_common PRIVATE $<$<CONFIG:Debug>:QCHAVES_DEBUG=1>)

if(QCHAVES_ENABLE_LIBSECP256K1)
    find_path(SECP256K1_INCLUDE_DIR secp256k1.h)
    find_library(SECP256K1_LIBRARY NAMES secp256k1 libsecp256k1)
    if(NOT SECP256K1_INCLUDE_DIR OR NOT SECP256K1_LIBRARY)
        message(FATAL_ERROR "QCHAVES_ENABLE_LIBSECP256K1=ON, mas libsecp256k1 nao foi encontrada")
    endif()
    target_include_directories(qchaves_common PRIVATE ${SECP256K1_INCLUDE_DIR})
    target_link_libraries(qchaves_common PRIVATE ${SECP256K1_LIBRARY})
    target_compile_definitions(qchaves_common PRIVATE QCHAVES_HAS_LIBSECP256K1=1)
endif()

if(MSVC)
    target_compile_options(qchaves_common PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
    if(QCHAVES_WARNINGS_AS_ERRORS)
        target_compile_options(qchaves_common PRIVATE /WX)
    endif()
    if(QCHAVES_ENABLE_NATIVE)
        target_compile_options(qchaves_common PRIVATE /O2 /GL /arch:AVX2)
    endif()
else()
    target_compile_options(qchaves_common PRIVATE -Wall -Wextra -Wpedantic)
    if(QCHAVES_WARNINGS_AS_ERRORS)
        target_compile_options(qchaves_common PRIVATE -Werror)
    endif()
    if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        target_compile_options(qchaves_common PRIVATE -O3 -ffunction-sections -fdata-sections)
        if(QCHAVES_ENABLE_NATIVE)
            target_compile_options(qchaves_common PRIVATE -march=native -mtune=native -flto -fomit-frame-pointer)
            target_link_options(qchaves_common PRIVATE -flto)
        endif()
    endif()
endif()

if(WIN32)
    target_compile_definitions(qchaves_common PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(address src/address_main.cpp)
add_executable(bsgs src/bsgs_main.cpp)
add_executable(kangaroo src/kangaroo_main.cpp)

target_link_libraries(address PRIVATE qchaves_common)
target_link_libraries(bsgs PRIVATE qchaves_common)
target_link_libraries(kangaroo PRIVATE qchaves_common)

if(BUILD_TESTING)
    add_executable(qchaves_tests tests/test_main.cpp)
    target_link_libraries(qchaves_tests PRIVATE qchaves_common)
    target_compile_definitions(qchaves_tests PRIVATE QCHAVES_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME qchaves_tests COMMAND qchaves_tests)
endif()
