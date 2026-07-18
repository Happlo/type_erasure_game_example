#pragma once

#include "core/location.hpp"
#include "core/map.hpp"

#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace core
{
Object view(const Object &value);
} // namespace core

namespace core::internal
{

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

template <typename T> core::Object adl_view(const T &value) { return view(value); }

class TypeErasedObject
{
  public:
    template <typename T>
    TypeErasedObject(T value) : self_(std::make_shared<Model<T>>(std::move(value)))
    {
    }

    core::Object view() const { return self_->view_(); }

  private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual core::Object view_() const = 0;
    };

    template <typename T> struct Model final : Concept
    {
        explicit Model(T value) : data_(std::move(value)) {}

        core::Object view_() const override { return adl_view(data_); }

        T data_;
    };

    std::shared_ptr<const Concept> self_;
};

struct Map
{
    int commits_left{0};
    int undos_left{0};
    std::optional<PlayerState> player;
    std::map<Location, TypeErasedObject> objects;
};

using History = std::vector<Map>;

void commit(History &history, Map state);

void undo(History &history);

Map &current(History &history);

const Map &current(const History &history);

core::MapView build_view(const Map &map);
} // namespace core::internal
