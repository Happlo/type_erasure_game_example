cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(type_erasure_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(core STATIC
    core/src/game.cpp
)
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

add_executable(type_erasure
    cli/main.cpp
)
target_link_libraries(type_erasure PRIVATE core)

enable_testing()
add_executable(core_tests
    test/core_tests.cpp
)
target_link_libraries(core_tests PRIVATE core)
add_test(NAME core_tests COMMAND core_tests)
