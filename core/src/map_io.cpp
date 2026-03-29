#include "map_io.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace core::map_io
{
namespace
{
using nlohmann::json;

internal::Facing player_facing_from_symbol(const char symbol)
{
    if (symbol == '^')
        return internal::Facing::North;
    if (symbol == 'v')
        return internal::Facing::South;
    if (symbol == '<')
        return internal::Facing::West;
    if (symbol == '>')
        return internal::Facing::East;
    throw std::runtime_error("Invalid player symbol; expected one of '^', 'v', '<', '>'");
}

internal::Object object_from_tile(const json &tile)
{
    const bool pushable = tile.value("pushable", false);
    const bool pickable = tile.value("pickable", false);
    const std::string symbol_text = tile.at("symbol").get<std::string>();
    const core::Object::ManipulationLevel manipulation_level =
        pickable ? core::Object::ManipulationLevel::Pick
                 : (pushable ? core::Object::ManipulationLevel::Push : core::Object::ManipulationLevel::None);

    if (symbol_text == "Player")
        return internal::Object(internal::PlayerState{}, manipulation_level);
    if (symbol_text.size() != 1)
        throw std::runtime_error("symbol must be one character or 'Player'");

    const char symbol = symbol_text[0];
    if (symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>')
    {
        return internal::Object(internal::PlayerState{.facing = player_facing_from_symbol(symbol)}, manipulation_level);
    }
    return internal::Object(symbol, manipulation_level);
}

json tile_from_object(const internal::Object &object, int x, int y)
{
    const core::CellView view = object.view();
    json tile{{"x", x}, {"y", y}, {"symbol", std::string(1, core::symbol_of(view))}};

    if (const auto *props = std::get_if<core::Object>(&view); props != nullptr)
    {
        if (props->manipulation_level != core::Object::ManipulationLevel::None)
            tile["pushable"] = true;
        if (props->manipulation_level == core::Object::ManipulationLevel::Pick)
            tile["pickable"] = true;
    }
    return tile;
}
} // namespace

internal::Map map_from_json(std::string_view json_text, const bool require_player)
{
    json root;
    try
    {
        root = json::parse(json_text);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Invalid map JSON: ") + ex.what());
    }

    const int width = root.at("size").at("width").get<int>();
    const int height = root.at("size").at("height").get<int>();
    if (width <= 0 || height <= 0)
        throw std::runtime_error("Map size must be positive");

    internal::Map map;
    map.commits_left = root.value("resources", json::object()).value("commits", 6);
    map.undos_left = root.value("resources", json::object()).value("undos", 6);
    map.grid.assign(static_cast<size_t>(height),
                    std::vector<internal::Object>(static_cast<size_t>(width), Empty{}));

    std::unordered_set<long long> occupied;
    const auto &tiles = root.at("tiles");
    for (const auto &tile : tiles)
    {
        const int x = tile.at("x").get<int>();
        const int y = tile.at("y").get<int>();
        if (x < 0 || y < 0 || x >= width || y >= height)
        {
            throw std::runtime_error("Tile is out of map bounds");
        }

        const long long key = (static_cast<long long>(y) << 32) | static_cast<unsigned int>(x);
        if (occupied.count(key) != 0)
            throw std::runtime_error("Multiple tiles at the same position");
        occupied.insert(key);
        map.grid[y][x] = object_from_tile(tile);
    }

    if (require_player && !internal::find_player(map).has_value())
    {
        throw std::runtime_error("Player position not found");
    }
    return map;
}

std::string map_to_json(const internal::Map &map)
{
    json root;
    root["version"] = 1;
    root["size"] = {{"width", internal::grid_width(map)}, {"height", internal::grid_height(map)}};
    root["resources"] = {{"commits", map.commits_left}, {"undos", map.undos_left}};

    json tiles = json::array();
    for (int y = 0; y < internal::grid_height(map); ++y)
    {
        for (int x = 0; x < internal::grid_width(map); ++x)
        {
            const auto &obj = map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (std::holds_alternative<core::Empty>(obj.view()))
                continue;
            tiles.push_back(tile_from_object(obj, x, y));
        }
    }

    root["tiles"] = tiles;
    return root.dump(2);
}
} // namespace core::map_io
