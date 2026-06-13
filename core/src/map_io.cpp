#include "map_io.hpp"

#include "object.hpp"
#include "player.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

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

bool is_player_symbol(const char symbol)
{
    return symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>';
}

bool tile_is_player(const json &tile)
{
    const std::string symbol_text = tile.at("symbol").get<std::string>();
    return symbol_text == "Player" || (symbol_text.size() == 1 && is_player_symbol(symbol_text[0]));
}

internal::PlayerState player_from_tile(const json &tile)
{
    const std::string symbol_text = tile.at("symbol").get<std::string>();
    const internal::Facing facing =
        symbol_text == "Player" ? internal::Facing::South : player_facing_from_symbol(symbol_text[0]);
    return internal::PlayerState{
        .player = core::Player{.location = core::Location{.x = tile.at("x").get<int>(),
                                                          .y = tile.at("y").get<int>()}},
        .facing = facing};
}

core::Object object_from_tile(const json &tile)
{
    const bool pushable = tile.value("pushable", false);
    const bool pickable = tile.value("pickable", false);
    const std::string symbol_text = tile.at("symbol").get<std::string>();
    const core::Object::ManipulationLevel manipulation_level =
        pickable ? core::Object::ManipulationLevel::Pick
                 : (pushable ? core::Object::ManipulationLevel::Push
                             : core::Object::ManipulationLevel::None);

    if (symbol_text.size() != 1)
        throw std::runtime_error("symbol must be one character or 'Player'");

    const char symbol = symbol_text[0];
    return internal::MakeObject(symbol).with_manipulation_level(manipulation_level);
}

json tile_from_object(const core::Object &object, const Location &location)
{
    json tile{{"x", location.x}, {"y", location.y}, {"symbol", std::string(1, object.symbol)}};

    if (object.manipulation_level != core::Object::ManipulationLevel::None)
        tile["pushable"] = true;
    if (object.manipulation_level == core::Object::ManipulationLevel::Pick)
        tile["pickable"] = true;
    return tile;
}

json tile_from_player(const internal::PlayerState &player)
{
    return json{{"x", player.player.location.x},
                {"y", player.player.location.y},
                {"symbol", std::string(1, internal::glyph(player))}};
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

    internal::Map map;
    map.commits_left = root.value("resources", json::object()).value("commits", 6);
    map.undos_left = root.value("resources", json::object()).value("undos", 6);

    std::set<Location> occupied;
    const auto &tiles = root.at("tiles");
    for (const auto &tile : tiles)
    {
        const int x = tile.at("x").get<int>();
        const int y = tile.at("y").get<int>();
        const Location location{.x = x, .y = y};
        if (occupied.count(location) != 0)
            throw std::runtime_error("Multiple tiles at the same position");
        occupied.insert(location);

        if (tile_is_player(tile))
        {
            if (map.player.has_value())
                throw std::runtime_error("Multiple player positions found");
            map.player = player_from_tile(tile);
            continue;
        }

        map.objects.insert_or_assign(location, object_from_tile(tile));
    }

    if (require_player && !map.player.has_value())
    {
        throw std::runtime_error("Player position not found");
    }
    return map;
}

std::string map_to_json(const internal::Map &map)
{
    json root;
    root["version"] = 1;
    root["resources"] = {{"commits", map.commits_left}, {"undos", map.undos_left}};

    json tiles = json::array();
    if (map.player.has_value())
        tiles.push_back(tile_from_player(*map.player));

    for (const auto &[location, object] : map.objects)
    {
        tiles.push_back(tile_from_object(object.view(), location));
    }

    root["tiles"] = tiles;
    return root.dump(2);
}

internal::Map map_from_file(const std::filesystem::path &path, const bool require_player)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Failed to open map file: " + path.string());

    const std::string text{std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>()};
    return map_from_json(text, require_player);
}

void map_to_file(const internal::Map &map, const std::filesystem::path &path)
{
    std::ofstream output(path);
    if (!output)
        throw std::runtime_error("Failed to write map file: " + path.string());

    output << map_to_json(map);
    if (!output)
        throw std::runtime_error("Failed to finish writing map file: " + path.string());
}
} // namespace core::map_io
