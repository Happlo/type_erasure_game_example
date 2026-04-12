find_package(nlohmann_json CONFIG REQUIRED)

add_library(core STATIC 
    src/game.cpp
    src/game_model.cpp
    src/grid_rules.cpp
    src/object.cpp
    src/player.cpp
    src/solution_rules.cpp
    src/map_io.cpp
    src/map_builder.cpp
    src/login.cpp)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(core PRIVATE nlohmann_json::nlohmann_json)
