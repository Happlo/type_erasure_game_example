cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(type_erasure_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_library(core STATIC
    core/src/game.cpp
    core/src/grid_rules.cpp
    core/src/solution_rules.cpp
    core/src/map_io.cpp
)
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)
target_link_libraries(core PRIVATE nlohmann_json::nlohmann_json)

add_executable(type_erasure
    cli/main.cpp
)
target_link_libraries(type_erasure PRIVATE core)

add_executable(map_builder
    builder/main.cpp
)
target_link_libraries(map_builder PRIVATE
    nlohmann_json::nlohmann_json
    imgui::imgui
    glfw
    OpenGL::GL
    core
)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(core_tests
    test/core_tests.cpp
)
target_link_libraries(core_tests PRIVATE core GTest::gtest_main)
add_test(NAME core_tests COMMAND core_tests)
