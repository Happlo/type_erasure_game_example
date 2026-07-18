#include "builder_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace gui
{
namespace
{
using gui::draw_tile_symbol;
using gui::empty_tile_fill;
using gui::object_tile_fill;

constexpr ImVec2 kToolsWindowPos{24.0f, 24.0f};
constexpr ImVec2 kToolsWindowSize{360.0f, 852.0f};
constexpr ImVec2 kCanvasWindowPos{408.0f, 24.0f};
constexpr ImVec2 kCanvasWindowSize{1008.0f, 852.0f};
constexpr int kDefaultViewportWidth = 9;
constexpr int kDefaultViewportHeight = 7;

struct Viewport
{
    core::Location min{.x = 0, .y = 0};
    int width{kDefaultViewportWidth};
    int height{kDefaultViewportHeight};
};

std::string join_chars(const std::vector<char> &chars)
{
    std::string result;
    result.reserve(chars.size() * 2);
    for (size_t i = 0; i < chars.size(); ++i)
    {
        if (i > 0)
            result += ' ';
        result.push_back(chars[i]);
    }
    return result;
}

Viewport viewport_for(const BuilderEditorState &state)
{
    const core::MapView &view = state.map->view();
    core::Location min{.x = 0, .y = 0};
    core::Location max{.x = kDefaultViewportWidth - 1, .y = kDefaultViewportHeight - 1};
    const auto include = [&](const core::Location &location) {
        min.x = std::min(min.x, location.x);
        min.y = std::min(min.y, location.y);
        max.x = std::max(max.x, location.x);
        max.y = std::max(max.y, location.y);
    };

    if (view.player.has_value())
        include(view.player->location);
    if (state.selected_cell.has_value())
        include(core::Location{.x = state.selected_cell->x, .y = state.selected_cell->y});
    for (const auto &[location, object] : view.objects)
    {
        (void)object;
        include(location);
    }

    return Viewport{.min = min, .width = max.x - min.x + 1, .height = max.y - min.y + 1};
}

void update_object_symbol(BuilderEditorState &state)
{
    state.object.symbol = state.symbol_buffer[0] == '\0' ? '*' : state.symbol_buffer[0];
}

void sync_symbol_buffer(BuilderEditorState &state)
{
    state.symbol_buffer[0] = state.object.symbol;
    state.symbol_buffer[1] = '\0';
}

bool manipulation_is_push(const core::Object::ManipulationLevel manipulation_level)
{
    return manipulation_level == core::Object::ManipulationLevel::Push;
}

bool manipulation_is_pick(const core::Object::ManipulationLevel manipulation_level)
{
    return manipulation_level == core::Object::ManipulationLevel::Pick;
}

void add_object_to_selected_cell(BuilderEditorState &state)
{
    if (!state.selected_cell.has_value())
        return;
    state.map->add_object(
        core::Location{.x = state.selected_cell->x, .y = state.selected_cell->y}, state.object);
}

void clear_selected_cell(BuilderEditorState &state)
{
    if (!state.selected_cell.has_value())
        return;
    state.map->clear_cell(core::Location{.x = state.selected_cell->x, .y = state.selected_cell->y});
}

void select_cell(BuilderEditorState &state, const int x, const int y)
{
    state.selected_cell = BuilderEditorState::CellSelection{.x = x, .y = y};
    const core::MapView &view = state.map->view();
    if (view.player.has_value() && view.player->location == core::Location{.x = x, .y = y})
    {
        state.object.symbol = view.player->symbol;
        state.object.manipulation_level = core::Object::ManipulationLevel::None;
        sync_symbol_buffer(state);
        return;
    }

    const auto object = view.objects.find(core::Location{.x = x, .y = y});
    if (object == view.objects.end())
    {
        state.object.symbol = '*';
        sync_symbol_buffer(state);
        return;
    }

    state.object = object->second;
    sync_symbol_buffer(state);
}

void move_selection(BuilderEditorState &state, const int dx, const int dy)
{
    if (!state.selected_cell.has_value())
    {
        select_cell(state, 0, 0);
        return;
    }

    select_cell(state, state.selected_cell->x + dx, state.selected_cell->y + dy);
}

void load_map(BuilderEditorState &state)
{
    try
    {
        state.map = core::MapBuilder::load_from_file(state.file_path.data());
        state.selected_cell.reset();
        state.status = "Loaded map.";
    }
    catch (const std::exception &ex)
    {
        state.status = std::string("Failed to load map: ") + ex.what();
    }
}

void save_map(BuilderEditorState &state)
{
    try
    {
        state.map->save_to_file(state.file_path.data());
        state.status = "Saved map.";
    }
    catch (const std::exception &ex)
    {
        state.status = std::string("Failed to save map: ") + ex.what();
    }
}

void validate_map(BuilderEditorState &state)
{
    try
    {
        (void)state.map->create_game();
        state.status = "Map validation passed.";
    }
    catch (const std::exception &ex)
    {
        state.status = std::string("Map validation failed: ") + ex.what();
    }
}

void try_map(BuilderEditorState &state)
{
    try
    {
        gui::start_game(state.try_game, state.map->create_game(), "Trying current builder map.");
        state.mode = BuilderMode::TryMap;
    }
    catch (const std::exception &ex)
    {
        state.status = std::string("Cannot try map: ") + ex.what();
    }
}

void draw_object_preview(const BuilderEditorState &state)
{
    const bool preview_is_player = state.object.symbol == '^' || state.object.symbol == 'v' ||
                                   state.object.symbol == '<' || state.object.symbol == '>';

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 preview_size = scaled(ImVec2(72.0f, 72.0f));
    const ImVec2 cell_max(origin.x + preview_size.x, origin.y + preview_size.y);
    draw_list->AddRectFilled(
        origin, cell_max,
        preview_is_player ? palette::player_fill : object_tile_fill(state.object), scaled(14.0f));
    draw_list->AddRect(origin, cell_max,
                       preview_is_player ? palette::player_outline : palette::tile_outline,
                       scaled(14.0f), 0, scaled(3.0f));
    draw_tile_symbol(*draw_list, origin, cell_max,
                     preview_is_player ? state.object.symbol : state.object.symbol, scaled(40.0f));
    ImGui::Dummy(preview_size);
}

bool draw_tools_window(BuilderEditorState &state, const bool show_back_button)
{
    bool back_requested = false;
    ImGui::SetNextWindowPos(scaled(kToolsWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(scaled(kToolsWindowSize), ImGuiCond_Always);
    ImGui::Begin("Builder Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure Builder");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Left click selects a cell. Typing places a symbol. Arrow keys beyond an edge expand "
        "the map. Right click clears and selects.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Object");
    ImGui::Text("Symbol: %c", state.object.symbol);
    bool pushable = manipulation_is_push(state.object.manipulation_level);
    bool pickable = manipulation_is_pick(state.object.manipulation_level);
    if (ImGui::Checkbox("Pushable", &pushable))
    {
        state.object.manipulation_level = pushable ? core::Object::ManipulationLevel::Push
                                                   : core::Object::ManipulationLevel::None;
        add_object_to_selected_cell(state);
    }
    if (ImGui::Checkbox("Pickable", &pickable))
    {
        state.object.manipulation_level = pickable ? core::Object::ManipulationLevel::Pick
                                                   : core::Object::ManipulationLevel::None;
        add_object_to_selected_cell(state);
    }
    draw_object_preview(state);
    if (state.selected_cell.has_value())
        ImGui::Text("Selected: (%d, %d)", state.selected_cell->x, state.selected_cell->y);
    else
        ImGui::TextDisabled("No cell selected.");

    ImGui::TextWrapped("Tips: type ^ v < > for player facing, 0..9 for numbers.");
    const std::string solver_operators = join_chars(core::MapBuilder::solver_operators());
    const std::string solver_separators = join_chars(core::MapBuilder::equation_delimiters());
    ImGui::TextWrapped("Solver operators: %s", solver_operators.c_str());
    ImGui::TextWrapped("Solver separators: %s", solver_separators.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::InputText("File", state.file_path.data(), state.file_path.size());
    if (ImGui::Button("Save Map", scaled(ImVec2(140.0f, 36.0f))))
        save_map(state);
    ImGui::SameLine();
    if (ImGui::Button("Load Map", scaled(ImVec2(140.0f, 36.0f))))
        load_map(state);
    if (ImGui::Button("Validate", scaled(ImVec2(140.0f, 36.0f))))
        validate_map(state);
    ImGui::SameLine();
    if (ImGui::Button("Try Map", scaled(ImVec2(140.0f, 36.0f))))
        try_map(state);

    if (show_back_button)
    {
        if (ImGui::Button("Back To Maps", scaled(ImVec2(310.0f, 38.0f))))
            back_requested = true;
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", state.status.c_str());

    ImGui::End();
    return back_requested;
}

void draw_map_tile(ImDrawList &draw_list, BuilderEditorState &state, const Viewport &viewport,
                   const float tile_size, const ImVec2 origin, const int x, const int y)
{
    const core::MapView &view = state.map->view();
    const auto object = view.objects.find(core::Location{.x = x, .y = y});
    const bool has_player =
        view.player.has_value() && view.player->location == core::Location{.x = x, .y = y};
    const int screen_x = x - viewport.min.x;
    const int screen_y = y - viewport.min.y;
    const ImVec2 cell_min(origin.x + screen_x * tile_size, origin.y + screen_y * tile_size);
    const float gap = scaled(4.0f);
    const float rounding = scaled(12.0f);
    const ImVec2 cell_max(cell_min.x + tile_size - gap, cell_min.y + tile_size - gap);
    draw_list.AddRectFilled(
        cell_min, cell_max,
        has_player
            ? palette::player_fill
            : (object == view.objects.end() ? empty_tile_fill() : object_tile_fill(object->second)),
        rounding);
    const bool selected = state.selected_cell.has_value() && state.selected_cell->x == x &&
                          state.selected_cell->y == y;
    draw_list.AddRect(cell_min, cell_max,
                      selected ? palette::selected_tile_outline
                               : (has_player ? palette::player_outline : palette::tile_outline),
                      rounding, 0, scaled(selected ? 4.0f : 2.0f));
    if (has_player || object != view.objects.end())
        draw_tile_symbol(draw_list, cell_min, cell_max,
                         has_player ? view.player->symbol : object->second.symbol,
                         tile_size * 0.58f);

    ImGui::SetCursorScreenPos(cell_min);
    ImGui::InvisibleButton(("cell_" + std::to_string(x) + "_" + std::to_string(y)).c_str(),
                           ImVec2(tile_size, tile_size));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        select_cell(state, x, y);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        select_cell(state, x, y);
        state.map->clear_cell(core::Location{.x = x, .y = y});
    }
}

void draw_map_row(ImDrawList &draw_list, BuilderEditorState &state, const Viewport &viewport,
                  const float tile_size, const ImVec2 origin, const int y)
{
    for (int x = viewport.min.x; x < viewport.min.x + viewport.width; ++x)
        draw_map_tile(draw_list, state, viewport, tile_size, origin, x, y);
}

void draw_canvas_window(BuilderEditorState &state)
{
    ImGui::SetNextWindowPos(scaled(kCanvasWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(scaled(kCanvasWindowSize), ImGuiCond_Always);
    ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const Viewport viewport = viewport_for(state);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float fit_size = std::min(available.x / static_cast<float>(viewport.width),
                                    available.y / static_cast<float>(viewport.height));
    const float tile_size = std::clamp(fit_size, scaled(24.0f), scaled(96.0f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(tile_size * viewport.width, tile_size * viewport.height);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + total_size.x, origin.y + total_size.y),
                             palette::board_background, scaled(18.0f));

    for (int y = viewport.min.y; y < viewport.min.y + viewport.height; ++y)
        draw_map_row(*draw_list, state, viewport, tile_size, origin, y);

    ImGui::Dummy(ImVec2(total_size.x, total_size.y));
    ImGui::End();
}

void return_to_builder(BuilderEditorState &state)
{
    gui::clear_game(state.try_game, "Ready.");
    state.mode = BuilderMode::Edit;
    state.status = "Returned from try map.";
}

void draw_try_tools_window(BuilderEditorState &state, const core::MapView &view)
{
    ImGui::SetNextWindowPos(scaled(kToolsWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(scaled(kToolsWindowSize), ImGuiCond_Always);
    ImGui::Begin("Try Map", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure Builder");
    ImGui::Separator();
    ImGui::TextWrapped("Playtesting the current builder map.");

    ImGui::Spacing();
    gui::draw_game_sidebar_state(state.try_game, view);

    ImGui::Spacing();
    gui::draw_game_controls_info();

    ImGui::Spacing();
    if (ImGui::Button("Back To Builder", scaled(ImVec2(310.0f, 38.0f))))
        return_to_builder(state);

    ImGui::Spacing();
    gui::draw_game_status(state.try_game);

    ImGui::End();
}

void draw_try_canvas_window(const GamePlayState &try_game, const core::MapView &view)
{
    ImGui::SetNextWindowPos(scaled(kCanvasWindowPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(scaled(kCanvasWindowSize), ImGuiCond_Always);
    ImGui::Begin("Playtest", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    gui::draw_game_grid(view, try_game.equation_result, &try_game);
    ImGui::End();
}
} // namespace

void start_builder_editor(BuilderEditorState &state, std::unique_ptr<core::MapBuilder> map,
                          const char *file_path, std::string status)
{
    state = BuilderEditorState{};
    state.map = std::move(map);
    std::snprintf(state.file_path.data(), state.file_path.size(), "%s", file_path);
    state.status = std::move(status);
}

void clear_builder_editor(BuilderEditorState &state)
{
    gui::clear_game(state.try_game, "Ready.");
    state.map.reset();
    state.selected_cell.reset();
    state.status = "Ready.";
    state.mode = BuilderMode::Edit;
}

bool builder_editor_active(const BuilderEditorState &state) { return state.map != nullptr; }

void handle_builder_editor_keyboard(BuilderEditorState &state)
{
    if (!builder_editor_active(state))
        return;

    if (state.mode == BuilderMode::TryMap && state.try_game.game)
    {
        gui::handle_game_keyboard(state.try_game);
        return;
    }

    update_object_symbol(state);

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
        move_selection(state, -1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
        move_selection(state, 1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
        move_selection(state, 0, -1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
        move_selection(state, 0, 1);

    if (!state.selected_cell.has_value())
        return;

    for (const ImWchar character : io.InputQueueCharacters)
    {
        if (character < 32 || character > 126)
            continue;
        if (!std::isprint(static_cast<unsigned char>(character)))
            continue;
        state.object.symbol = static_cast<char>(character);
        sync_symbol_buffer(state);
        add_object_to_selected_cell(state);
        move_selection(state, 1, 0);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        clear_selected_cell(state);
    }
}

bool draw_builder_editor(BuilderEditorState &state, const bool show_back_button)
{
    if (!builder_editor_active(state))
        return false;

    if (state.mode == BuilderMode::TryMap && state.try_game.game)
    {
        const core::MapView view = state.try_game.game->view();
        draw_try_tools_window(state, view);
        if (state.try_game.game)
            draw_try_canvas_window(state.try_game, view);
        return false;
    }

    const bool back_requested = draw_tools_window(state, show_back_button);
    draw_canvas_window(state);
    return back_requested;
}
} // namespace gui
