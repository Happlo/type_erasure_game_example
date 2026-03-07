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

internal::Player::Facing parse_facing(const std::string& facing)
{
    if (facing == "north") return internal::Player::Facing::North;
    if (facing == "south") return internal::Player::Facing::South;
    if (facing == "west") return internal::Player::Facing::West;
    if (facing == "east") return internal::Player::Facing::East;
    throw std::runtime_error("Invalid player facing: " + facing);
}

internal::Object object_from_tile(const json& tile)
{
    const std::string kind = tile.at("kind").get<std::string>();
    const bool pushable = tile.value("pushable", false);
    const bool blocking = tile.value("blocking", false);

    if (kind == "empty") return internal::Object(internal::Empty {});
    if (kind == "player")
    {
        const std::string facing = tile.value("facing", std::string("south"));
        return internal::Object(internal::Player {parse_facing(facing)}, pushable, blocking);
    }
    if (kind == "number")
    {
        const int value = tile.at("value").get<int>();
        return internal::Object(value, pushable, blocking);
    }
    if (kind == "plus") return internal::Object('+', pushable, blocking);
    if (kind == "equals") return internal::Object('=', pushable, blocking);
    if (kind == "wall") return internal::Object('#', pushable, true);
    if (kind == "symbol")
    {
        const std::string glyph = tile.at("glyph").get<std::string>();
        if (glyph.size() != 1) throw std::runtime_error("symbol glyph must be one character");
        return internal::Object(glyph[0], pushable, blocking);
    }

    throw std::runtime_error("Invalid tile kind: " + kind);
}

json tile_from_object(const internal::Object& object, int x, int y)
{
    const auto props = object.properties();
    json tile {
        {"x", x},
        {"y", y}
    };

    if (props.is_player)
    {
        tile["kind"] = "player";
        if (props.glyph == '^') tile["facing"] = "north";
        else if (props.glyph == 'v') tile["facing"] = "south";
        else if (props.glyph == '<') tile["facing"] = "west";
        else if (props.glyph == '>') tile["facing"] = "east";
        else tile["facing"] = "south";
    }
    else if (props.number_value >= 0)
    {
        tile["kind"] = "number";
        tile["value"] = props.number_value;
    }
    else if (props.glyph == '+')
    {
        tile["kind"] = "plus";
    }
    else if (props.glyph == '=')
    {
        tile["kind"] = "equals";
    }
    else
    {
        tile["kind"] = "symbol";
        tile["glyph"] = std::string(1, props.glyph);
    }

    if (props.is_pushable) tile["pushable"] = true;
    if (props.blocks_movement) tile["blocking"] = true;
    return tile;
}
}  // namespace

internal::Map map_from_json(std::string_view json_text)
{
    json root;
    try
    {
        root = json::parse(json_text);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("Invalid map JSON: ") + ex.what());
    }

    const int width = root.at("size").at("width").get<int>();
    const int height = root.at("size").at("height").get<int>();
    if (width <= 0 || height <= 0) throw std::runtime_error("Map size must be positive");

    internal::Map map;
    map.commits_left = root.value("resources", json::object()).value("commits", 6);
    map.undos_left = root.value("resources", json::object()).value("undos", 6);
    map.grid.assign(static_cast<size_t>(height), std::vector<internal::Object>(static_cast<size_t>(width), internal::Empty {}));

    std::unordered_set<long long> occupied;
    const auto& tiles = root.at("tiles");
    for (const auto& tile : tiles)
    {
        const int x = tile.at("x").get<int>();
        const int y = tile.at("y").get<int>();
        if (x < 0 || y < 0 || x >= width || y >= height)
        {
            throw std::runtime_error("Tile is out of map bounds");
        }

        const long long key = (static_cast<long long>(y) << 32) | static_cast<unsigned int>(x);
        if (occupied.count(key) != 0) throw std::runtime_error("Multiple tiles at the same position");
        occupied.insert(key);
        map.grid[y][x] = object_from_tile(tile);
    }

    (void)internal::find_player(map);
    return map;
}

std::string map_to_json(const internal::Map& map)
{
    json root;
    root["version"] = 1;
    root["size"] = {
        {"width", internal::grid_width(map)},
        {"height", internal::grid_height(map)}
    };
    root["resources"] = {
        {"commits", map.commits_left},
        {"undos", map.undos_left}
    };

    json tiles = json::array();
    for (int y = 0; y < internal::grid_height(map); ++y)
    {
        for (int x = 0; x < internal::grid_width(map); ++x)
        {
            const auto& obj = map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)];
            const auto props = obj.properties();
            if (props.is_empty) continue;
            tiles.push_back(tile_from_object(obj, x, y));
        }
    }

    root["tiles"] = tiles;
    return root.dump(2);
}
}  // namespace core::map_io
