#pragma once

#include "game_model.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace core::map_io
{
internal::Map map_from_json(std::string_view json_text, bool require_player = true);
std::string map_to_json(const internal::Map& map);
internal::Map map_from_file(const std::filesystem::path& path, bool require_player = true);
void map_to_file(const internal::Map& map, const std::filesystem::path& path);
}  // namespace core::map_io
