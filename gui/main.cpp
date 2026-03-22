#include "core/game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
constexpr ImVec2 kControlWindowPos {24.0f, 24.0f};
constexpr ImVec2 kControlWindowSize {360.0f, 852.0f};
constexpr ImVec2 kBoardWindowPos {408.0f, 24.0f};
constexpr ImVec2 kBoardWindowSize {1008.0f, 852.0f};
constexpr ImVec2 kMoveButtonSize {96.0f, 36.0f};
constexpr ImVec2 kActionButtonSize {150.0f, 38.0f};
constexpr ImVec2 kResetButtonSize {310.0f, 38.0f};
constexpr ImVec2 kLoadButtonSize {150.0f, 36.0f};
constexpr char kDefaultMapPath[] = "maps/try_map.json";

struct AppState
{
    std::unique_ptr<core::Game> game {core::Game::create_default()};
    std::string status {"Ready."};
    std::array<char, 256> file_path {};

    AppState()
    {
        std::snprintf(file_path.data(), file_path.size(), "%s", kDefaultMapPath);
    }
};

std::optional<std::string> load_file(const std::string& path)
{
    std::ifstream input(path);
    if (!input) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

ImVec4 color_from_rgb(const int r, const int g, const int b)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

void apply_style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.TabRounding = 10.0f;
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_ChildBg] = color_from_rgb(24, 29, 35);
    colors[ImGuiCol_PopupBg] = color_from_rgb(24, 29, 35);
    colors[ImGuiCol_FrameBg] = color_from_rgb(36, 43, 51);
    colors[ImGuiCol_FrameBgHovered] = color_from_rgb(52, 61, 73);
    colors[ImGuiCol_FrameBgActive] = color_from_rgb(67, 79, 94);
    colors[ImGuiCol_TitleBg] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_TitleBgActive] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_Button] = color_from_rgb(180, 91, 63);
    colors[ImGuiCol_ButtonHovered] = color_from_rgb(205, 113, 81);
    colors[ImGuiCol_ButtonActive] = color_from_rgb(151, 72, 48);
    colors[ImGuiCol_Header] = color_from_rgb(45, 54, 64);
    colors[ImGuiCol_HeaderHovered] = color_from_rgb(63, 73, 85);
    colors[ImGuiCol_HeaderActive] = color_from_rgb(78, 90, 105);
    colors[ImGuiCol_Text] = color_from_rgb(235, 233, 225);
    colors[ImGuiCol_TextDisabled] = color_from_rgb(143, 148, 154);
    colors[ImGuiCol_Border] = color_from_rgb(62, 68, 76);
}

ImU32 tile_fill(const core::CellView& cell)
{
    return std::visit(
        [](const auto& value) -> ImU32
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, core::Empty>)
            {
                return IM_COL32(31, 37, 43, 255);
            }
            else if constexpr (std::is_same_v<T, core::Player>)
            {
                return IM_COL32(236, 177, 79, 255);
            }
            else
            {
                if (value.symbol == '=' || value.symbol == '+') return IM_COL32(108, 167, 124, 255);
                if (value.is_pickable) return IM_COL32(97, 147, 196, 255);
                if (value.is_pushable) return IM_COL32(189, 112, 143, 255);
                return IM_COL32(116, 123, 132, 255);
            }
        },
        cell);
}

ImU32 tile_outline(const core::CellView& cell)
{
    return std::visit(
        [](const auto& value) -> ImU32
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, core::Player>)
            {
                return IM_COL32(255, 226, 157, 255);
            }
            else if constexpr (std::is_same_v<T, core::Object>)
            {
                if (value.symbol == '=' || value.symbol == '+') return IM_COL32(170, 223, 170, 255);
            }
            return IM_COL32(72, 79, 88, 255);
        },
        cell);
}

bool trigger_event(core::Game& game, const core::Event event, std::string& status)
{
    const bool applied = game.apply_event(event);
    if (game.solved())
    {
        status = "Equation solved.";
        return true;
    }

    if (!applied)
    {
        status = "That action is blocked.";
        return false;
    }

    switch (event)
    {
        case core::Event::MoveUp:
        case core::Event::MoveLeft:
        case core::Event::MoveDown:
        case core::Event::MoveRight:
            status = "Moved.";
            break;
        case core::Event::PickItem:
            status = "Picked up item.";
            break;
        case core::Event::DropItem:
            status = "Dropped item.";
            break;
        case core::Event::Commit:
            status = "Committed map state.";
            break;
        case core::Event::Undo:
            status = "Restored previous commit.";
            break;
    }

    return true;
}

struct GridRenderContext
{
    ImDrawList* draw_list;
    ImVec2 origin;
    float tile_size;
    bool solved;
};

void reset_to_default_map(AppState& app)
{
    app.game = core::Game::create_default();
    app.status = "Reset to default map.";
}

