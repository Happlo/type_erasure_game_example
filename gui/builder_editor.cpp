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
using gui::tile_fill;
using gui::tile_outline;

constexpr ImVec2 kToolsWindowPos{24.0f, 24.0f};
constexpr ImVec2 kToolsWindowSize{360.0f, 852.0f};
constexpr ImVec2 kCanvasWindowPos{408.0f, 24.0f};
constexpr ImVec2 kCanvasWindowSize{1008.0f, 852.0f};

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

void clamp_size(BuilderEditorState &state)
{
    state.resize_width = std::clamp(state.resize_width, 1, 64);
    state.resize_height = std::clamp(state.resize_height, 1, 64);
}

void sync_size_from_map(BuilderEditorState &state)
{
    state.resize_width = state.map->view().width;
    state.resize_height = state.map->view().height;
    if (state.selected_cell.has_value() &&
        (state.selected_cell->x >= state.resize_width ||
         state.selected_cell->y >= state.resize_height))
    {
        state.selected_cell.reset();
    }
}

void update_brush_symbol(BuilderEditorState &state)
{
    state.brush.symbol = state.symbol_buffer[0] == '\0' ? '*' : state.symbol_buffer[0];
}

void sync_symbol_buffer(BuilderEditorState &state)
{
    state.symbol_buffer[0] = state.brush.symbol;
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

core::Object::ManipulationLevel manipulation_from_cell(const core::CellView &cell)
{
    if (const auto *object = std::get_if<core::Object>(&cell); object != nullptr)
        return object->manipulation_level;
    return core::Object::ManipulationLevel::None;
}

void apply_brush_to_selected_cell(BuilderEditorState &state)
{
    if (!state.selected_cell.has_value())
        return;
    state.map->apply_brush(state.selected_cell->x, state.selected_cell->y, state.brush);
}

void clear_selected_cell(BuilderEditorState &state)
{
    if (!state.selected_cell.has_value())
        return;
    state.map->clear_cell(state.selected_cell->x, state.selected_cell->y);
}

void select_cell(BuilderEditorState &state, const int x, const int y)
{
    state.selected_cell = BuilderEditorState::CellSelection{.x = x, .y = y};
    const core::CellView &cell = state.map->view().at(x, y);
    if (std::holds_alternative<core::Empty>(cell))
    {
        state.brush.symbol = '*';
        state.brush.manipulation_level = core::Object::ManipulationLevel::None;
        sync_symbol_buffer(state);
        return;
    }

    state.brush.symbol = core::symbol_of(cell);
    state.brush.manipulation_level = manipulation_from_cell(cell);
    sync_symbol_buffer(state);
}

void move_selection(BuilderEditorState &state, const int dx, const int dy)
{
    if (!state.selected_cell.has_value())
    {
        select_cell(state, 0, 0);
        return;
    }

    const int next_x = std::clamp(state.selected_cell->x + dx, 0, state.map->view().width - 1);
    const int next_y = std::clamp(state.selected_cell->y + dy, 0, state.map->view().height - 1);
    select_cell(state, next_x, next_y);
}

void resize_map(BuilderEditorState &state)
{
    clamp_size(state);
    state.map->resize(state.resize_width, state.resize_height);
    sync_size_from_map(state);
    state.status = "Resized map.";
}

void load_map(BuilderEditorState &state)
{
    try
    {
        state.map = core::MapBuilder::load_from_file(state.file_path.data());
        sync_size_from_map(state);
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

void draw_brush_preview(const BuilderEditorState &state)
{
    const core::CellView preview = [&]() -> core::CellView {
        if (state.brush.symbol == '^' || state.brush.symbol == 'v' || state.brush.symbol == '<' ||
            state.brush.symbol == '>')
        {
            return core::Player{.symbol = state.brush.symbol, .inventory = {}};
        }
        return core::Object{.symbol = state.brush.symbol,
                            .manipulation_level = state.brush.manipulation_level};
    }();

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 cell_max(origin.x + 72.0f, origin.y + 72.0f);
    draw_list->AddRectFilled(origin, cell_max, tile_fill(preview), 14.0f);
    draw_list->AddRect(origin, cell_max, tile_outline(preview), 14.0f, 0, 3.0f);
    draw_tile_symbol(*draw_list, origin, cell_max, core::symbol_of(preview), 40.0f);
    ImGui::Dummy(ImVec2(72.0f, 72.0f));
}

bool draw_tools_window(BuilderEditorState &state, const bool show_back_button)
{
    bool back_requested = false;
    ImGui::SetNextWindowPos(kToolsWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kToolsWindowSize, ImGuiCond_Always);
    ImGui::Begin("Builder Deck", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure Builder");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Left click selects a cell. Typing commits a symbol. Right click clears and selects.");

    ImGui::Spacing();
    ImGui::Text("Map size: %d x %d", state.map->view().width, state.map->view().height);
    ImGui::InputInt("Width", &state.resize_width);
    ImGui::InputInt("Height", &state.resize_height);
    if (ImGui::Button("Resize Map", ImVec2(150.0f, 38.0f)))
        resize_map(state);

    ImGui::Spacing();
    int commits_left = state.map->view().commits_left;
    int undos_left = state.map->view().undos_left;
    if (ImGui::InputInt("Commits", &commits_left))
        state.map->set_commits_left(commits_left);
    if (ImGui::InputInt("Undos", &undos_left))
        state.map->set_undos_left(undos_left);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Brush");
    ImGui::Text("Symbol: %c", state.brush.symbol);
    bool pushable = manipulation_is_push(state.brush.manipulation_level);
    bool pickable = manipulation_is_pick(state.brush.manipulation_level);
    if (ImGui::Checkbox("Pushable", &pushable))
    {
        state.brush.manipulation_level = pushable ? core::Object::ManipulationLevel::Push
                                                  : core::Object::ManipulationLevel::None;
        apply_brush_to_selected_cell(state);
    }
    if (ImGui::Checkbox("Pickable", &pickable))
    {
        state.brush.manipulation_level = pickable ? core::Object::ManipulationLevel::Pick
                                                  : core::Object::ManipulationLevel::None;
        apply_brush_to_selected_cell(state);
    }
    draw_brush_preview(state);
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
    if (ImGui::Button("Save Map", ImVec2(140.0f, 36.0f)))
        save_map(state);
    ImGui::SameLine();
    if (ImGui::Button("Load Map", ImVec2(140.0f, 36.0f)))
        load_map(state);
    if (ImGui::Button("Validate", ImVec2(140.0f, 36.0f)))
        validate_map(state);
    ImGui::SameLine();
    if (ImGui::Button("Try Map", ImVec2(140.0f, 36.0f)))
        try_map(state);

    if (show_back_button)
    {
        if (ImGui::Button("Back To Maps", ImVec2(310.0f, 38.0f)))
            back_requested = true;
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", state.status.c_str());

    ImGui::End();
    return back_requested;
}

void draw_map_tile(ImDrawList &draw_list, BuilderEditorState &state, const float tile_size,
                   const ImVec2 origin, const int x, const int y)
{
    const core::CellView &cell = state.map->view().at(x, y);
    const ImVec2 cell_min(origin.x + x * tile_size, origin.y + y * tile_size);
    const ImVec2 cell_max(cell_min.x + tile_size - 4.0f, cell_min.y + tile_size - 4.0f);
    draw_list.AddRectFilled(cell_min, cell_max, tile_fill(cell), 12.0f);
    const bool selected =
        state.selected_cell.has_value() && state.selected_cell->x == x &&
        state.selected_cell->y == y;
    draw_list.AddRect(cell_min, cell_max,
                      selected ? IM_COL32(255, 231, 168, 255) : tile_outline(cell), 12.0f, 0,
                      selected ? 4.0f : 2.0f);
    draw_tile_symbol(draw_list, cell_min, cell_max, core::symbol_of(cell), tile_size * 0.58f);

    ImGui::SetCursorScreenPos(cell_min);
    ImGui::InvisibleButton(("cell_" + std::to_string(x) + "_" + std::to_string(y)).c_str(),
                           ImVec2(tile_size, tile_size));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        select_cell(state, x, y);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        select_cell(state, x, y);
        state.map->clear_cell(x, y);
    }
}

void draw_map_row(ImDrawList &draw_list, BuilderEditorState &state, const float tile_size,
                  const ImVec2 origin, const int y)
{
    for (int x = 0; x < state.map->view().width; ++x)
        draw_map_tile(draw_list, state, tile_size, origin, x, y);
}

void draw_canvas_window(BuilderEditorState &state)
{
    ImGui::SetNextWindowPos(kCanvasWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kCanvasWindowSize, ImGuiCond_Always);
    ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const auto &view = state.map->view();
    const float tile_size =
        std::clamp(560.0f / static_cast<float>(std::max(view.width, view.height)), 34.0f, 72.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(tile_size * view.width, tile_size * view.height);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + total_size.x, origin.y + total_size.y),
                             IM_COL32(20, 24, 29, 255), 18.0f);

    for (int y = 0; y < view.height; ++y)
        draw_map_row(*draw_list, state, tile_size, origin, y);

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
    ImGui::SetNextWindowPos(kToolsWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kToolsWindowSize, ImGuiCond_Always);
    ImGui::Begin("Try Map", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Type Erasure Builder");
    ImGui::Separator();
    ImGui::TextWrapped("Playtesting the current builder map.");

    ImGui::Spacing();
    gui::draw_game_sidebar_state(state.try_game, view);

    ImGui::Spacing();
    gui::draw_game_action_controls(state.try_game);

    ImGui::Spacing();
    if (ImGui::Button("Back To Builder", ImVec2(310.0f, 38.0f)))
        return_to_builder(state);

    ImGui::Spacing();
    gui::draw_game_status(state.try_game);

    ImGui::End();
}

void draw_try_canvas_window(const GamePlayState &try_game, const core::MapView &view)
{
    ImGui::SetNextWindowPos(kCanvasWindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kCanvasWindowSize, ImGuiCond_Always);
    ImGui::Begin("Playtest", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    gui::draw_game_grid(view, try_game.equation_result);
    ImGui::End();
}
} // namespace

void start_builder_editor(BuilderEditorState &state, std::unique_ptr<core::MapBuilder> map,
                          const char *file_path, std::string status)
{
    state = BuilderEditorState{};
    state.map = std::move(map);
    std::snprintf(state.file_path.data(), state.file_path.size(), "%s", file_path);
    sync_size_from_map(state);
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

    update_brush_symbol(state);

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
        state.brush.symbol = static_cast<char>(character);
        sync_symbol_buffer(state);
        apply_brush_to_selected_cell(state);
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
