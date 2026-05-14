#pragma once

#include "core/game.hpp"
#include "core/map.hpp"

#include <imgui.h>

#include <memory>
#include <optional>
#include <string>

namespace gui
{
struct GamePlayState
{
    std::unique_ptr<core::Game> game;
    std::optional<core::GameResult> equation_result;
    std::string status{"Ready."};
    std::string assignment_feedback;
};

void apply_style();

bool game_is_solved(const core::GameResult& result);
void start_game(GamePlayState& state, std::unique_ptr<core::Game> game, std::string status);
void clear_game(GamePlayState& state, std::string status);
bool trigger_game_event(GamePlayState& state, core::Event event);
void handle_game_keyboard(GamePlayState& state);
bool game_action_button(const char* label, ImVec2 size, GamePlayState& state, core::Event event);
void draw_game_status(const GamePlayState& state);
void draw_game_map_stats(const core::MapView& view);
void draw_game_sidebar_state(const GamePlayState& state, const core::MapView& view);
void draw_game_action_controls(GamePlayState& state);
void draw_game_variables(const std::optional<core::GameResult>& result);
void draw_game_assignment_feedback(const GamePlayState& state);
void draw_game_inventory(const core::MapView& view);
void draw_game_grid(const core::MapView& view, const std::optional<core::GameResult>& result);

ImU32 tile_fill(const core::CellView& cell);
ImU32 tile_outline(const core::CellView& cell);
void draw_tile_symbol(ImDrawList& draw_list, const ImVec2& cell_min, const ImVec2& cell_max, char symbol,
                      float font_size, ImU32 color = IM_COL32(245, 242, 233, 255));
}  // namespace gui