void load_map(AppState& app, const std::string& path, const bool startup_load = false)
{
    const auto content = load_file(path);
    if (!content.has_value())
    {
        app.status = startup_load ? "Failed to load startup map." : "Failed to load map file.";
        if (startup_load) app.game = core::Game::create_default();
        return;
    }

    try
    {
        app.game = core::Game::from_json(*content);
        app.status = startup_load ? std::string("Loaded map: ") + path : "Loaded map JSON.";
    }
    catch (const std::exception& ex)
    {
        app.status = std::string(startup_load ? "Invalid startup map: " : "Invalid map JSON: ") + ex.what();
        if (startup_load) app.game = core::Game::create_default();
    }
}

void handle_keyboard(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    struct KeyBinding
    {
        ImGuiKey primary;
        ImGuiKey alternate;
        core::Event event;
    };

    constexpr std::array<KeyBinding, 8> bindings {{
        {ImGuiKey_W, ImGuiKey_UpArrow, core::Event::MoveUp},
        {ImGuiKey_A, ImGuiKey_LeftArrow, core::Event::MoveLeft},
        {ImGuiKey_S, ImGuiKey_DownArrow, core::Event::MoveDown},
        {ImGuiKey_D, ImGuiKey_RightArrow, core::Event::MoveRight},
        {ImGuiKey_E, ImGuiKey_None, core::Event::PickItem},
        {ImGuiKey_Q, ImGuiKey_None, core::Event::DropItem},
        {ImGuiKey_C, ImGuiKey_None, core::Event::Commit},
        {ImGuiKey_U, ImGuiKey_None, core::Event::Undo},
    }};

    for (const auto& binding : bindings)
    {
        if (ImGui::IsKeyPressed(binding.primary, false) ||
            (binding.alternate != ImGuiKey_None && ImGui::IsKeyPressed(binding.alternate, false)))
        {
            trigger_event(*app.game, binding.event, app.status);
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) reset_to_default_map(app);
}

bool action_button(const char* label, const ImVec2 size, core::Game& game, const core::Event event, std::string& status)
{
    if (!ImGui::Button(label, size)) return false;
    return trigger_event(game, event, status);
}

void draw_status_panel(const AppState& app)
{
    if (app.game->solved())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.91f, 0.67f, 1.0f));
        ImGui::TextWrapped("Solved. Load another map or reset to play again.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextWrapped("Controls: WASD or arrow keys move. E picks up, Q drops, C commits, U undoes.");
    }
    ImGui::TextWrapped("%s", app.status.c_str());
}

void draw_inventory(const core::MapView& view)
{
    ImGui::TextUnformatted("Inventory");
    if (!view.player.has_value() || view.player->inventory.empty())
    {
        ImGui::TextDisabled("Empty");
        return;
    }

    for (size_t i = 0; i < view.player->inventory.size(); ++i)
    {
        const auto& item = view.player->inventory[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.58f, 0.77f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.46f, 0.67f, 0.87f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.31f, 0.50f, 0.69f, 1.0f));
        char label[8];
        std::snprintf(label, sizeof(label), "%c", item.symbol);
        ImGui::Button(label, ImVec2(36.0f, 32.0f));
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        if (i + 1 < view.player->inventory.size()) ImGui::SameLine();
    }
}

void draw_tile_symbol(const GridRenderContext& ctx, const core::CellView& cell, const ImVec2 cell_min, const ImVec2 cell_max)
{
    const char symbol[2] = {core::symbol_of(cell), '\0'};
    ImFont* font = ImGui::GetFont();
    const float font_size = ctx.tile_size * 0.58f;
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, symbol);
    const ImVec2 text_pos(
        cell_min.x + ((cell_max.x - cell_min.x) - text_size.x) * 0.5f,
        cell_min.y + ((cell_max.y - cell_min.y) - text_size.y) * 0.5f);
    ctx.draw_list->AddText(font, font_size, text_pos, IM_COL32(245, 242, 233, 255), symbol);
}

void draw_tile(const GridRenderContext& ctx, const core::CellView& cell, const int x, const int y)
{
    const ImVec2 cell_min(ctx.origin.x + x * ctx.tile_size, ctx.origin.y + y * ctx.tile_size);
    const ImVec2 cell_max(cell_min.x + ctx.tile_size - 4.0f, cell_min.y + ctx.tile_size - 4.0f);

    ctx.draw_list->AddRectFilled(cell_min, cell_max, tile_fill(cell), 12.0f);
    ctx.draw_list->AddRect(
        cell_min,
        cell_max,
        ctx.solved ? IM_COL32(182, 240, 175, 255) : tile_outline(cell),
        12.0f,
        0,
        ctx.solved ? 3.0f : 2.0f);
    draw_tile_symbol(ctx, cell, cell_min, cell_max);
}

void draw_grid_row(const GridRenderContext& ctx, const core::MapView& view, const int y)
{
    for (int x = 0; x < view.width; ++x) draw_tile(ctx, view.at(x, y), x, y);
}

