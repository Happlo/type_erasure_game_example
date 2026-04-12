#pragma once

#include "core/map.hpp"
#include "game_model.hpp"

#include <utility>

namespace core
{
char glyph(const Empty &value);
char glyph(const Object &value);

CellView view(const Empty &value);
CellView view(const Object &value);
} // namespace core

namespace core::internal
{
char glyph(const int &value);
char glyph(const char &value);

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
        return TypeErasedObject(core::Object{
            .symbol = glyph(value_),
            .manipulation_level = manipulation_level_,
        });
    }

    operator TypeErasedObject() && { return std::move(*this).build(); }

  private:
    T value_;
    core::Object::ManipulationLevel manipulation_level_{core::Object::ManipulationLevel::None};
};
} // namespace core::internal
