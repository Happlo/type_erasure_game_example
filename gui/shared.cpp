#include "shared.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

namespace gui
{
namespace
{
const ImVec4 kSolvedTextColor{0.68f, 0.91f, 0.67f, 1.0f};
const ImVec4 kAssignmentTextColor{0.95f, 0.84f, 0.45f, 1.0f};
const ImVec4 kInventoryButtonColor{0.38f, 0.58f, 0.77f, 1.0f};
const ImVec4 kInventoryButtonHoveredColor{0.46f, 0.67f, 0.87f, 1.0f};
const ImVec4 kInventoryButtonActiveColor{0.31f, 0.50f, 0.69f, 1.0f};

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

bool is_remaining_equal_sign(const core::GameResult& result, const int x, const int y)
{
    return std::find(result.remaining_equal_signs.begin(), result.remaining_equal_signs.end(),
                     core::Location{x, y}) != result.remaining_equal_signs.end();
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

constexpr int kVisibleTilesPerAxis = 13;
constexpr int kVisibleTileRadius = kVisibleTilesPerAxis / 2;

static_assert(kVisibleTilesPerAxis % 2 == 1);

DisplayBounds fixed_display_bounds_around(const core::Location& center)
{
    return DisplayBounds{
        .min = core::Location{.x = center.x - kVisibleTileRadius,
                              .y = center.y - kVisibleTileRadius},
        .max = core::Location{.x = center.x + kVisibleTileRadius,
                              .y = center.y + kVisibleTileRadius}};
}

DisplayBounds visible_display_bounds(const core::MapView& view)
{
    if (view.player.has_value())
        return fixed_display_bounds_around(view.player->location);

    const DisplayBounds bounds = display_bounds(view);
    const core::Location center{.x = (bounds.min.x + bounds.max.x) / 2,
                                .y = (bounds.min.y + bounds.max.y) / 2};
    return fixed_display_bounds_around(center);
}
}  // namespace

float ui_scale()
{
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    if (display_size.x <= 0.0f || display_size.y <= 0.0f)
        return 1.0f;

    constexpr ImVec2 kDesignSize{1440.0f, 900.0f};
    return std::clamp(std::min(display_size.x / kDesignSize.x,
                               display_size.y / kDesignSize.y),
                      0.75f, 2.25f);
}

float scaled(const float value) { return value * ui_scale(); }

ImVec2 scaled(const ImVec2 value) { return ImVec2(scaled(value.x), scaled(value.y)); }

void apply_style()
{
    const float scale = ui_scale();
    ImGui::GetIO().FontGlobalScale = scale;

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};
    style.WindowRounding = 18.0f * scale;
    style.ChildRounding = 14.0f * scale;
    style.FrameRounding = 10.0f * scale;
    style.PopupRounding = 10.0f * scale;
    style.GrabRounding = 10.0f * scale;
    style.ScrollbarRounding = 10.0f * scale;
    style.TabRounding = 10.0f * scale;
    style.WindowPadding = scaled(ImVec2(16.0f, 16.0f));
    style.FramePadding = scaled(ImVec2(10.0f, 8.0f));
    style.ItemSpacing = scaled(ImVec2(10.0f, 10.0f));
    style.ItemInnerSpacing = scaled(ImVec2(8.0f, 6.0f));

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
    return result.solved();
}

void start_game(GamePlayState& state, std::unique_ptr<core::Game> game, std::string status)
{
    state.game = std::move(game);
    state.equation_result.reset();
    state.assignment_feedback.clear();
    state.removed_tiles.clear();
    state.status = std::move(status);
}

void clear_game(GamePlayState& state, std::string status)
{
    state.game.reset();
    state.equation_result.reset();
    state.assignment_feedback.clear();
    state.removed_tiles.clear();
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
    if (!state.equation_result->removed_objects.empty())
    {
        state.removed_tiles = state.equation_result->removed_objects;
        state.removed_tiles_at = ImGui::GetTime();
    }
    else if (event == core::Event::Undo)
    {
        state.removed_tiles.clear();
    }

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
    case core::Event::Undo:
        state.status = "Undid previous action.";
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

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        trigger_game_event(state, core::Event::Undo);
        return;
    }

    struct KeyBinding
    {
        ImGuiKey primary;
        ImGuiKey alternate;
        core::Event event;
    };

