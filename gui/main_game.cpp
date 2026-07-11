#include "builder_editor.hpp"
#include "core/game.hpp"
#include "core/login.hpp"
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
#include <optional>
#include <string>
#include <vector>

namespace
{
using gui::GamePlayState;

constexpr ImVec2 kControlWindowPos{24.0f, 24.0f};
constexpr ImVec2 kControlWindowSize{360.0f, 852.0f};
constexpr ImVec2 kBoardWindowPos{408.0f, 24.0f};
constexpr ImVec2 kBoardWindowSize{1008.0f, 852.0f};
constexpr ImVec2 kLoginWindowPos{24.0f, 24.0f};
constexpr ImVec2 kLoginWindowSize{520.0f, 852.0f};
constexpr ImVec2 kHighscoreWindowPos{568.0f, 24.0f};
constexpr ImVec2 kHighscoreWindowSize{848.0f, 852.0f};
constexpr ImVec2 kResetButtonSize{310.0f, 38.0f};

struct AppState
{
    std::unique_ptr<core::LoginView> login{core::LoginView::create()};
    core::User *current_user{nullptr};
    GamePlayState play;
    gui::BuilderEditorState builder;
    std::array<char, 128> username_input{};
};

void return_to_login(AppState &app)
{
    gui::clear_game(app.play, "Choose a map to start a game.");
    gui::clear_builder_editor(app.builder);
}

void start_selected_map(AppState &app, const std::string &map_id)
{
    if (app.current_user == nullptr)
        return;

    try
    {
        gui::start_game(app.play, app.current_user->select_map(map_id),
                                      "Loaded map: " + map_id);
    }
    catch (const std::exception &ex)
    {
        app.play.status = std::string("Failed to start map: ") + ex.what();
    }
}

void create_new_map(AppState &app)
{
    if (app.current_user == nullptr)
        return;

    try
    {
        gui::clear_game(app.play, "Choose a map to start a game.");
        gui::start_builder_editor(app.builder, app.current_user->create_new_map(), "new_map.json",
                                  "Editing new map.");
    }
    catch (const std::exception &ex)
    {
        app.play.status = std::string("Create map failed: ") + ex.what();
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

void handle_keyboard(AppState &app)
{
    if (gui::builder_editor_active(app.builder))
        gui::handle_builder_editor_keyboard(app.builder);
    else
    {
        gui::handle_game_keyboard(app.play);
        if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            return_to_login(app);
    }
}

void draw_control_window(AppState &app, const core::MapView &view)
{
    ImGui::SetNextWindowPos(gui::scaled(kControlWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(gui::scaled(kControlWindowSize), ImGuiCond_Always);
    ImGui::Begin("Control Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Make the row containing '=' become a true equation.");
    if (app.current_user != nullptr)
        ImGui::Text("User: %s", app.current_user->username().c_str());

    ImGui::Spacing();
    gui::draw_game_sidebar_state(app.play, view);

    ImGui::Spacing();
    gui::draw_game_action_controls(app.play);

    ImGui::Spacing();
    if (ImGui::Button("Back To Login  [R]", gui::scaled(kResetButtonSize)))
        return_to_login(app);

    gui::draw_game_status(app.play);

    ImGui::End();
}

void draw_board_window(const core::MapView &view,
                       const std::optional<core::GameResult> &result)
{
    ImGui::SetNextWindowPos(gui::scaled(kBoardWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(gui::scaled(kBoardWindowSize), ImGuiCond_Always);
    ImGui::Begin("Board", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    gui::draw_game_grid(view, result);
    ImGui::End();
}

void draw_highscore_rows(const std::vector<core::HighscoreEntry> &highscores)
{
    for (const auto &entry : highscores)
    {
        ImGui::BulletText("%s", entry.username.c_str());
        ImGui::SameLine(gui::scaled(240.0f));
        ImGui::Text("%d solved", entry.solved_maps);
    }
}

void draw_map_buttons(AppState &app)
{
    if (app.current_user == nullptr)
    {
        ImGui::TextDisabled("Log in to see the map list.");
        return;
    }

    const auto &maps = app.current_user->available_maps();
    const auto &solved_maps = app.current_user->solved_maps();
    if (maps.empty())
    {
        ImGui::TextDisabled("No maps found in maps/.");
        return;
    }

    for (const auto &map : maps)
    {
        // Check if this map is in the solved maps list
        const bool is_solved = std::any_of(solved_maps.begin(), solved_maps.end(),
                                           [&map](const core::MapEntry &solved) {
                                               return solved.map_id == map.map_id;
                                           });

        // Change button color if solved
        if (is_solved)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(76, 175, 80, 255));         // Green
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));

        const std::string label =
            (map.display_name.empty() ? "(unnamed map)" : map.display_name) + "###map_" + map.map_id;
        if (ImGui::Button(label.c_str(), gui::scaled(ImVec2(220.0f, 38.0f))))
            start_selected_map(app, map.map_id);

        ImGui::PopStyleColor();
    }
}

void draw_login_window(AppState &app)
{
    ImGui::SetNextWindowPos(gui::scaled(kLoginWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(gui::scaled(kLoginWindowSize), ImGuiCond_Always);
    ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Create a user or log in, then choose a map to start a game.");
    ImGui::InputText("Username", app.username_input.data(), app.username_input.size());

    if (ImGui::Button("Log In", gui::scaled(ImVec2(140.0f, 40.0f))))
        login_existing_user(app);
    ImGui::SameLine();
    if (ImGui::Button("Create User", gui::scaled(ImVec2(140.0f, 40.0f))))
        create_new_user(app);

    ImGui::Spacing();
    if (app.current_user != nullptr)
        ImGui::Text("Current user: %s", app.current_user->username().c_str());
    gui::draw_game_status(app.play);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Available Maps");
    if (app.current_user != nullptr)
    {
        if (ImGui::Button("Create Map", gui::scaled(ImVec2(220.0f, 38.0f))))
            create_new_map(app);
        ImGui::Spacing();
    }
    draw_map_buttons(app);

    ImGui::End();
}

void draw_highscore_window(const AppState &app)
{
    ImGui::SetNextWindowPos(gui::scaled(kHighscoreWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(gui::scaled(kHighscoreWindowSize), ImGuiCond_Always);
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
    gui::apply_style();
    ImGui::NewFrame();

    handle_keyboard(app);
    if (gui::builder_editor_active(app.builder))
    {
        if (gui::draw_builder_editor(app.builder, true))
        {
            gui::clear_builder_editor(app.builder);
            if (app.current_user != nullptr)
                (void)app.current_user->available_maps();
        }
    }
    else if (app.play.game)
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

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    int window_width = 1440;
    int window_height = 900;
    if (GLFWmonitor *monitor = glfwGetPrimaryMonitor())
    {
        if (const GLFWvidmode *mode = glfwGetVideoMode(monitor))
        {
            const int min_width = std::min(1024, mode->width);
            const int min_height = std::min(720, mode->height);
            window_width = std::clamp(static_cast<int>(mode->width * 0.9f), min_width, mode->width);
            window_height = std::clamp(static_cast<int>(mode->height * 0.9f), min_height, mode->height);
        }
    }

    GLFWwindow *window = glfwCreateWindow(window_width, window_height, "Type Erasure", nullptr, nullptr);
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
    gui::apply_style();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AppState app;

    while (!glfwWindowShouldClose(window))
        draw_frame(window, app);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
