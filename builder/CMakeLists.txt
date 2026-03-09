find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(map_builder
    main.cpp
)

target_link_libraries(map_builder PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    core
)
