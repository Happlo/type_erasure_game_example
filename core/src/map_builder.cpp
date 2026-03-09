#include "core/map_builder.hpp"

#include "core/game.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

namespace core
{
MapBuilder MapBuilder::make_default()
{
    MapBuilder map;
    map.view = Game::create_default()->view();
    return map;
}

std::optional<MapBuilder> MapBuilder::from_json(const std::string& text, std::string& error_message)
{
    try
    {
        MapBuilder map;
        map.view = Game::from_json(text)->view();
        return map;
    }
    catch (const std::exception& ex)
    {
        error_message = ex.what();
        return std::nullopt;
    }
}

const CellView& MapBuilder::at(const int x, const int y) const
{
    return view.at(x, y);
}

CellView& MapBuilder::at(const int x, const int y)
{
    return view.cells[static_cast<size_t>(y * view.width + x)];
}

void MapBuilder::resize(const int new_width, const int new_height)
{
    std::vector<CellView> next(static_cast<size_t>(new_width * new_height), empty_cell());
    for (int y = 0; y < new_height && y < view.height; ++y)
    {
        for (int x = 0; x < new_width && x < view.width; ++x)
        {
            next[static_cast<size_t>(y * new_width + x)] = at(x, y);
        }
    }

    view.width = new_width;
    view.height = new_height;
    view.cells = std::move(next);
}

void MapBuilder::apply_brush(const int x, const int y, const Brush& brush)
{
    at(x, y) = cell_from_brush(brush);
    if (is_player_symbol(brush.symbol)) ensure_single_player(x, y);
}

void MapBuilder::clear_cell(const int x, const int y)
{
    at(x, y) = empty_cell();
}

std::string MapBuilder::to_json() const
{
    nlohmann::json root;
    root["version"] = 1;
    root["size"] = {{"width", view.width}, {"height", view.height}};
    root["resources"] = {{"commits", view.commits_left}, {"undos", view.undos_left}};

    nlohmann::json tiles = nlohmann::json::array();
    for (int y = 0; y < view.height; ++y)
    {
        for (int x = 0; x < view.width; ++x)
        {
            const CellView& cell = at(x, y);
            if (std::holds_alternative<Empty>(cell.properties)) continue;

            nlohmann::json tile {{"x", x}, {"y", y}, {"symbol", std::string(1, cell.symbol)}};
            if (const auto* props = std::get_if<Object>(&cell.properties); props != nullptr)
            {
                if (props->is_pushable) tile["pushable"] = true;
            }
            tiles.push_back(std::move(tile));
        }
    }

    root["tiles"] = std::move(tiles);
    return root.dump(2);
}

char MapBuilder::glyph_for_cell(const CellView& cell)
{
    return std::holds_alternative<Empty>(cell.properties) ? ' ' : cell.symbol;
}

bool MapBuilder::is_player_symbol(const char symbol)
{
    return symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>';
}

CellView MapBuilder::empty_cell()
{
    return CellView {.symbol = ' ', .properties = Empty {}};
}

CellView MapBuilder::cell_from_brush(const Brush& brush)
{
    if (is_player_symbol(brush.symbol)) return CellView {.symbol = brush.symbol, .properties = Player {}};

    return CellView {
        .symbol = brush.symbol,
        .properties = Object {.is_pushable = brush.pushable},
    };
}

void MapBuilder::ensure_single_player(const int keep_x, const int keep_y)
{
    for (int y = 0; y < view.height; ++y)
    {
        for (int x = 0; x < view.width; ++x)
        {
            if (x == keep_x && y == keep_y) continue;
            CellView& cell = at(x, y);
            if (std::holds_alternative<Player>(cell.properties)) cell = empty_cell();
        }
    }
}
}  // namespace core
