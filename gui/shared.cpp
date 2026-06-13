#include "shared.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <utility>

namespace gui
{
namespace
{
ImVec4 color_from_rgb(const int r, const int g, const int b)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

std::string describe_new_assignments(const core::GameResult& before,
                                     const core::GameResult& after)
{
    std::string out;

    for (const auto& [symbol, value] : after.resolved_variables)
    {
        const auto it = before.resolved_variables.find(symbol);
        if (it != before.resolved_variables.end() && it->second == value)
            continue;

        if (!out.empty())
            out += ", ";
        out.push_back(symbol);
        out += "=" + std::to_string(value);
    }

    return out;
}

bool tile_has_equal_status(const core::GameResult& result, const int x, const int y,
                           core::EqualityStatus& status)
{
    const auto it = result.equal_sign_status.find(core::Location{x, y});
    if (it == result.equal_sign_status.end())
        return false;
    status = it->second;
    return true;
}

struct GridRenderContext
{
    ImDrawList* draw_list;
    ImVec2 origin;
    core::Location min_location;
    float tile_size;
    bool solved;
    const core::GameResult* equation_result;
};

struct DisplayBounds
{
    core::Location min;
    core::Location max;
};

DisplayBounds display_bounds(const core::MapView& view)
{
    DisplayBounds bounds{.min = core::Location{.x = 0, .y = 0},
                         .max = core::Location{.x = 0, .y = 0}};
    bool initialized = false;
    const auto include = [&](const core::Location& location)
    {
        if (!initialized)
        {
            bounds.min = location;
            bounds.max = location;
            initialized = true;
            return;
        }
        bounds.min.x = std::min(bounds.min.x, location.x);
        bounds.min.y = std::min(bounds.min.y, location.y);
        bounds.max.x = std::max(bounds.max.x, location.x);
        bounds.max.y = std::max(bounds.max.y, location.y);
    };

    if (view.player.has_value())
        include(view.player->location);
    for (const auto& [location, object] : view.objects)
    {
        (void)object;
        include(location);
    }

    return bounds;
}

int display_width(const DisplayBounds& bounds) { return bounds.max.x - bounds.min.x + 1; }

int display_height(const DisplayBounds& bounds) { return bounds.max.y - bounds.min.y + 1; }

constexpr ImVec2 kMoveButtonSize{96.0f, 36.0f};
constexpr ImVec2 kActionButtonSize{150.0f, 38.0f};
constexpr int kMaxRenderedTilesPerAxis = 15;

std::pair<int, int> clamp_axis(const int min_value, const int max_value, const int focus)
{
    const int size = max_value - min_value + 1;
    if (size <= kMaxRenderedTilesPerAxis)
        return {min_value, max_value};

    int start = focus - kMaxRenderedTilesPerAxis / 2;
    start = std::max(start, min_value);
    start = std::min(start, max_value - kMaxRenderedTilesPerAxis + 1);
    return {start, start + kMaxRenderedTilesPerAxis - 1};
}

DisplayBounds limit_display_bounds(const DisplayBounds& bounds, const core::MapView& view)
{
    if (!view.player.has_value())
        return bounds;

    const auto [min_x, max_x] =
        clamp_axis(bounds.min.x, bounds.max.x, view.player->location.x);
    const auto [min_y, max_y] =
        clamp_axis(bounds.min.y, bounds.max.y, view.player->location.y);
    return DisplayBounds{.min = core::Location{.x = min_x, .y = min_y},
                         .max = core::Location{.x = max_x, .y = max_y}};
}
}  // namespace

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

bool game_is_solved(const core::GameResult& result)
{
    return std::any_of(result.equal_sign_status.begin(), result.equal_sign_status.end(),
                       [](const auto& entry)
                       { return entry.second == core::EqualityStatus::Equal; });
}

void start_game(GamePlayState& state, std::unique_ptr<core::Game> game, std::string status)
{
    state.game = std::move(game);
    state.equation_result.reset();
    state.assignment_feedback.clear();
    state.status = std::move(status);
}

void clear_game(GamePlayState& state, std::string status)
{
    state.game.reset();
    state.equation_result.reset();
    state.assignment_feedback.clear();
    state.status = std::move(status);
}

bool trigger_game_event(GamePlayState& state, const core::Event event)
{
    if (!state.game)
        return false;

    const core::GameResult previous_result =
        state.equation_result.value_or(core::GameResult{});
    state.equation_result = state.game->apply_event(event);
    state.assignment_feedback = describe_new_assignments(previous_result, *state.equation_result);

    if (game_is_solved(*state.equation_result))
    {
        state.status = state.assignment_feedback.empty()
                           ? "Equation solved."
                           : "Assigned " + state.assignment_feedback + ". Equation solved.";
        return true;
    }

    if (!state.assignment_feedback.empty())
    {
        state.status = "Assigned " + state.assignment_feedback + ".";
        return true;
    }

    switch (event)
    {
    case core::Event::MoveUp:
    case core::Event::MoveLeft:
    case core::Event::MoveDown:
    case core::Event::MoveRight:
        state.status = "Moved.";
        break;
    case core::Event::PickItem:
        state.status = "Picked up item.";
        break;
    case core::Event::DropItem:
        state.status = "Dropped item.";
        break;
    case core::Event::Commit:
        state.status = "Committed map state.";
        break;
    case core::Event::Undo:
        state.status = "Restored previous commit.";
        break;
    }

    return true;
}

void handle_game_keyboard(GamePlayState& state)
{
    if (!state.game)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    struct KeyBinding
    {
        ImGuiKey primary;
        ImGuiKey alternate;
        core::Event event;
    };

    constexpr std::array<KeyBinding, 8> bindings{{
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
            trigger_game_event(state, binding.event);
        }
    }
}

bool game_action_button(const char* label, const ImVec2 size, GamePlayState& state,
                        const core::Event event)
{
    if (!ImGui::Button(label, size))
        return false;
    return trigger_game_event(state, event);
}

void draw_game_status(const GamePlayState& state)
{
    if (!state.game)
    {
        ImGui::TextWrapped("%s", state.status.c_str());
        return;
    }

    if (state.equation_result.has_value() && game_is_solved(*state.equation_result))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.91f, 0.67f, 1.0f));
        ImGui::TextWrapped("Solved. Load another map or reset to play again.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextWrapped(
            "Controls: WASD or arrow keys move. E picks up, Q drops, C commits, U undoes.");
    }
    ImGui::TextWrapped("%s", state.status.c_str());
}

