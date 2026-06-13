#pragma once

#include "core/map_builder.hpp"
#include "shared.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>

namespace gui
{
enum class BuilderMode
{
    Edit,
    TryMap,
};

struct BuilderEditorState
{
    struct CellSelection
    {
        int x;
        int y;
    };

    std::unique_ptr<core::MapBuilder> map;
    core::Brush brush;
    std::string status{"Ready."};
    std::array<char, 2> symbol_buffer{brush.symbol, '\0'};
    std::array<char, 256> file_path{};
    std::optional<CellSelection> selected_cell;
    BuilderMode mode{BuilderMode::Edit};
    GamePlayState try_game;
};

void start_builder_editor(BuilderEditorState &state, std::unique_ptr<core::MapBuilder> map,
                          const char *file_path, std::string status);
void clear_builder_editor(BuilderEditorState &state);
bool builder_editor_active(const BuilderEditorState &state);
void handle_builder_editor_keyboard(BuilderEditorState &state);
bool draw_builder_editor(BuilderEditorState &state, bool show_back_button = false);
} // namespace gui
