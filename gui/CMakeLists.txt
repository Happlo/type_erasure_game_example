find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(game_gui
    main_game.cpp
    shared.cpp
)

add_executable(map_builder
    main_builder.cpp
    shared.cpp
)

target_link_libraries(game_gui PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    core
)

target_link_libraries(map_builder PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    core
)
