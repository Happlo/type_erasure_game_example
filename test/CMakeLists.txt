find_package(GTest CONFIG REQUIRED)
enable_testing()

add_executable(core_tests
    core_tests.cpp
)

target_link_libraries(core_tests PRIVATE
    core
    GTest::gtest_main
)

add_test(NAME core_tests COMMAND core_tests)
