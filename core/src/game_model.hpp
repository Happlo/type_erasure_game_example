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

namespace core
{
CellView view(const Empty &value);
CellView view(const Object &value);
} // namespace core

namespace core::internal
{
struct Point
{
    int x{};
    int y{};
};

struct PlayerState;
core::CellView view(const PlayerState &value);

template <typename T> core::CellView adl_view(const T &value) { return view(value); }

class TypeErasedObject
{
  public:
    template <typename T>
    TypeErasedObject(T value) : self_(std::make_shared<Model<T>>(std::move(value)))
    {
    }

    core::CellView view() const { return self_->view_(); }

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
        explicit Model(T value) : data_(std::move(value)) {}

        core::CellView view_() const override { return adl_view(data_); }

        std::type_index type_() const override { return std::type_index(typeid(T)); }

        const void *ptr_(const std::type_index type) const override
        {
            return type == std::type_index(typeid(T)) ? std::addressof(data_) : nullptr;
        }

        T data_;
    };

    std::shared_ptr<const Concept> self_;
};

struct Map
{
    int commits_left{0};
    int undos_left{0};
    std::vector<std::vector<TypeErasedObject>> grid;
};

using History = std::vector<Map>;

int grid_height(const Map &map);

int grid_width(const Map &map);

bool in_bounds(const Map &map, const Point &point);

void commit(History &history);

void undo(History &history);

Map &current(History &history);

const Map &current(const History &history);

core::MapView build_view(const Map &map);
} // namespace core::internal
