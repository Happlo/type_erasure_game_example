#include "core/map_builder.hpp"

#include "game_factory.hpp"
#include "game_model.hpp"
#include "map_io.hpp"
#include "player.hpp"
#include "solution_rules.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace core::internal;
namespace core
{
namespace
{
internal::Facing player_facing_from_symbol(const char symbol)
{
    switch (symbol)
    {
    case '^':
        return internal::Facing::North;
    case 'v':
        return internal::Facing::South;
    case '<':
        return internal::Facing::West;
    case '>':
        return internal::Facing::East;
    }

    throw std::runtime_error("Invalid player symbol");
}

bool is_player_symbol(const char symbol)
{
    return symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>';
}

internal::Map make_empty_map()
{
    internal::Map map;
    map.player =
        internal::PlayerState{.player = core::Player{.location = core::Location{.x = 0, .y = 0}},
                              .facing = internal::Facing::South};
    return map;
}

std::filesystem::path resolve_save_path(const std::filesystem::path &save_directory,
                                        const std::filesystem::path &requested_path)
{
    if (save_directory.empty())
        return requested_path;

    std::filesystem::path filename = requested_path.filename();
    if (filename.empty() || filename == "." || filename == "..")
        throw std::runtime_error("Map filename must not be empty");
    if (!filename.has_extension())
        filename += ".json";

    std::error_code error;
    std::filesystem::create_directories(save_directory, error);
    if (error)
        throw std::runtime_error("Failed to create map directory: " + error.message());

    return save_directory / filename;
}

class DefaultMapBuilder final : public MapBuilder
{
  public:
    explicit DefaultMapBuilder(std::filesystem::path save_directory = {})
        : map_(make_empty_map()), save_directory_(std::move(save_directory))
    {
    }

    explicit DefaultMapBuilder(internal::Map map, std::filesystem::path save_directory = {})
        : map_(std::move(map)), save_directory_(std::move(save_directory))
    {
    }

    const MapView &view() const override
    {
        view_ = internal::build_view(map_);
        return view_;
    }

    MapBuilder &set_commits_left(const int commits_left) override
    {
        map_.commits_left = commits_left;
        return *this;
    }

    MapBuilder &set_undos_left(const int undos_left) override
    {
        map_.undos_left = undos_left;
        return *this;
    }

    MapBuilder &add_object(const Location &location, const Object &object) override
    {
        if (is_player_symbol(object.symbol))
        {
            map_.player = internal::PlayerState{
                .player = core::Player{.location = location},
                .facing = player_facing_from_symbol(object.symbol)};
            map_.objects.erase(location);
            return *this;
        }

        if (map_.player.has_value() && map_.player->player.location == location)
            map_.player = std::nullopt;

        map_.objects.insert_or_assign(location, object);
        return *this;
    }

    MapBuilder &clear_cell(const Location &location) override
    {
        map_.objects.erase(location);
        if (map_.player.has_value() && map_.player->player.location == location)
            map_.player = std::nullopt;
        return *this;
    }

    std::unique_ptr<Game> create_game() const override
    {
        if (!internal::find_player(map_).has_value())
            throw std::runtime_error("Player position not found");
        return internal::make_game(map_);
    }

    void save_to_file(const std::filesystem::path &path) const override
    {
        map_io::map_to_file(map_, resolve_save_path(save_directory_, path));
    }

  private:
    mutable MapView view_;
    internal::Map map_;
    std::filesystem::path save_directory_;
};
} // namespace

std::unique_ptr<MapBuilder> MapBuilder::create_default(const std::filesystem::path &save_directory)
{
    return std::make_unique<DefaultMapBuilder>(save_directory);
}

std::unique_ptr<MapBuilder> MapBuilder::create(const std::filesystem::path &save_directory)
{
    return std::make_unique<DefaultMapBuilder>(make_empty_map(), save_directory);
}

std::unique_ptr<MapBuilder> MapBuilder::load_from_file(const std::filesystem::path &path)
{
    return std::make_unique<DefaultMapBuilder>(map_io::map_from_file(path, false));
}

const std::vector<char> &MapBuilder::solver_operators() { return solution_rules::OPERATORS; }

const std::vector<char> &MapBuilder::equation_delimiters()
{
    return solution_rules::EQUATION_DELIMITERS;
}

// std::optional<std::unique_ptr<MapBuilder>> MapBuilder::from_json(const std::string &text,
//                                                                  std::string &error_message)
// {
//     try
//     {
//         return std::make_optional<std::unique_ptr<MapBuilder>>(
//             std::make_unique<DefaultMapBuilder>(map_io::map_from_json(text, false)));
//     }
//     catch (const std::exception &ex)
//     {
//         error_message = ex.what();
//         return std::nullopt;
//     }
// }

} // namespace core
