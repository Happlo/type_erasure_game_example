#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>
#include <utility>
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
bool is_empty(const T&)
{
    return false;
}

inline bool is_empty(const Empty&)
{
    return true;
}

template <typename T>
bool is_player(const T&)
{
    return false;
}

inline bool is_player(const Player&)
{
    return true;
}

template <typename T>
int number_value(const T&)
{
    return -1;
}

inline int number_value(const int& value)
{
    return value;
}

struct ObjectProperties
{
    char glyph {'?'};
    bool is_empty {false};
    bool is_player {false};
    bool is_pushable {false};
    bool blocks_movement {false};
    int number_value {-1};
};

template <typename T>
ObjectProperties create_properties(const T& value, bool is_pushable, bool blocks_movement)
{
    return {glyph(value), is_empty(value), is_player(value), is_pushable, blocks_movement, number_value(value)};
}

class Object
{
public:
    template <typename T>
    Object(T value, bool is_pushable = false, bool blocks_movement = false)
        : self_(std::make_shared<Model<T>>(std::move(value), is_pushable, blocks_movement))
    {}

    ObjectProperties properties() const
    {
        return self_->properties_();
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual ObjectProperties properties_() const = 0;
    };

    template <typename T>
    struct Model : Concept
    {
        explicit Model(T value, bool is_pushable, bool blocks_movement) : data_(std::move(value))
        {
            properties = create_properties(data_, is_pushable, blocks_movement);
        }

        ObjectProperties properties_() const override
        {
            return create_properties(data_, properties.is_pushable, properties.blocks_movement);
        }

        T data_;
        ObjectProperties properties {};
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

    MakeObject blocking() const
    {
        MakeObject out = *this;
        out.blocks_movement_ = true;
        return out;
    }

    Object build() &&
    {
        return Object(std::move(value_), is_pushable_, blocks_movement_);
    }

    operator Object() &&
    {
        return std::move(*this).build();
    }

private:
    T value_;
    bool is_pushable_ {false};
    bool blocks_movement_ {false};
};

struct World
{
    int commits_left {6};
    int undos_left {6};
    std::vector<std::vector<Object>> grid;
};

using History = std::vector<World>;

inline int grid_height(const World& world)
{
    return static_cast<int>(world.grid.size());
}

inline int grid_width(const World& world)
{
    if (world.grid.empty()) return 0;
    return static_cast<int>(world.grid[0].size());
}

inline bool in_bounds(const World& world, const Point& point)
{
    return point.x >= 0 && point.y >= 0 && point.x < grid_width(world) && point.y < grid_height(world);
}

inline Point find_player(const World& world)
{
    for (int y = 0; y < grid_height(world); ++y)
    {
        for (int x = 0; x < grid_width(world); ++x)
        {
            if (world.grid[y][x].properties().is_player) return {x, y};
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

inline World& current(History& history)
{
    assert(!history.empty());
    return history.back();
}

inline const World& current(const History& history)
{
    assert(!history.empty());
    return history.back();
}

inline World make_world()
{
    constexpr size_t width = 9;
    constexpr size_t height = 7;

    World world;
    world.grid.assign(height, std::vector<Object>(width, Empty {}));

    world.grid[6][0] = MakeObject(Player {});
    world.grid[1][2] = MakeObject('+').pushable();
    world.grid[1][4] = MakeObject('=').pushable();

    world.grid[4][4] = MakeObject(1).pushable();
    world.grid[5][7] = MakeObject(2).pushable();
    world.grid[3][6] = MakeObject(3).pushable();
    world.grid[5][2] = MakeObject(4).pushable();

    return world;
}
}  // namespace core::internal