void draw_grid_cells(const GridRenderContext& ctx, const core::MapView& view)
{
    for (int y = 0; y < view.height; ++y) draw_grid_row(ctx, view, y);
}

void draw_solved_badge(ImDrawList& draw_list, const ImVec2 origin)
{
    const char* message = "Equation Solved";
    ImFont* font = ImGui::GetFont();
    const float font_size = 30.0f;
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, message);
    const ImVec2 badge_min(origin.x + 18.0f, origin.y + 18.0f);
    const ImVec2 badge_max(badge_min.x + text_size.x + 28.0f, badge_min.y + text_size.y + 18.0f);
    const ImVec2 text_pos(badge_min.x + 14.0f, badge_min.y + 9.0f);
    draw_list.AddRectFilled(badge_min, badge_max, IM_COL32(97, 181, 92, 235), 12.0f);
    draw_list.AddText(font, font_size, text_pos, IM_COL32(250, 255, 247, 255), message);
}

void draw_grid_background(ImDrawList& draw_list, const ImVec2 origin, const ImVec2 board_max, const bool solved)
{
    draw_list.AddRectFilled(origin, board_max, solved ? IM_COL32(23, 43, 30, 255) : IM_COL32(20, 24, 29, 255), 18.0f);
    if (!solved) return;

    draw_list.AddRect(origin, board_max, IM_COL32(137, 224, 151, 255), 18.0f, 0, 4.0f);
    draw_list.AddRect(origin, board_max, IM_COL32(195, 255, 204, 120), 18.0f, 0, 10.0f);
}

void draw_grid(const core::MapView& view, const bool solved)
{
    const float tile_size = std::clamp(560.0f / static_cast<float>(std::max(view.width, view.height)), 36.0f, 72.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(tile_size * view.width, tile_size * view.height);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 board_max(origin.x + total_size.x, origin.y + total_size.y);
    const GridRenderContext ctx {draw_list, origin, tile_size, solved};

    draw_grid_background(*draw_list, origin, board_max, solved);
    draw_grid_cells(ctx, view);
    if (solved) draw_solved_badge(*draw_list, origin);

    ImGui::Dummy(ImVec2(total_size.x, total_size.y));
}

void draw_control_window(AppState& app, const core::MapView& view)
{
    ImGui::SetNextWindowPos(kControlWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kControlWindowSize, ImGuiCond_Always);
    ImGui::Begin("Control Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Make the row containing '=' become a true equation.");

    ImGui::Spacing();
    ImGui::Text("Map commits: %d", view.commits_left);
    ImGui::Text("Map undos: %d", view.undos_left);
    draw_inventory(view);

    ImGui::Spacing();
    ImGui::TextUnformatted("Movement");
    action_button("Up", kMoveButtonSize, *app.game, core::Event::MoveUp, app.status);
    action_button("Left", kMoveButtonSize, *app.game, core::Event::MoveLeft, app.status);
    ImGui::SameLine();
    action_button("Down", kMoveButtonSize, *app.game, core::Event::MoveDown, app.status);
    ImGui::SameLine();
    action_button("Right", kMoveButtonSize, *app.game, core::Event::MoveRight, app.status);

    ImGui::Spacing();
    action_button("Pick  [E]", kActionButtonSize, *app.game, core::Event::PickItem, app.status);
    ImGui::SameLine();
    action_button("Drop  [Q]", kActionButtonSize, *app.game, core::Event::DropItem, app.status);
    action_button("Commit  [C]", kActionButtonSize, *app.game, core::Event::Commit, app.status);
    ImGui::SameLine();
    action_button("Undo  [U]", kActionButtonSize, *app.game, core::Event::Undo, app.status);

    ImGui::Spacing();
    if (ImGui::Button("Reset Default  [R]", kResetButtonSize)) reset_to_default_map(app);

    ImGui::InputText("Load map", app.file_path.data(), app.file_path.size());
    if (ImGui::Button("Load JSON", kLoadButtonSize)) load_map(app, app.file_path.data());

    ImGui::Spacing();
    draw_status_panel(app);

    ImGui::End();
}

void draw_board_window(const core::MapView& view, const bool solved)
{
    ImGui::SetNextWindowPos(kBoardWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kBoardWindowSize, ImGuiCond_Always);
    ImGui::Begin("Board", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    draw_grid(view, solved);
    ImGui::End();
}

void render_frame(GLFWwindow* window)
{
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

void draw_frame(GLFWwindow* window, AppState& app)
{
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    handle_keyboard(app);
    const core::MapView view = app.game->view();

    draw_control_window(app, view);
    draw_board_window(view, app.game->solved());
    render_frame(window);
}
}  // namespace

int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1440, 900, "Type Erasure", nullptr, nullptr);
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
    apply_style();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AppState app;
    if (argc == 2) load_map(app, argv[1], true);

    while (!glfwWindowShouldClose(window)) draw_frame(window, app);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
