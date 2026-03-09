#include "core/map_builder.hpp"
#include "core/game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
bool save_file(const std::string& path, const std::string& content)
{
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

bool load_file(const std::string& path, std::string& content)
{
    std::ifstream in(path);
    if (!in) return false;
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}
}  // namespace

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Type Erasure Map Builder", nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    core::MapBuilder map = core::MapBuilder::make_default();
    core::Brush brush;
    int resize_width = map.view.width;
    int resize_height = map.view.height;

    std::array<char, 256> file_path {};
    std::snprintf(file_path.data(), file_path.size(), "%s", "maps/try_map.json");

    std::string status = "Ready.";

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Tools");

        ImGui::InputInt("Width", &resize_width);
        ImGui::InputInt("Height", &resize_height);
        if (resize_width < 1) resize_width = 1;
        if (resize_height < 1) resize_height = 1;
        if (resize_width > 64) resize_width = 64;
        if (resize_height > 64) resize_height = 64;
        if (ImGui::Button("Resize Map"))
        {
            map.resize(resize_width, resize_height);
            status = "Resized map.";
        }

        ImGui::InputInt("Commits", &map.view.commits_left);
        ImGui::InputInt("Undos", &map.view.undos_left);

        ImGui::Separator();
        ImGui::Text("Brush");
        char symbol_buffer[2] {brush.symbol, '\0'};
        ImGui::InputText("Symbol", symbol_buffer, sizeof(symbol_buffer));
        brush.symbol = symbol_buffer[0] == '\0' ? '*' : symbol_buffer[0];
        ImGui::Checkbox("Pushable", &brush.pushable);
        ImGui::TextUnformatted("Tips: use ^ v < > for player facing, 0..9 for numbers.");

        ImGui::Separator();
        ImGui::InputText("File", file_path.data(), file_path.size());
        if (ImGui::Button("Save JSON"))
        {
            const std::string json_text = map.to_json();
            if (!save_file(file_path.data(), json_text))
            {
                status = "Failed to save file.";
            }
            else
            {
                status = "Saved map JSON.";
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load JSON"))
        {
            std::string text;
            if (!load_file(file_path.data(), text))
            {
                status = "Failed to load file.";
            }
            else
            {
                std::string error;
                auto parsed = core::MapBuilder::from_json(text, error);
                if (!parsed)
                {
                    status = "Invalid map JSON: " + error;
                }
                else
                {
                    map = *parsed;
                    resize_width = map.view.width;
                    resize_height = map.view.height;
                    status = "Loaded map JSON.";
                }
            }
        }

        if (ImGui::Button("Validate with core parser"))
        {
            try
            {
                (void)core::Game::from_json(map.to_json());
                status = "Core parser validation passed.";
            }
            catch (const std::exception& ex)
            {
                status = std::string("Core parser validation failed: ") + ex.what();
            }
        }

        ImGui::TextWrapped("%s", status.c_str());

        ImGui::End();

        ImGui::Begin("Grid");
        for (int y = 0; y < map.view.height; ++y)
        {
            for (int x = 0; x < map.view.width; ++x)
            {
                const core::CellView& cell = map.at(x, y);
                const char glyph = core::MapBuilder::glyph_for_cell(cell);
                char label[32];
                std::snprintf(label, sizeof(label), "%c##%d_%d", glyph, x, y);

                if (ImGui::Button(label, ImVec2(28.0f, 28.0f)))
                {
                    map.apply_brush(x, y, brush);
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    map.clear_cell(x, y);
                }

                if (x + 1 < map.view.width) ImGui::SameLine();
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
