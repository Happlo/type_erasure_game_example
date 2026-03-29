#include "core/game.hpp"
#include "core/login.hpp"
#include "shared.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <string>

namespace
{
using type_erasure::gui::load_text_file;
using type_erasure::gui::tile_fill;
using type_erasure::gui::tile_outline;

constexpr ImVec2 kControlWindowPos {24.0f, 24.0f};
constexpr ImVec2 kControlWindowSize {360.0f, 852.0f};
constexpr ImVec2 kBoardWindowPos {408.0f, 24.0f};
constexpr ImVec2 kBoardWindowSize {1008.0f, 852.0f};
constexpr ImVec2 kLoginWindowPos {24.0f, 24.0f};
constexpr ImVec2 kLoginWindowSize {520.0f, 852.0f};
constexpr ImVec2 kHighscoreWindowPos {568.0f, 24.0f};
constexpr ImVec2 kHighscoreWindowSize {848.0f, 852.0f};
constexpr ImVec2 kMoveButtonSize {96.0f, 36.0f};
constexpr ImVec2 kActionButtonSize {150.0f, 38.0f};
constexpr ImVec2 kResetButtonSize {310.0f, 38.0f};
constexpr ImVec2 kLoadButtonSize {150.0f, 36.0f};
constexpr char kDefaultMapPath[] = "maps/try_map.json";

struct AppState
{
    std::unique_ptr<core::LoginView> login {core::LoginView::create()};
    core::User* current_user {nullptr};
    std::unique_ptr<core::Game> game;
    std::string status {"Ready."};
    std::array<char, 256> file_path {};
    std::array<char, 128> username_input {};

    AppState()
    {
        std::snprintf(file_path.data(), file_path.size(), "%s", kDefaultMapPath);
    }
};

std::optional<std::string> load_file(const std::string& path)
{
    std::string content;
    if (!load_text_file(path, content)) return std::nullopt;
    return content;
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

void return_to_login(AppState& app)
{
    app.game.reset();
    app.status = "Choose a map to start a game.";
}

bool has_active_game(const AppState& app)
{
    return static_cast<bool>(app.game);
}

void start_selected_map(AppState& app, const std::string& map_id)
{
    if (app.current_user == nullptr) return;

    try
    {
        app.game = app.current_user->select_map(map_id);
        app.status = "Loaded map: " + map_id;
    }
    catch (const std::exception& ex)
    {
        app.status = std::string("Failed to start map: ") + ex.what();
    }
}

void login_existing_user(AppState& app)
{
    try
    {
        app.current_user = &app.login->login_as_user(app.username_input.data());
        app.status = std::string("Logged in as ") + app.current_user->username();
    }
    catch (const std::exception& ex)
    {
        app.status = std::string("Login failed: ") + ex.what();
    }
}

void create_new_user(AppState& app)
{
    try
    {
        app.current_user = &app.login->create_user(app.username_input.data());
        app.status = std::string("Created user ") + app.current_user->username();
    }
    catch (const std::exception& ex)
    {
        app.status = std::string("Create user failed: ") + ex.what();
    }
}

void load_map(AppState& app, const std::string& path, const bool startup_load = false)
{
    const auto content = load_file(path);
    if (!content.has_value())
    {
        app.status = startup_load ? "Failed to load startup map." : "Failed to load map file.";
        if (startup_load) app.game.reset();
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
        if (startup_load) app.game.reset();
    }
}

void handle_keyboard(AppState& app)
{
    if (!has_active_game(app)) return;

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

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) return_to_login(app);
}

bool action_button(const char* label, const ImVec2 size, core::Game& game, const core::Event event, std::string& status)
{
    if (!ImGui::Button(label, size)) return false;
    return trigger_event(game, event, status);
}

void draw_status_panel(const AppState& app)
{
    if (!has_active_game(app))
    {
        ImGui::TextWrapped("%s", app.status.c_str());
        return;
    }

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
    const float font_size = ctx.tile_size * 0.58f;
    type_erasure::gui::draw_tile_symbol(*ctx.draw_list, cell_min, cell_max, core::symbol_of(cell), font_size);
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
    if (app.current_user != nullptr) ImGui::Text("User: %s", app.current_user->username().c_str());

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
    if (ImGui::Button("Back To Login  [R]", kResetButtonSize)) return_to_login(app);

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

void draw_highscore_rows(const std::vector<core::HighscoreEntry>& highscores)
{
    for (const auto& entry : highscores)
    {
        ImGui::BulletText("%s", entry.username.c_str());
        ImGui::SameLine(240.0f);
        ImGui::Text("%d solved", entry.solved_maps);
    }
}

void draw_solved_map_rows(const core::User& user)
{
    const auto& solved_maps = user.solved_maps();
    if (solved_maps.empty())
    {
        ImGui::TextDisabled("No solved maps yet.");
        return;
    }

    for (const auto& map : solved_maps) ImGui::BulletText("%s", map.display_name.c_str());
}

void draw_map_buttons(AppState& app)
{
    if (app.current_user == nullptr)
    {
        ImGui::TextDisabled("Log in to see the map list.");
        return;
    }

    const auto& maps = app.current_user->available_maps();
    if (maps.empty())
    {
        ImGui::TextDisabled("No maps found in maps/.");
        return;
    }

    for (const auto& map : maps)
    {
        if (ImGui::Button(map.display_name.c_str(), ImVec2(220.0f, 38.0f))) start_selected_map(app, map.map_id);
    }
}

void draw_login_window(AppState& app)
{
    ImGui::SetNextWindowPos(kLoginWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kLoginWindowSize, ImGuiCond_Always);
    ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Create a user or log in, then choose a map to start a game.");
    ImGui::InputText("Username", app.username_input.data(), app.username_input.size());

    if (ImGui::Button("Log In", ImVec2(140.0f, 40.0f))) login_existing_user(app);
    ImGui::SameLine();
    if (ImGui::Button("Create User", ImVec2(140.0f, 40.0f))) create_new_user(app);

    ImGui::Spacing();
    if (app.current_user != nullptr) ImGui::Text("Current user: %s", app.current_user->username().c_str());
    draw_status_panel(app);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Available Maps");
    draw_map_buttons(app);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Solved Maps");
    if (app.current_user != nullptr) draw_solved_map_rows(*app.current_user);
    if (app.current_user == nullptr) ImGui::TextDisabled("No user selected.");

    ImGui::End();
}

void draw_highscore_window(const AppState& app)
{
    ImGui::SetNextWindowPos(kHighscoreWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kHighscoreWindowSize, ImGuiCond_Always);
    ImGui::Begin("Highscores", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Leaderboard");
    ImGui::Separator();
    draw_highscore_rows(app.login->highscore_list());
    ImGui::End();
}

void draw_login_frame(AppState& app)
{
    draw_login_window(app);
    draw_highscore_window(app);
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
    if (has_active_game(app))
    {
        const core::MapView view = app.game->view();
        draw_control_window(app, view);
        if (has_active_game(app)) draw_board_window(view, app.game->solved());
    }
    else
    {
        draw_login_frame(app);
    }
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
    type_erasure::gui::apply_style();

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
