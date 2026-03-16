#include "core/map_builder.hpp"

#include "game_model.hpp"
#include "map_io.hpp"

#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

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

template <typename T>
internal::Object make_object(T value, const bool pushable = false, const bool pickable = false)
{
    auto builder = internal::MakeObject(std::move(value));
    if (pushable)
        builder = builder.pushable();
    if (pickable)
        builder = builder.pickable();
    return std::move(builder).build();
}

bool is_player_symbol(const char symbol)
{
    return symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>';
}

internal::Map make_empty_map(const int width, const int height)
{
    if (width <= 0 || height <= 0)
        throw std::runtime_error("Map size must be positive");

    internal::Map map;
    map.grid.assign(
        static_cast<size_t>(height),
        std::vector<internal::Object>(static_cast<size_t>(width), Empty{}));
    return map;
}

class DefaultMapBuilder final : public MapBuilder
{
  public:
    DefaultMapBuilder() { sync_view(); }

    explicit DefaultMapBuilder(internal::Map map) : map_(std::move(map)) { sync_view(); }

    const MapView &view() const override { return view_; }

    const CellView &at(const int x, const int y) const override { return view_.at(x, y); }

    void set_commits_left(const int commits_left) override
    {
        map_.commits_left = commits_left;
        sync_view();
    }

    void set_undos_left(const int undos_left) override
    {
        map_.undos_left = undos_left;
        sync_view();
    }

    void resize(const int new_width, const int new_height) override
    {
        std::vector<std::vector<internal::Object>> next(
            static_cast<size_t>(new_height),
            std::vector<internal::Object>(static_cast<size_t>(new_width), Empty{}));
        for (int y = 0; y < new_height && y < internal::grid_height(map_); ++y)
        {
            for (int x = 0; x < new_width && x < internal::grid_width(map_); ++x)
            {
                next[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                    std::move(map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)]);
            }
        }

        map_.grid = std::move(next);
        sync_view();
    }

    void apply_brush(const int x, const int y, const Brush &brush) override
    {
        if (is_player_symbol(brush.symbol))
        {
            map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                internal::Object(internal::PlayerState{.facing = player_facing_from_symbol(brush.symbol)});
            ensure_single_player(x, y);
            sync_view();
            return;
        }

        if ('0' <= brush.symbol && brush.symbol <= '9')
        {
            map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                make_object(brush.symbol - '0', brush.pushable, brush.pickable);
        }
        else
        {
            map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                make_object(brush.symbol, brush.pushable, brush.pickable);
        }
        sync_view();
    }

    void clear_cell(const int x, const int y) override
    {
        map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = make_object(Empty{});
        sync_view();
    }

    std::string to_json() const override { return map_io::map_to_json(map_); }

  private:
    void ensure_single_player(const int keep_x, const int keep_y)
    {
        for (int y = 0; y < internal::grid_height(map_); ++y)
        {
            for (int x = 0; x < internal::grid_width(map_); ++x)
            {
                if (x == keep_x && y == keep_y)
                    continue;
                auto &cell = map_.grid[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (std::holds_alternative<Player>(cell.view()))
                {
                    cell = make_object(Empty{});
                }
            }
        }
    }

    void sync_view() { view_ = internal::build_view(map_); }

    MapView view_;
    internal::Map map_ = internal::make_map();
};
} // namespace

std::unique_ptr<MapBuilder> MapBuilder::create_default()
{
    return std::make_unique<DefaultMapBuilder>();
}

std::unique_ptr<MapBuilder> MapBuilder::create(const int width, const int height)
{
    return std::make_unique<DefaultMapBuilder>(make_empty_map(width, height));
}

std::optional<std::unique_ptr<MapBuilder>> MapBuilder::from_json(const std::string &text,
                                                                 std::string &error_message)
{
    try
    {
        return std::make_optional<std::unique_ptr<MapBuilder>>(
            std::make_unique<DefaultMapBuilder>(map_io::map_from_json(text)));
    }
    catch (const std::exception &ex)
    {
        error_message = ex.what();
        return std::nullopt;
    }
}

} // namespace core
