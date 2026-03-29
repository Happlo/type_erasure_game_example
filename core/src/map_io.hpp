#pragma once

#include "game_model.hpp"

#include <string>
#include <string_view>

namespace core::map_io
{
internal::Map map_from_json(std::string_view json_text, bool require_player = true);
std::string map_to_json(const internal::Map& map);
}  // namespace core::map_io
