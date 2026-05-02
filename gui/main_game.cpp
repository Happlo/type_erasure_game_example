#include "core/game.hpp"
#include "core/login.hpp"
#include "shared.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
using type_erasure::gui::GamePlayState;
using type_erasure::gui::load_text_file;

constexpr ImVec2 kControlWindowPos{24.0f, 24.0f};
constexpr ImVec2 kControlWindowSize{360.0f, 852.0f};
constexpr ImVec2 kBoardWindowPos{408.0f, 24.0f};
constexpr ImVec2 kBoardWindowSize{1008.0f, 852.0f};
constexpr ImVec2 kLoginWindowPos{24.0f, 24.0f};
constexpr ImVec2 kLoginWindowSize{520.0f, 852.0f};
constexpr ImVec2 kHighscoreWindowPos{568.0f, 24.0f};
constexpr ImVec2 kHighscoreWindowSize{848.0f, 852.0f};
constexpr ImVec2 kResetButtonSize{310.0f, 38.0f};
constexpr ImVec2 kLoadButtonSize{150.0f, 36.0f};
constexpr char kDefaultMapPath[] = "maps/try_map.json";

struct AppState
{
    std::unique_ptr<core::LoginView> login{core::LoginView::create()};
    core::User *current_user{nullptr};
    GamePlayState play;
    std::array<char, 256> file_path{};
    std::array<char, 128> username_input{};

    AppState() { std::snprintf(file_path.data(), file_path.size(), "%s", kDefaultMapPath); }
};

void return_to_login(AppState &app)
{
    type_erasure::gui::clear_game(app.play, "Choose a map to start a game.");
}

void start_selected_map(AppState &app, const std::string &map_id)
{
    if (app.current_user == nullptr)
        return;

    try
    {
        type_erasure::gui::start_game(app.play, app.current_user->select_map(map_id),
                                      "Loaded map: " + map_id);
    }
    catch (const std::exception &ex)
    {
        app.play.status = std::string("Failed to start map: ") + ex.what();
    }
}

void login_existing_user(AppState &app)
{
    try
    {
        app.current_user = &app.login->login_as_user(app.username_input.data());
        app.play.status = std::string("Logged in as ") + app.current_user->username();
    }
    catch (const std::exception &ex)
    {
        app.play.status = std::string("Login failed: ") + ex.what();
    }
}

void create_new_user(AppState &app)
{
    try
    {
        app.current_user = &app.login->create_user(app.username_input.data());
        app.play.status = std::string("Created user ") + app.current_user->username();
    }
    catch (const std::exception &ex)
    {
        app.play.status = std::string("Create user failed: ") + ex.what();
    }
}

void load_map(AppState &app, const std::string &path, const bool startup_load = false)
{
    std::string content;
    if (!load_text_file(path, content))
    {
        app.play.status = startup_load ? "Failed to load startup map." : "Failed to load map file.";
        if (startup_load)
            type_erasure::gui::clear_game(app.play, app.play.status);
        return;
    }

    try
    {
        type_erasure::gui::start_game(
            app.play, core::Game::from_json(content),
            startup_load ? std::string("Loaded map: ") + path : "Loaded map JSON.");
    }
    catch (const std::exception &ex)
    {
        app.play.status =
            std::string(startup_load ? "Invalid startup map: " : "Invalid map JSON: ") + ex.what();
        if (startup_load)
            type_erasure::gui::clear_game(app.play, app.play.status);
    }
}

void handle_keyboard(AppState &app)
{
    type_erasure::gui::handle_game_keyboard(app.play);
    if (ImGui::IsKeyPressed(ImGuiKey_R, false))
        return_to_login(app);
}

void draw_control_window(AppState &app, const core::MapView &view)
{
    ImGui::SetNextWindowPos(kControlWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kControlWindowSize, ImGuiCond_Always);
    ImGui::Begin("Control Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Make the row containing '=' become a true equation.");
    if (app.current_user != nullptr)
        ImGui::Text("User: %s", app.current_user->username().c_str());

    ImGui::Spacing();
    type_erasure::gui::draw_game_sidebar_state(app.play, view);

    ImGui::Spacing();
    type_erasure::gui::draw_game_action_controls(app.play);

    ImGui::Spacing();
    if (ImGui::Button("Back To Login  [R]", kResetButtonSize))
        return_to_login(app);

    ImGui::InputText("Load map", app.file_path.data(), app.file_path.size());
    if (ImGui::Button("Load JSON", kLoadButtonSize))
        load_map(app, app.file_path.data());

    ImGui::Spacing();
    type_erasure::gui::draw_game_status(app.play);

    ImGui::End();
}

