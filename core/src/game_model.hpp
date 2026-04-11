#pragma once

#include "core/map.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
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
core::CellView view(const T &value)
{
    return core::Object{.symbol = glyph(value)};
}

inline core::CellView view(const core::Empty &value)
{
    return core::Empty{.symbol = glyph(value)};
}

inline core::CellView view(const PlayerState &value)
{
    auto player = value.player;
    player.symbol = glyph(value);
    return player;
}

inline core::CellView view(const core::Object &value)
{
    return value;
}

class TypeErasedObject
{
  public:
    template <typename T> TypeErasedObject(T value) : self_(std::make_shared<Model<T>>(std::move(value)))
    {
    }

    core::CellView view() const;

    template <typename T> bool is() const { return self_->type_() == std::type_index(typeid(T)); }

    template <typename T> const T &get() const
    {
        if (!is<T>())
            throw std::runtime_error("Object does not contain requested type");
        const T *ptr = static_cast<const T *>(self_->ptr_(std::type_index(typeid(T))));
        return *ptr;
    }

  private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual core::CellView view_() const = 0;
        virtual std::type_index type_() const = 0;
        virtual const void *ptr_(std::type_index type) const = 0;
    };

    template <typename T> struct Model : Concept
    {
        explicit Model(T value) : data_(std::move(value))
        {
        }

        core::CellView view_() const override
        {
            using core::internal::view;
            return view(data_);
        }

        std::type_index type_() const override { return std::type_index(typeid(T)); }

        const void *ptr_(const std::type_index type) const override
        {
            return type == std::type_index(typeid(T)) ? std::addressof(data_) : nullptr;
        }

        T data_;
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

    TypeErasedObject build() &&
    {
        if constexpr (std::is_same_v<T, core::Empty> || std::is_same_v<T, PlayerState> || std::is_same_v<T, core::Object>)
        {
            return TypeErasedObject(std::move(value_));
        }
        else
        {
            return TypeErasedObject(core::Object{
                .symbol = glyph(value_),
                .manipulation_level = manipulation_level_,
            });
        }
    }

    operator TypeErasedObject() && { return std::move(*this).build(); }

  private:
    T value_;
    core::Object::ManipulationLevel manipulation_level_{core::Object::ManipulationLevel::None};
};

struct Map
{
    int commits_left{6};
    int undos_left{6};
    std::vector<std::vector<TypeErasedObject>> grid;
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
