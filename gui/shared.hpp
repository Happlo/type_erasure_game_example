#pragma once

#include "core/game.hpp"
#include "core/map.hpp"

#include <imgui.h>

#include <memory>
#include <optional>
#include <string>

namespace gui
{
namespace palette
{
inline constexpr ImU32 tile_symbol = IM_COL32(245, 242, 233, 255);
inline constexpr ImU32 empty_tile = IM_COL32(31, 37, 43, 255);
inline constexpr ImU32 fixed_tile = IM_COL32(116, 123, 132, 255);
inline constexpr ImU32 pickable_tile = IM_COL32(97, 147, 196, 255);
inline constexpr ImU32 pushable_tile = IM_COL32(189, 112, 143, 255);
inline constexpr ImU32 tile_outline = IM_COL32(72, 79, 88, 255);
inline constexpr ImU32 selected_tile_outline = IM_COL32(255, 231, 168, 255);
inline constexpr ImU32 player_fill = IM_COL32(236, 177, 79, 255);
inline constexpr ImU32 player_outline = IM_COL32(255, 226, 157, 255);
inline constexpr ImU32 correct_equation = IM_COL32(154, 236, 170, 255);
inline constexpr ImU32 incorrect_equation = IM_COL32(235, 126, 126, 255);
inline constexpr ImU32 solved_tile_outline = IM_COL32(182, 240, 175, 255);
inline constexpr ImU32 board_background = IM_COL32(20, 24, 29, 255);
inline constexpr ImU32 solved_board_background = IM_COL32(23, 43, 30, 255);
inline constexpr ImU32 solved_board_outline = IM_COL32(137, 224, 151, 255);
inline constexpr ImU32 solved_board_glow = IM_COL32(195, 255, 204, 120);
inline constexpr ImU32 solved_badge = IM_COL32(97, 181, 92, 235);
inline constexpr ImU32 solved_badge_text = IM_COL32(250, 255, 247, 255);
} // namespace palette

struct GamePlayState
{
    std::unique_ptr<core::Game> game;
    std::optional<core::GameResult> equation_result;
    std::string status{"Ready."};
    std::string assignment_feedback;
    std::map<core::Location, core::Object> removed_tiles;
    double removed_tiles_at{0.0};
};

void apply_style();
float ui_scale();
float scaled(float value);
ImVec2 scaled(ImVec2 value);

bool game_is_solved(const core::GameResult& result);
void start_game(GamePlayState& state, std::unique_ptr<core::Game> game, std::string status);
void clear_game(GamePlayState& state, std::string status);
bool trigger_game_event(GamePlayState& state, core::Event event);
void handle_game_keyboard(GamePlayState& state);
void draw_game_status(const GamePlayState& state);
void draw_game_sidebar_state(const GamePlayState& state, const core::MapView& view);
void draw_game_controls_info();
void draw_game_variables(const std::optional<core::GameResult>& result);
void draw_game_assignment_feedback(const GamePlayState& state);
void draw_game_inventory(const core::MapView& view);
void draw_game_grid(const core::MapView& view, const std::optional<core::GameResult>& result,
                    const GamePlayState* state = nullptr);

ImU32 empty_tile_fill();
ImU32 object_tile_fill(const core::Object& object);
void draw_tile_symbol(ImDrawList& draw_list, const ImVec2& cell_min, const ImVec2& cell_max, char symbol,
                      float font_size, ImU32 color = palette::tile_symbol);
}  // namespace gui