    constexpr std::array<KeyBinding, 6> bindings{{
        {ImGuiKey_W, ImGuiKey_UpArrow, core::Event::MoveUp},
        {ImGuiKey_A, ImGuiKey_LeftArrow, core::Event::MoveLeft},
        {ImGuiKey_S, ImGuiKey_DownArrow, core::Event::MoveDown},
        {ImGuiKey_D, ImGuiKey_RightArrow, core::Event::MoveRight},
        {ImGuiKey_E, ImGuiKey_None, core::Event::PickItem},
        {ImGuiKey_Q, ImGuiKey_None, core::Event::DropItem},
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

void draw_game_status(const GamePlayState& state)
{
    if (!state.game)
    {
        ImGui::TextWrapped("%s", state.status.c_str());
        return;
    }

    if (state.equation_result.has_value() && game_is_solved(*state.equation_result))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kSolvedTextColor);
        ImGui::TextWrapped("Solved. Load another map or reset to play again.");
        ImGui::PopStyleColor();
    }
    ImGui::TextWrapped("%s", state.status.c_str());
}

void draw_game_sidebar_state(const GamePlayState& state, const core::MapView& view)
{
    draw_game_inventory(view);
    draw_game_variables(state.equation_result);
    draw_game_assignment_feedback(state);
}

void draw_game_controls_info()
{
    ImGui::TextUnformatted("Controls");
    ImGui::TextWrapped("Move: WASD or arrow keys");
    ImGui::TextWrapped("Pick up: E");
    ImGui::TextWrapped("Drop: Q");
    ImGui::TextWrapped("Undo: Ctrl+Z");
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

    ImGui::PushStyleColor(ImGuiCol_Text, kAssignmentTextColor);
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
        ImGui::PushStyleColor(ImGuiCol_Button, kInventoryButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kInventoryButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kInventoryButtonActiveColor);
        char label[8];
        std::snprintf(label, sizeof(label), "%c", item.symbol);
        ImGui::Button(label, scaled(ImVec2(36.0f, 32.0f)));
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        if (i + 1 < view.player->inventory.size())
            ImGui::SameLine();
    }
}

ImU32 empty_tile_fill()
{
    return palette::empty_tile;
}

ImU32 object_tile_fill(const core::Object& object)
{
    if (object.manipulation_level == core::Object::ManipulationLevel::Pick) return palette::pickable_tile;
    if (object.manipulation_level == core::Object::ManipulationLevel::Push) return palette::pushable_tile;
    return palette::fixed_tile;
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
    const float gap = scaled(4.0f);
    const float rounding = scaled(12.0f);
    const ImVec2 cell_max(cell_min.x + ctx.tile_size - gap, cell_min.y + ctx.tile_size - gap);
    ImU32 fill = object == nullptr ? empty_tile_fill() : object_tile_fill(*object);
    ImU32 outline =
        ctx.solved ? palette::solved_tile_outline : palette::tile_outline;
    float thickness = scaled(ctx.solved ? 3.0f : 2.0f);

    const core::Location location{.x = ctx.min_location.x + x, .y = ctx.min_location.y + y};
    if (object != nullptr && object->symbol == '=' && ctx.equation_result != nullptr)
    {
        if (is_remaining_equal_sign(*ctx.equation_result, location.x, location.y))
        {
            outline = palette::incorrect_equation;
            thickness = scaled(3.0f);
        }
    }

    ctx.draw_list->AddRectFilled(cell_min, cell_max, fill, rounding);
    ctx.draw_list->AddRect(cell_min, cell_max, outline, rounding, 0, thickness);
    if (object != nullptr)
        draw_tile_symbol(*ctx.draw_list, cell_min, cell_max, object->symbol, ctx.tile_size * 0.58f);
}