void draw_game_map_stats(const core::MapView& view)
{
    ImGui::Text("Map commits: %d", view.commits_left);
    ImGui::Text("Map undos: %d", view.undos_left);
}

void draw_game_sidebar_state(const GamePlayState& state, const core::MapView& view)
{
    draw_game_map_stats(view);
    draw_game_inventory(view);
    draw_game_variables(state.equation_result);
    draw_game_assignment_feedback(state);
}

void draw_game_action_controls(GamePlayState& state)
{
    ImGui::TextUnformatted("Movement");
    game_action_button("Up", kMoveButtonSize, state, core::Event::MoveUp);
    game_action_button("Left", kMoveButtonSize, state, core::Event::MoveLeft);
    ImGui::SameLine();
    game_action_button("Down", kMoveButtonSize, state, core::Event::MoveDown);
    ImGui::SameLine();
    game_action_button("Right", kMoveButtonSize, state, core::Event::MoveRight);

    ImGui::Spacing();
    game_action_button("Pick  [E]", kActionButtonSize, state, core::Event::PickItem);
    ImGui::SameLine();
    game_action_button("Drop  [Q]", kActionButtonSize, state, core::Event::DropItem);
    game_action_button("Commit  [C]", kActionButtonSize, state, core::Event::Commit);
    ImGui::SameLine();
    game_action_button("Undo  [U]", kActionButtonSize, state, core::Event::Undo);
}

void draw_game_variables(const std::optional<core::GameResult>& result)
{
    ImGui::TextUnformatted("Variables");
    if (!result.has_value())
    {
        ImGui::TextDisabled("No assignments evaluated yet.");
        return;
    }

    if (result->resolved_variables.empty())
    {
        ImGui::TextDisabled("None");
        return;
    }

    bool first = true;
    for (const auto& [symbol, value] : result->resolved_variables)
    {
        if (!first)
            ImGui::SameLine();
        first = false;
        ImGui::Text("%c=%d", symbol, value);
    }
}

void draw_game_assignment_feedback(const GamePlayState& state)
{
    if (state.assignment_feedback.empty())
        return;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.84f, 0.45f, 1.0f));
    ImGui::TextWrapped("Assigned: %s", state.assignment_feedback.c_str());
    ImGui::PopStyleColor();
}

void draw_game_inventory(const core::MapView& view)
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
        if (i + 1 < view.player->inventory.size())
            ImGui::SameLine();
    }
}

ImU32 empty_tile_fill()
{
    return IM_COL32(31, 37, 43, 255);
}

ImU32 object_tile_fill(const core::Object& object)
{
    if (object.symbol == '=' || object.symbol == '+') return IM_COL32(108, 167, 124, 255);
    if (object.manipulation_level == core::Object::ManipulationLevel::Pick) return IM_COL32(97, 147, 196, 255);
    if (object.manipulation_level == core::Object::ManipulationLevel::Push) return IM_COL32(189, 112, 143, 255);
    return IM_COL32(116, 123, 132, 255);
}

ImU32 object_tile_outline(const core::Object& object)
{
    if (object.symbol == '=' || object.symbol == '+') return IM_COL32(170, 223, 170, 255);
    return IM_COL32(72, 79, 88, 255);
}

