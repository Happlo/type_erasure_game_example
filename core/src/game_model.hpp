#pragma once

#include "core/map.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace core::internal
{
struct Point
{
    int x {};
    int y {};
};

struct Empty
{};

struct Player
{
    enum class Facing
    {
        North,
        South,
        West,
        East
    };

    Facing facing {Facing::South};

    static Player from_delta(int dx, int dy)
    {
        if (dx == 0 && dy < 0) return {Facing::North};
        if (dx == 0 && dy > 0) return {Facing::South};
        if (dx < 0 && dy == 0) return {Facing::West};
        if (dx > 0 && dy == 0) return {Facing::East};
        return {};
    }
};

inline char glyph(const Player& player)
{
    switch (player.facing)
    {
        case Player::Facing::North: return '^';
        case Player::Facing::South: return 'v';
        case Player::Facing::West: return '<';
        case Player::Facing::East: return '>';
    }
    return '@';
}

inline char glyph(const Empty&)
{
    return ' ';
}

inline char glyph(const int& value)
{
    if (value < 0 || 9 < value)
    {
        throw std::runtime_error("int must be between 0 and 9 to generate a glyph");
    }
    return static_cast<char>('0' + value);
}

inline char glyph(const char& value)
{
    return value;
}

template <typename T>
core::CellView view(const T& value, bool is_pushable)
{
    return {
        .symbol = glyph(value),
        .properties = core::Object {
            .is_pushable = is_pushable
        }
    };
}

inline core::CellView view(const Empty& value, bool)
{
    return {.symbol = glyph(value), .properties = core::Empty {}};
}

inline core::CellView view(const Player& value, bool)
{
    return {.symbol = glyph(value), .properties = core::Player {}};
}

class Object
{
public:
    template <typename T>
    Object(T value, bool is_pushable = false)
        : self_(std::make_shared<Model<T>>(std::move(value), is_pushable))
    {}

    core::CellView view() const
    {
        return self_->view_();
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual core::CellView view_() const = 0;
    };

    template <typename T>
    struct Model : Concept
    {
        explicit Model(T value, bool is_pushable) : data_(std::move(value)), is_pushable_(is_pushable)
        {}

        core::CellView view_() const override
        {
            using core::internal::view;
            return view(data_, is_pushable_);
        }

        T data_;
        bool is_pushable_ {false};
    };

    std::shared_ptr<const Concept> self_;
};

template <typename T>
class MakeObject
{
public:
    explicit MakeObject(T value) : value_(std::move(value))
    {}

    MakeObject pushable() const
    {
        MakeObject out = *this;
        out.is_pushable_ = true;
        return out;
    }

    Object build() &&
    {
        return Object(std::move(value_), is_pushable_);
    }

    operator Object() &&
    {
        return std::move(*this).build();
    }

private:
    T value_;
    bool is_pushable_ {false};
};

struct Map
{
    int commits_left {6};
    int undos_left {6};
    std::vector<std::vector<Object>> grid;
};

using History = std::vector<Map>;

inline int grid_height(const Map& map)
{
    return static_cast<int>(map.grid.size());
}

inline int grid_width(const Map& map)
{
    if (map.grid.empty()) return 0;
    return static_cast<int>(map.grid[0].size());
}

inline bool in_bounds(const Map& map, const Point& point)
{
    return point.x >= 0 && point.y >= 0 && point.x < grid_width(map) && point.y < grid_height(map);
}

inline Point find_player(const Map& map)
{
    for (int y = 0; y < grid_height(map); ++y)
    {
        for (int x = 0; x < grid_width(map); ++x)
        {
            if (std::holds_alternative<core::Player>(map.grid[y][x].view().properties)) return {x, y};
        }
    }
    throw std::runtime_error("Player not found");
}

inline void commit(History& history)
{
    assert(!history.empty());
    history.push_back(history.back());
}

inline void undo(History& history)
{
    assert(!history.empty());
    history.pop_back();
}

inline Map& current(History& history)
{
    assert(!history.empty());
    return history.back();
}

inline const Map& current(const History& history)
{
    assert(!history.empty());
    return history.back();
}

inline Map make_map()
{
    constexpr size_t width = 9;
    constexpr size_t height = 7;

    Map map;
    map.grid.assign(height, std::vector<Object>(width, Empty {}));

    map.grid[6][0] = MakeObject(Player {});
    map.grid[1][2] = MakeObject('+').pushable();
    map.grid[1][4] = MakeObject('=').pushable();

    map.grid[4][4] = MakeObject(1).pushable();
    map.grid[5][7] = MakeObject(2).pushable();
    map.grid[3][6] = MakeObject(3).pushable();
    map.grid[5][2] = MakeObject(4).pushable();

    return map;
}
}  // namespace core::internal
