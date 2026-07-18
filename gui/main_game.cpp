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
constexpr int kUsernameSelection = 0;
constexpr int kLoginSelection = 1;
constexpr int kCreateUserSelection = 2;
constexpr int kCreateMapSelection = 3;
constexpr int kFirstMapSelection = 4;

struct AppState
{
    std::unique_ptr<core::LoginView> login{core::LoginView::create()};
    core::User *current_user{nullptr};
    GamePlayState play;
    gui::BuilderEditorState builder;
    std::array<char, 128> username_input{};
    bool should_focus_username{true};
    int login_selection{kUsernameSelection};
};

void return_to_login(AppState &app)
{
    gui::clear_game(app.play, "Choose a map to start a game.");
    gui::clear_builder_editor(app.builder);
    app.should_focus_username = true;
    app.login_selection = kUsernameSelection;
}

int login_selectable_count(const AppState &app, const std::vector<core::MapEntry> *maps)
{
    int count = kCreateUserSelection + 1;
    if (app.current_user != nullptr)
        count = kCreateMapSelection + 1 + static_cast<int>(maps == nullptr ? 0 : maps->size());
    return count;
}

void clamp_login_selection(AppState &app, const std::vector<core::MapEntry> *maps)
{
    const int count = login_selectable_count(app, maps);
    app.login_selection = std::clamp(app.login_selection, 0, count - 1);
}

void draw_selection_border(const bool selected)
{
    if (!selected)
        return;

    constexpr float kInset = 2.0f;
    const float inset = gui::scaled(kInset);
    ImGui::GetWindowDrawList()->AddRect(
        ImVec2(ImGui::GetItemRectMin().x - inset, ImGui::GetItemRectMin().y - inset),
        ImVec2(ImGui::GetItemRectMax().x + inset, ImGui::GetItemRectMax().y + inset),
        IM_COL32(245, 214, 112, 255), gui::scaled(8.0f), 0, gui::scaled(2.5f));
}

void handle_login_navigation(AppState &app, const std::vector<core::MapEntry> *maps)
{
    clamp_login_selection(app, maps);

    const int count = login_selectable_count(app, maps);
    bool moved = false;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
    {
        app.login_selection = (app.login_selection + 1) % count;
        moved = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
    {
        app.login_selection = (app.login_selection + count - 1) % count;
        moved = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) &&
        app.login_selection == kLoginSelection)
    {
        app.login_selection = kCreateUserSelection;
        moved = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) &&
        app.login_selection == kCreateUserSelection)
    {
        app.login_selection = kLoginSelection;
        moved = true;
    }

    if (moved)
        app.should_focus_username = false;
}

bool enter_released()
{
    return ImGui::IsKeyReleased(ImGuiKey_Enter) || ImGui::IsKeyReleased(ImGuiKey_KeypadEnter);
}

bool mouse_is_using_login_controls()
{
    return ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
           ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
           ImGui::IsMouseReleased(ImGuiMouseButton_Left);
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
    gui::draw_game_controls_info();

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

void draw_map_buttons(AppState &app, const std::vector<core::MapEntry> &maps)
{
    if (app.current_user == nullptr)
    {
        ImGui::TextDisabled("Log in to see the map list.");
        return;
    }

    const auto &solved_maps = app.current_user->solved_maps();
    if (maps.empty())
    {
        ImGui::TextDisabled("No maps found in maps/.");
        return;
    }

    for (int index = 0; index < static_cast<int>(maps.size()); ++index)
    {
        const auto &map = maps[static_cast<std::size_t>(index)];
        const int selection = kFirstMapSelection + index;
        const bool selected = app.login_selection == selection;

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
        if (ImGui::Button(label.c_str(), gui::scaled(ImVec2(220.0f, 38.0f))) ||
            (selected && enter_released()))
        {
            app.login_selection = selection;
            start_selected_map(app, map.map_id);
        }
        draw_selection_border(selected);

        ImGui::PopStyleColor();
    }
}

void draw_login_window(AppState &app)
{
    ImGui::SetNextWindowPos(gui::scaled(kLoginWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(gui::scaled(kLoginWindowSize), ImGuiCond_Always);
    ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const std::vector<core::MapEntry> *maps_for_navigation = nullptr;
    if (app.current_user != nullptr)
        maps_for_navigation = &app.current_user->available_maps();
    handle_login_navigation(app, maps_for_navigation);

    ImGui::TextUnformatted("Type Erasure");
    ImGui::Separator();
    ImGui::TextWrapped("Create a user or log in, then choose a map to start a game.");
    
    const bool username_selected = app.login_selection == kUsernameSelection;
    if (app.should_focus_username && app.current_user == nullptr &&
        !mouse_is_using_login_controls())
    {
        ImGui::SetKeyboardFocusHere();
    }
    if (ImGui::InputText("Username", app.username_input.data(), app.username_input.size(), ImGuiInputTextFlags_EnterReturnsTrue) &&
        username_selected)
        login_existing_user(app);
    draw_selection_border(username_selected);
    if (ImGui::IsItemActive())
        app.should_focus_username = false;

    const bool login_selected = app.login_selection == kLoginSelection;
    if (ImGui::Button("Log In", gui::scaled(ImVec2(140.0f, 40.0f))) ||
        (login_selected && enter_released()))
    {
        app.login_selection = kLoginSelection;
        login_existing_user(app);
    }
    draw_selection_border(login_selected);

    ImGui::SameLine();
    const bool create_user_selected = app.login_selection == kCreateUserSelection;
    if (ImGui::Button("Create User", gui::scaled(ImVec2(140.0f, 40.0f))) ||
        (create_user_selected && enter_released()))
    {
        app.login_selection = kCreateUserSelection;
        create_new_user(app);
    }
    draw_selection_border(create_user_selected);

    ImGui::Spacing();
    if (app.current_user != nullptr)
        ImGui::Text("Current user: %s", app.current_user->username().c_str());
    gui::draw_game_status(app.play);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Available Maps");
    if (app.current_user != nullptr)
    {
        const bool create_map_selected = app.login_selection == kCreateMapSelection;
        if (ImGui::Button("Create Map", gui::scaled(ImVec2(220.0f, 38.0f))) ||
            (create_map_selected && enter_released()))
        {
            app.login_selection = kCreateMapSelection;
            create_new_map(app);
        }
        draw_selection_border(create_map_selected);
        ImGui::Spacing();
    }
    if (app.current_user != nullptr)
        draw_map_buttons(app, app.current_user->available_maps());
    else
        ImGui::TextDisabled("Log in to see the map list.");

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