void draw_tile_symbol(ImDrawList& draw_list, const ImVec2& cell_min, const ImVec2& cell_max, const char symbol,
                      const float font_size, const ImU32 color)
{
    const char glyph[2] = {symbol, '\0'};
    ImFont* font = ImGui::GetFont();
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, glyph);
    const ImVec2 text_pos(
        cell_min.x + ((cell_max.x - cell_min.x) - text_size.x) * 0.5f,
        cell_min.y + ((cell_max.y - cell_min.y) - text_size.y) * 0.5f);
    draw_list.AddText(font, font_size, text_pos, color, glyph);
}

void draw_game_tile(const GridRenderContext& ctx, const core::Object* object, const int x,
                    const int y)
{
    const ImVec2 cell_min(ctx.origin.x + x * ctx.tile_size, ctx.origin.y + y * ctx.tile_size);
    const ImVec2 cell_max(cell_min.x + ctx.tile_size - 4.0f, cell_min.y + ctx.tile_size - 4.0f);
    ImU32 fill = object == nullptr ? empty_tile_fill() : object_tile_fill(*object);
    ImU32 outline =
        ctx.solved ? IM_COL32(182, 240, 175, 255)
                   : (object == nullptr ? IM_COL32(72, 79, 88, 255) : object_tile_outline(*object));
    float thickness = ctx.solved ? 3.0f : 2.0f;

    const core::Location location{.x = ctx.min_location.x + x, .y = ctx.min_location.y + y};
    if (object != nullptr && object->symbol == '=' && ctx.equation_result != nullptr)
    {
        core::EqualityStatus equality_status;
        if (tile_has_equal_status(*ctx.equation_result, location.x, location.y, equality_status))
        {
            if (equality_status == core::EqualityStatus::Equal)
            {
                fill = IM_COL32(36, 88, 54, 255);
                outline = IM_COL32(154, 236, 170, 255);
            }
            else
            {
                fill = IM_COL32(92, 39, 39, 255);
                outline = IM_COL32(235, 126, 126, 255);
            }
            thickness = 3.0f;
        }
    }

    ctx.draw_list->AddRectFilled(cell_min, cell_max, fill, 12.0f);
    ctx.draw_list->AddRect(cell_min, cell_max, outline, 12.0f, 0, thickness);
    if (object != nullptr)
        draw_tile_symbol(*ctx.draw_list, cell_min, cell_max, object->symbol, ctx.tile_size * 0.58f);
}

void draw_player_tile(const GridRenderContext& ctx, const core::Player& player)
{
    const int x = player.location.x - ctx.min_location.x;
    const int y = player.location.y - ctx.min_location.y;
    const ImVec2 cell_min(ctx.origin.x + x * ctx.tile_size, ctx.origin.y + y * ctx.tile_size);
    const ImVec2 cell_max(cell_min.x + ctx.tile_size - 4.0f, cell_min.y + ctx.tile_size - 4.0f);
    ctx.draw_list->AddRectFilled(cell_min, cell_max, IM_COL32(236, 177, 79, 255), 12.0f);
    ctx.draw_list->AddRect(cell_min, cell_max, IM_COL32(255, 226, 157, 255), 12.0f, 0, 3.0f);
    draw_tile_symbol(*ctx.draw_list, cell_min, cell_max, player.symbol, ctx.tile_size * 0.58f);
}

void draw_game_solved_badge(ImDrawList& draw_list, const ImVec2 origin)
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

void draw_game_grid_background(ImDrawList& draw_list, const ImVec2 origin, const ImVec2 board_max,
                               const bool solved)
{
    draw_list.AddRectFilled(origin, board_max,
                            solved ? IM_COL32(23, 43, 30, 255) : IM_COL32(20, 24, 29, 255),
                            18.0f);
    if (!solved)
        return;

    draw_list.AddRect(origin, board_max, IM_COL32(137, 224, 151, 255), 18.0f, 0, 4.0f);
    draw_list.AddRect(origin, board_max, IM_COL32(195, 255, 204, 120), 18.0f, 0, 10.0f);
}

void draw_game_grid(const core::MapView& view, const std::optional<core::GameResult>& result)
{
    const bool solved = result.has_value() && game_is_solved(*result);
    const DisplayBounds bounds = limit_display_bounds(display_bounds(view), view);
    const int width = display_width(bounds);
    const int height = display_height(bounds);
    const float tile_size =
        std::clamp(560.0f / static_cast<float>(std::max(width, height)), 36.0f, 72.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(tile_size * width, tile_size * height);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 board_max(origin.x + total_size.x, origin.y + total_size.y);
    const GridRenderContext ctx{
        draw_list, origin, bounds.min, tile_size, solved, result.has_value() ? &*result : nullptr};

    draw_game_grid_background(*draw_list, origin, board_max, solved);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto object = view.objects.find(
                core::Location{.x = bounds.min.x + x, .y = bounds.min.y + y});
            draw_game_tile(ctx, object == view.objects.end() ? nullptr : &object->second, x, y);
        }
    }
    if (view.player.has_value())
        draw_player_tile(ctx, *view.player);
    if (solved)
        draw_game_solved_badge(*draw_list, origin);

    ImGui::Dummy(ImVec2(total_size.x, total_size.y));
}
}  // namespace gui
