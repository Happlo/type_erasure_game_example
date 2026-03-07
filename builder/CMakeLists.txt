add_executable(map_builder
    main.cpp
)

target_link_libraries(map_builder PRIVATE
    nlohmann_json::nlohmann_json
    imgui::imgui
    glfw
    OpenGL::GL
    core
)
