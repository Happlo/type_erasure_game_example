#include "core/game.hpp"
#include "core/map_builder.hpp"
#include "shared.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

namespace
{
using type_erasure::gui::draw_tile_symbol;
using type_erasure::gui::load_text_file;
using type_erasure::gui::save_text_file;
using type_erasure::gui::tile_fill;
using type_erasure::gui::tile_outline;

constexpr ImVec2 kToolsWindowPos {24.0f, 24.0f};
constexpr ImVec2 kToolsWindowSize {360.0f, 852.0f};
constexpr ImVec2 kCanvasWindowPos {408.0f, 24.0f};
constexpr ImVec2 kCanvasWindowSize {1008.0f, 852.0f};

struct BuilderApp
{
    std::unique_ptr<core::MapBuilder> map {core::MapBuilder::create_default()};
    core::Brush brush;
    std::string status {"Ready."};
    int resize_width {map->view().width};
    int resize_height {map->view().height};
    std::array<char, 2> symbol_buffer {brush.symbol, '\0'};
    std::array<char, 256> file_path {};

    BuilderApp()
    {
        std::snprintf(file_path.data(), file_path.size(), "%s", "maps/try_map.json");
    }
};

std::string join_chars(const std::vector<char>& chars)
{
    std::string result;
    result.reserve(chars.size() * 2);
    for (size_t i = 0; i < chars.size(); ++i)
    {
        if (i > 0) result += ' ';
        result.push_back(chars[i]);
    }
    return result;
}

void clamp_size(BuilderApp& app)
{
    app.resize_width = std::clamp(app.resize_width, 1, 64);
    app.resize_height = std::clamp(app.resize_height, 1, 64);
}

void sync_size_from_map(BuilderApp& app)
{
    app.resize_width = app.map->view().width;
    app.resize_height = app.map->view().height;
}

void update_brush_symbol(BuilderApp& app)
{
    app.brush.symbol = app.symbol_buffer[0] == '\0' ? '*' : app.symbol_buffer[0];
}

bool manipulation_is_push(const core::Object::ManipulationLevel manipulation_level)
{
    return manipulation_level == core::Object::ManipulationLevel::Push;
}

bool manipulation_is_pick(const core::Object::ManipulationLevel manipulation_level)
{
    return manipulation_level == core::Object::ManipulationLevel::Pick;
}

void resize_map(BuilderApp& app)
{
    clamp_size(app);
    app.map->resize(app.resize_width, app.resize_height);
    app.status = "Resized map.";
}

void load_map(BuilderApp& app)
{
    std::string text;
    if (!load_text_file(app.file_path.data(), text))
    {
        app.status = "Failed to load file.";
        return;
    }

    std::string error;
    auto parsed = core::MapBuilder::from_json(text, error);
    if (!parsed)
    {
        app.status = "Invalid map JSON: " + error;
        return;
    }

    app.map = std::move(*parsed);
    sync_size_from_map(app);
    app.status = "Loaded map JSON.";
}

void save_map(const BuilderApp& app, std::string& status)
{
    if (!save_text_file(app.file_path.data(), app.map->to_json()))
    {
        status = "Failed to save file.";
        return;
    }

    status = "Saved map JSON.";
}

void validate_map(const BuilderApp& app, std::string& status)
{
    try
    {
        (void)core::Game::from_json(app.map->to_json());
        status = "Core parser validation passed.";
    }
    catch (const std::exception& ex)
    {
        status = std::string("Core parser validation failed: ") + ex.what();
    }
}

void draw_brush_preview(const BuilderApp& app)
{
    const core::CellView preview = [&]() -> core::CellView {
        if (app.brush.symbol == '^' || app.brush.symbol == 'v' || app.brush.symbol == '<' || app.brush.symbol == '>')
        {
            return core::Player {.symbol = app.brush.symbol};
        }
        return core::Object {
            .symbol = app.brush.symbol,
            .manipulation_level = app.brush.manipulation_level};
    }();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 cell_max(origin.x + 72.0f, origin.y + 72.0f);
    draw_list->AddRectFilled(origin, cell_max, tile_fill(preview), 14.0f);
    draw_list->AddRect(origin, cell_max, tile_outline(preview), 14.0f, 0, 3.0f);
    draw_tile_symbol(*draw_list, origin, cell_max, core::symbol_of(preview), 40.0f);
    ImGui::Dummy(ImVec2(72.0f, 72.0f));
}

void draw_tools_window(BuilderApp& app)
{
    ImGui::SetNextWindowPos(kToolsWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kToolsWindowSize, ImGuiCond_Always);
    ImGui::Begin("Builder Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure Builder");
    ImGui::Separator();
    ImGui::TextWrapped("Paint tiles onto the board. Left click applies the current brush. Right click clears a cell.");

    ImGui::Spacing();
    ImGui::Text("Map size: %d x %d", app.map->view().width, app.map->view().height);
    ImGui::InputInt("Width", &app.resize_width);
    ImGui::InputInt("Height", &app.resize_height);
    if (ImGui::Button("Resize Map", ImVec2(150.0f, 38.0f))) resize_map(app);

    ImGui::Spacing();
    int commits_left = app.map->view().commits_left;
    int undos_left = app.map->view().undos_left;
    if (ImGui::InputInt("Commits", &commits_left)) app.map->set_commits_left(commits_left);
    if (ImGui::InputInt("Undos", &undos_left)) app.map->set_undos_left(undos_left);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Brush");
    ImGui::InputText("Symbol", app.symbol_buffer.data(), app.symbol_buffer.size());
    update_brush_symbol(app);
    bool pushable = manipulation_is_push(app.brush.manipulation_level);
    bool pickable = manipulation_is_pick(app.brush.manipulation_level);
    if (ImGui::Checkbox("Pushable", &pushable))
    {
        app.brush.manipulation_level = pushable ? core::Object::ManipulationLevel::Push
                                                : core::Object::ManipulationLevel::None;
    }
    if (ImGui::Checkbox("Pickable", &pickable))
    {
        app.brush.manipulation_level = pickable ? core::Object::ManipulationLevel::Pick
                                                : core::Object::ManipulationLevel::None;
    }
    draw_brush_preview(app);
    ImGui::TextWrapped("Tips: use ^ v < > for player facing, 0..9 for numbers.");
    const std::string solver_operators = join_chars(core::MapBuilder::solver_operators());
    const std::string solver_separators = join_chars(core::MapBuilder::equation_delimiters());
    ImGui::TextWrapped("Solver operators: %s", solver_operators.c_str());
    ImGui::TextWrapped("Solver separators: %s", solver_separators.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::InputText("File", app.file_path.data(), app.file_path.size());
    if (ImGui::Button("Save JSON", ImVec2(140.0f, 36.0f))) save_map(app, app.status);
    ImGui::SameLine();
    if (ImGui::Button("Load JSON", ImVec2(140.0f, 36.0f))) load_map(app);
    if (ImGui::Button("Validate", ImVec2(140.0f, 36.0f))) validate_map(app, app.status);

    ImGui::Spacing();
    ImGui::TextWrapped("%s", app.status.c_str());

    ImGui::End();
}

void draw_map_tile(ImDrawList& draw_list, BuilderApp& app, const float tile_size, const ImVec2 origin, const int x,
                   const int y)
{
    const core::CellView& cell = app.map->at(x, y);
    const ImVec2 cell_min(origin.x + x * tile_size, origin.y + y * tile_size);
    const ImVec2 cell_max(cell_min.x + tile_size - 4.0f, cell_min.y + tile_size - 4.0f);
    draw_list.AddRectFilled(cell_min, cell_max, tile_fill(cell), 12.0f);
    draw_list.AddRect(cell_min, cell_max, tile_outline(cell), 12.0f, 0, 2.0f);
    draw_tile_symbol(draw_list, cell_min, cell_max, core::symbol_of(cell), tile_size * 0.58f);

    ImGui::SetCursorScreenPos(cell_min);
    ImGui::InvisibleButton(("cell_" + std::to_string(x) + "_" + std::to_string(y)).c_str(), ImVec2(tile_size, tile_size));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        app.map->apply_brush(x, y, app.brush);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        app.map->clear_cell(x, y);
    }
}

void draw_map_row(ImDrawList& draw_list, BuilderApp& app, const float tile_size, const ImVec2 origin, const int y)
{
    for (int x = 0; x < app.map->view().width; ++x) draw_map_tile(draw_list, app, tile_size, origin, x, y);
}

void draw_canvas_window(BuilderApp& app)
{
    ImGui::SetNextWindowPos(kCanvasWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kCanvasWindowSize, ImGuiCond_Always);
    ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const auto& view = app.map->view();
    const float tile_size = std::clamp(560.0f / static_cast<float>(std::max(view.width, view.height)), 34.0f, 72.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(tile_size * view.width, tile_size * view.height);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + total_size.x, origin.y + total_size.y), IM_COL32(20, 24, 29, 255), 18.0f);

    for (int y = 0; y < view.height; ++y) draw_map_row(*draw_list, app, tile_size, origin, y);

    ImGui::Dummy(ImVec2(total_size.x, total_size.y));
    ImGui::End();
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

    GLFWwindow* window = glfwCreateWindow(1440, 900, "Type Erasure Builder", nullptr, nullptr);
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
    type_erasure::gui::apply_style();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    BuilderApp app;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_tools_window(app);
        draw_canvas_window(app);

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.07f, 0.09f, 0.11f, 1.0f);
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
