#include "core/game.hpp"

#include <cassert>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace core
{
namespace
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

char glyph(const Player& player)
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

char glyph(const Empty&)
{
    return ' ';
}

char glyph(const int& value)
{
    if (value < 0 || 9 < value)
    {
        throw std::runtime_error("int must be between 0 and 9 to generate a glyph");
    }
    return static_cast<char>('0' + value);
}

char glyph(const char& value)
{
    return value;
}

template <typename T>
bool is_empty(const T&)
{
    return false;
}

bool is_empty(const Empty&)
{
    return true;
}

template <typename T>
bool is_player(const T&)
{
    return false;
}

bool is_player(const Player&)
{
    return true;
}

template <typename T>
int number_value(const T&)
{
    return -1;
}

int number_value(const int& value)
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
    return {glyph(value), ::core::is_empty(value), ::core::is_player(value), is_pushable, blocks_movement, ::core::number_value(value)};
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

void commit(History& history)
{
    assert(!history.empty());
    history.push_back(history.back());
}

void undo(History& history)
{
    assert(!history.empty());
    history.pop_back();
}

World& current(History& history)
{
    assert(!history.empty());
    return history.back();
}

const World& current(const History& history)
{
    assert(!history.empty());
    return history.back();
}

int grid_height(const World& world)
{
    return static_cast<int>(world.grid.size());
}

int grid_width(const World& world)
{
    if (world.grid.empty()) return 0;
    return static_cast<int>(world.grid[0].size());
}

bool in_bounds(const World& world, const Point& point)
{
    return point.x >= 0 && point.y >= 0 && point.x < grid_width(world) && point.y < grid_height(world);
}

Object& cell(World& world, const Point& point)
{
    return world.grid[point.y][point.x];
}

const Object& cell(const World& world, const Point& point)
{
    return world.grid[point.y][point.x];
}

Point find_player(const World& world)
{
    for (int y = 0; y < grid_height(world); ++y)
    {
        for (int x = 0; x < grid_width(world); ++x)
        {
            Point point {x, y};
            if (cell(world, point).properties().is_player) return point;
        }
    }
    throw std::runtime_error("Player not found");
}

bool equation_is_correct(const std::string& equation)
{
    std::string compact;
    for (char ch : equation)
    {
        if (ch != ' ') compact.push_back(ch);
    }

    if (compact.size() != 5) return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[0]))) return false;
    if (compact[1] != '+') return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[2]))) return false;
    if (compact[3] != '=') return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[4]))) return false;

    const int lhs = compact[0] - '0';
    const int rhs = compact[2] - '0';
    const int result = compact[4] - '0';
    return lhs + rhs == result;
}

bool solved_equation(const World& world)
{
    for (int y = 0; y < grid_height(world); ++y)
    {
        std::string row;
        row.reserve(static_cast<size_t>(grid_width(world)));
        for (int x = 0; x < grid_width(world); ++x)
        {
            row.push_back(cell(world, {x, y}).properties().glyph);
        }

        if (row.find('=') != std::string::npos) return equation_is_correct(row);
    }

    return false;
}

bool try_move_player(World& world, int dx, int dy)
{
    const Point player = find_player(world);
    const Player moved_player = Player::from_delta(dx, dy);

    const Point next {player.x + dx, player.y + dy};
    if (!in_bounds(world, next)) return false;

    const Object next_object = cell(world, next);
    const auto next_props = next_object.properties();
    if (next_props.blocks_movement) return false;

    if (next_props.is_empty)
    {
        cell(world, player) = Object(Empty {});
        cell(world, next) = Object(moved_player);
        return true;
    }

    if (!next_props.is_pushable) return false;

    const Point pushed {next.x + dx, next.y + dy};
    if (!in_bounds(world, pushed)) return false;

    const Object pushed_object = cell(world, pushed);
    if (!pushed_object.properties().is_empty) return false;

    cell(world, pushed) = next_object;
    cell(world, next) = Object(moved_player);
    cell(world, player) = Object(Empty {});
    return true;
}

bool world_commit(History& history)
{
    auto& world = current(history);
    if (world.commits_left <= 0) return false;
    const int commits_after = world.commits_left - 1;
    commit(history);
    current(history).commits_left = commits_after;
    return true;
}

bool world_undo(History& history)
{
    auto& world = current(history);
    if (world.undos_left <= 0) return false;
    if (history.size() <= 1) return false;
    const int undos_after = world.undos_left - 1;
    undo(history);
    current(history).undos_left = undos_after;
    return true;
}

World make_world()
{
    constexpr size_t width = 9;
    constexpr size_t height = 7;

    World world;
    world.grid.assign(height, std::vector<Object>(width, Empty {}));

    cell(world, {0, 6}) = MakeObject(Player {});
    cell(world, {2, 1}) = MakeObject('+').pushable();
    cell(world, {4, 1}) = MakeObject('=').pushable();

    cell(world, {4, 4}) = MakeObject(1).pushable();
    cell(world, {7, 5}) = MakeObject(2).pushable();
    cell(world, {6, 3}) = MakeObject(3).pushable();
    cell(world, {2, 5}) = MakeObject(4).pushable();

    return world;
}

std::string render_world(const World& world)
{
    std::string out;
    out += "\nWorld commits/undos: " + std::to_string(world.commits_left) + "/" + std::to_string(world.undos_left) + "\n\n";

    for (int y = 0; y < grid_height(world); ++y)
    {
        for (int x = 0; x < grid_width(world); ++x)
        {
            const Point point {x, y};
            out.push_back(cell(world, point).properties().glyph);
            out.push_back(' ');
        }
        out.push_back('\n');
    }

    out += "\nCommands: w/a/s/d, commit, undo, help, quit\n";
    out += "Goal: make the row containing '=' form a true equation.\n";
    out += "The '+' and '=' tiles stay fixed in place.\n";
    return out;
}
}  // namespace

struct Game::Impl
{
    History history {make_world()};
};

Game::Game() : impl_(std::make_unique<Impl>())
{}

Game::Game(const Game& other) : impl_(std::make_unique<Impl>(*other.impl_))
{}

Game& Game::operator=(const Game& other)
{
    if (this == &other) return *this;
    *impl_ = *other.impl_;
    return *this;
}

Game::Game(Game&& other) noexcept = default;

Game& Game::operator=(Game&& other) noexcept = default;

Game::~Game() = default;

bool Game::apply_event(const Event event)
{
    auto& world = current(impl_->history);
    switch (event)
    {
        case Event::MoveUp: return try_move_player(world, 0, -1);
        case Event::MoveLeft: return try_move_player(world, -1, 0);
        case Event::MoveDown: return try_move_player(world, 0, 1);
        case Event::MoveRight: return try_move_player(world, 1, 0);
        case Event::Commit: return world_commit(impl_->history);
        case Event::Undo: return world_undo(impl_->history);
    }
    return false;
}

bool Game::solved() const
{
    return solved_equation(current(impl_->history));
}

std::string Game::render() const
{
    return render_world(current(impl_->history));
}
}  // namespace core
