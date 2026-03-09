#pragma once

#include "core/map.hpp"

#include <optional>
#include <string>

namespace core
{
struct Brush
{
    char symbol {'*'};
    bool pushable {false};
};

class MapBuilder
{
public:
    MapView view;

    static MapBuilder make_default();
    static std::optional<MapBuilder> from_json(const std::string& text, std::string& error_message);

    const CellView& at(int x, int y) const;
    CellView& at(int x, int y);

    void resize(int new_width, int new_height);
    void apply_brush(int x, int y, const Brush& brush);
    void clear_cell(int x, int y);

    std::string to_json() const;

    static char glyph_for_cell(const CellView& cell);

private:
    static bool is_player_symbol(char symbol);
    static CellView empty_cell();
    static CellView cell_from_brush(const Brush& brush);
    void ensure_single_player(int keep_x, int keep_y);
};
}  // namespace core
