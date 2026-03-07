#pragma once

#include "game_model.hpp"

#include <string>
#include <string_view>

namespace core::world_io
{
internal::World world_from_json(std::string_view json_text);
std::string world_to_json(const internal::World& world);
}  // namespace core::world_io
