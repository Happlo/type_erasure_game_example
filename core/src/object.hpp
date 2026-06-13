#pragma once

#include "core/map.hpp"

#include <utility>

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

    core::Object build() &&
    {
        return core::Object{
            .symbol = glyph(value_),
            .manipulation_level = manipulation_level_,
        };
    }

    operator core::Object() && { return std::move(*this).build(); }

  private:
    T value_;
    core::Object::ManipulationLevel manipulation_level_{core::Object::ManipulationLevel::None};
};
} // namespace core::internal
