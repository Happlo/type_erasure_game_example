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
    core::Player player {};
    Facing facing {Facing::South};

    static PlayerState from_delta(int dx, int dy);
};

Facing facing_from_delta(int dx, int dy);

char glyph(Facing facing);

char glyph(const PlayerState &player);

char glyph(const core::Empty &);

char glyph(const int &value);

char glyph(const char &value);

template <typename T> core::CellView view(const T &value, bool is_pushable)
{
    return {.symbol = glyph(value), .properties = core::Object{.is_pushable = is_pushable}};
}

inline core::CellView view(const core::Empty &value, bool)
{
    return {.symbol = glyph(value), .properties = core::Empty{}};
}

inline core::CellView view(const PlayerState &value, bool)
{
    return {.symbol = glyph(value), .properties = value.player};
}

class Object
{
  public:
    template <typename T>
    Object(T value, bool is_pushable = false)
        : self_(std::make_shared<Model<T>>(std::move(value), is_pushable))
    {
    }

    core::CellView view() const;

  private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual core::CellView view_() const = 0;
    };

    template <typename T> struct Model : Concept
    {
        explicit Model(T value, bool is_pushable)
            : data_(std::move(value)), is_pushable_(is_pushable)
        {
        }

        core::CellView view_() const override
        {
            using core::internal::view;
            return view(data_, is_pushable_);
        }

        T data_;
        bool is_pushable_{false};
    };

    std::shared_ptr<const Concept> self_;
};

template <typename T> class MakeObject
{
  public:
    explicit MakeObject(T value) : value_(std::move(value)) {}

    MakeObject pushable()
    {
        is_pushable_ = true;
        return *this;
    }

    Object build() && { return Object(std::move(value_), is_pushable_); }

    operator Object() && { return std::move(*this).build(); }

  private:
    T value_;
    bool is_pushable_{false};
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

Point find_player(const Map &map);

void commit(History &history);

void undo(History &history);

Map &current(History &history);

const Map &current(const History &history);

Map make_map();

core::MapView build_view(const Map &map);
} // namespace core::internal
