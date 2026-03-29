#pragma once

#include "core/map.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace core::internal
{
struct Point
{
    int x{};
    int y{};
};

enum class Facing
{
    North,
    South,
    West,
    East
};

struct PlayerState
{
    core::Player player{};
    Facing facing{Facing::South};

    static PlayerState from_delta(int dx, int dy, core::Player player = {});
};

Facing facing_from_delta(int dx, int dy);

char glyph(Facing facing);

char glyph(const PlayerState &player);

char glyph(const core::Empty &);

char glyph(const int &value);

char glyph(const char &value);

char glyph(const core::Object &value);

template <typename T>
core::CellView view(const T &value, core::Object::ManipulationLevel manipulation_level)
{
    return core::Object{
        .symbol = glyph(value),
        .manipulation_level = manipulation_level,
    };
}

inline core::CellView view(const core::Empty &value, core::Object::ManipulationLevel)
{
    return core::Empty{.symbol = glyph(value)};
}

inline core::CellView view(const PlayerState &value, core::Object::ManipulationLevel)
{
    auto player = value.player;
    player.symbol = glyph(value);
    return player;
}

inline core::CellView view(const core::Object &value, core::Object::ManipulationLevel)
{
    return value;
}

class Object
{
  public:
    template <typename T>
    Object(T value, core::Object::ManipulationLevel manipulation_level =
                        core::Object::ManipulationLevel::None)
        : self_(std::make_shared<Model<T>>(std::move(value), manipulation_level))
    {
    }

    core::CellView view() const;
    bool is_empty() const;
    bool is_player() const;
    PlayerState player_state() const;

  private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual core::CellView view_() const = 0;
        virtual bool is_empty_() const = 0;
        virtual bool is_player_() const = 0;
        virtual PlayerState player_state_() const = 0;
    };

    template <typename T> struct Model : Concept
    {
        explicit Model(T value, core::Object::ManipulationLevel manipulation_level)
            : data_(std::move(value)), manipulation_level_(manipulation_level)
        {
        }

        core::CellView view_() const override
        {
            using core::internal::view;
            return view(data_, manipulation_level_);
        }

        bool is_empty_() const override { return std::is_same_v<T, core::Empty>; }

        bool is_player_() const override { return std::is_same_v<T, PlayerState>; }

        PlayerState player_state_() const override
        {
            if constexpr (std::is_same_v<T, PlayerState>)
            {
                return data_;
            }
            throw std::runtime_error("Object does not contain a player");
        }

        T data_;
        core::Object::ManipulationLevel manipulation_level_{core::Object::ManipulationLevel::None};
    };

    std::shared_ptr<const Concept> self_;
};

template <typename T> class MakeObject
{
  public:
    explicit MakeObject(T value) : value_(std::move(value)) {}

    MakeObject with_manipulation_level(core::Object::ManipulationLevel manipulation_level)
    {
        manipulation_level_ = manipulation_level;
        return *this;
    }

    Object build() && { return Object(std::move(value_), manipulation_level_); }

    operator Object() && { return std::move(*this).build(); }

  private:
    T value_;
    core::Object::ManipulationLevel manipulation_level_{core::Object::ManipulationLevel::None};
};

struct Map
{
    int commits_left{6};
    int undos_left{6};
    std::vector<std::vector<Object>> grid;
};

using History = std::vector<Map>;

int grid_height(const Map &map);

int grid_width(const Map &map);

bool in_bounds(const Map &map, const Point &point);

std::optional<Point> find_player(const Map &map);
std::optional<core::Player> get_public_player(const Map &map);

void commit(History &history);

void undo(History &history);

Map &current(History &history);

const Map &current(const History &history);

core::MapView build_view(const Map &map);
} // namespace core::internal