void draw_player_tile(const GridRenderContext& ctx, const core::Player& player)
{
    const int x = player.location.x - ctx.min_location.x;
    const int y = player.location.y - ctx.min_location.y;
    const ImVec2 cell_min(ctx.origin.x + x * ctx.tile_size, ctx.origin.y + y * ctx.tile_size);
    const float gap = scaled(4.0f);
    const float rounding = scaled(12.0f);
    const ImVec2 cell_max(cell_min.x + ctx.tile_size - gap, cell_min.y + ctx.tile_size - gap);
    ctx.draw_list->AddRectFilled(cell_min, cell_max, palette::player_fill, rounding);
    ctx.draw_list->AddRect(cell_min, cell_max, palette::player_outline, rounding, 0, scaled(3.0f));
    draw_tile_symbol(*ctx.draw_list, cell_min, cell_max, player.symbol, ctx.tile_size * 0.58f);
}

void draw_game_solved_badge(ImDrawList& draw_list, const ImVec2 origin)
{
    const char* message = "Equation Solved";
    ImFont* font = ImGui::GetFont();
    const float font_size = scaled(30.0f);
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, message);
    const ImVec2 badge_min(origin.x + scaled(18.0f), origin.y + scaled(18.0f));
    const ImVec2 badge_max(badge_min.x + text_size.x + scaled(28.0f),
                           badge_min.y + text_size.y + scaled(18.0f));
    const ImVec2 text_pos(badge_min.x + scaled(14.0f), badge_min.y + scaled(9.0f));
    draw_list.AddRectFilled(badge_min, badge_max, palette::solved_badge, scaled(12.0f));
    draw_list.AddText(font, font_size, text_pos, palette::solved_badge_text, message);
}

void draw_game_grid_background(ImDrawList& draw_list, const ImVec2 origin, const ImVec2 board_max,
                               const bool solved)
{
    draw_list.AddRectFilled(origin, board_max,
                            solved ? palette::solved_board_background : palette::board_background,
                            scaled(18.0f));
    if (!solved)
        return;

    draw_list.AddRect(origin, board_max, palette::solved_board_outline, scaled(18.0f), 0,
                      scaled(4.0f));
    draw_list.AddRect(origin, board_max, palette::solved_board_glow, scaled(18.0f), 0,
                      scaled(10.0f));
}

void draw_game_grid(const core::MapView& view, const std::optional<core::GameResult>& result,
                    const GamePlayState* state)
{
    const bool solved = result.has_value() && game_is_solved(*result);
    const DisplayBounds bounds = visible_display_bounds(view);
    const int width = display_width(bounds);
    const int height = display_height(bounds);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float fit_size = std::min(available.x / static_cast<float>(width),
                                    available.y / static_cast<float>(height));
    const float tile_size = std::clamp(fit_size, scaled(24.0f), scaled(96.0f));
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
    constexpr double kRemovalEffectDuration = 0.7;
    if (state != nullptr && !state->removed_tiles.empty())
    {
        const float progress = static_cast<float>(
            std::clamp((ImGui::GetTime() - state->removed_tiles_at) / kRemovalEffectDuration,
                       0.0, 1.0));
        if (progress < 1.0f)
        {
            const ImU32 alpha = static_cast<ImU32>(255.0f * (1.0f - progress));
            const auto with_alpha = [alpha](const ImU32 color) {
                return (color & IM_COL32(255, 255, 255, 0)) | (alpha << IM_COL32_A_SHIFT);
            };
            for (const auto& [location, object] : state->removed_tiles)
            {
                const int x = location.x - bounds.min.x;
                const int y = location.y - bounds.min.y;
                if (x < 0 || x >= width || y < 0 || y >= height)
                    continue;

                const ImVec2 center(origin.x + (x + 0.5f) * tile_size,
                                    origin.y + (y + 0.5f) * tile_size);
                const float half_size = (tile_size - scaled(4.0f)) * 0.5f *
                                        (1.0f - 0.25f * progress);
                const ImVec2 tile_min(center.x - half_size, center.y - half_size);
                const ImVec2 tile_max(center.x + half_size, center.y + half_size);
                draw_list->AddRectFilled(tile_min, tile_max, with_alpha(object_tile_fill(object)),
                                         scaled(12.0f));
                draw_tile_symbol(*draw_list, tile_min, tile_max, object.symbol,
                                 tile_size * 0.58f, with_alpha(palette::tile_symbol));
            }
        }
    }
    if (view.player.has_value())
        draw_player_tile(ctx, *view.player);
    if (solved)
        draw_game_solved_badge(*draw_list, origin);

    ImGui::Dummy(ImVec2(total_size.x, total_size.y));
}
}  // namespace gui
