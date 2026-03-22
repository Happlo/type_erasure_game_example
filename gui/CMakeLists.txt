find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(game_gui
    main.cpp
)

target_link_libraries(game_gui PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    core
)