void draw_board_window(const core::MapView &view,
                       const std::optional<core::EquationResult> &result)
{
    ImGui::SetNextWindowPos(kBoardWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kBoardWindowSize, ImGuiCond_Always);
    ImGui::Begin("Board", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    type_erasure::gui::draw_game_grid(view, result);
    ImGui::End();
}

void draw_highscore_rows(const std::vector<core::HighscoreEntry> &highscores)
{
    for (const auto &entry : highscores)
    {
        ImGui::BulletText("%s", entry.username.c_str());
        ImGui::SameLine(240.0f);
        ImGui::Text("%d solved", entry.solved_maps);
    }
}

void draw_solved_map_rows(const core::User &user)
{
    const auto &solved_maps = user.solved_maps();
    if (solved_maps.empty())
    {
        ImGui::TextDisabled("No solved maps yet.");
        return;
    }

    for (const auto &map : solved_maps)
        ImGui::BulletText("%s", map.display_name.c_str());
}

void draw_map_buttons(AppState &app)
{
    if (app.current_user == nullptr)
    {
        ImGui::TextDisabled("Log in to see the map list.");
        return;
    }

    const auto &maps = app.current_user->available_maps();
    if (maps.empty())
    {
        ImGui::TextDisabled("No maps found in maps/.");
        return;
    }

    for (const auto &map : maps)
    {
        if (ImGui::Button(map.display_name.c_str(), ImVec2(220.0f, 38.0f)))
            start_selected_map(app, map.map_id);
    }
}

void draw_login_window(AppState &app)
{
    ImGui::SetNextWindowPos(kLoginWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kLoginWindowSize, ImGuiCond_Always);
    ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Create a user or log in, then choose a map to start a game.");
    ImGui::InputText("Username", app.username_input.data(), app.username_input.size());

    if (ImGui::Button("Log In", ImVec2(140.0f, 40.0f)))
        login_existing_user(app);
    ImGui::SameLine();
    if (ImGui::Button("Create User", ImVec2(140.0f, 40.0f)))
        create_new_user(app);

    ImGui::Spacing();
    if (app.current_user != nullptr)
        ImGui::Text("Current user: %s", app.current_user->username().c_str());
    type_erasure::gui::draw_game_status(app.play);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Available Maps");
    draw_map_buttons(app);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Solved Maps");
    if (app.current_user != nullptr)
        draw_solved_map_rows(*app.current_user);
    if (app.current_user == nullptr)
        ImGui::TextDisabled("No user selected.");

    ImGui::End();
}

void draw_highscore_window(const AppState &app)
{
    ImGui::SetNextWindowPos(kHighscoreWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kHighscoreWindowSize, ImGuiCond_Always);
    ImGui::Begin("Highscores", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Leaderboard");
    ImGui::Separator();
    draw_highscore_rows(app.login->highscore_list());
    ImGui::End();
}

void draw_login_frame(AppState &app)
{
    draw_login_window(app);
    draw_highscore_window(app);
}

void render_frame(GLFWwindow *window)
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

void draw_frame(GLFWwindow *window, AppState &app)
{
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    handle_keyboard(app);
    if (app.play.game)
    {
        const core::MapView view = app.play.game->view();
        draw_control_window(app, view);
        if (app.play.game)
            draw_board_window(view, app.play.equation_result);
    }
    else
    {
        draw_login_frame(app);
    }
    render_frame(window);
}
} // namespace

int main(int argc, char **argv)
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window = glfwCreateWindow(1440, 900, "Type Erasure", nullptr, nullptr);
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
    if (argc == 2)
        load_map(app, argv[1], true);

    while (!glfwWindowShouldClose(window))
        draw_frame(window, app);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
