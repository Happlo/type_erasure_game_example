#pragma once

#include "core/map.hpp"

#include <imgui.h>

#include <string>
#include <string_view>

namespace type_erasure::gui
{
bool load_text_file(const std::string& path, std::string& content);
bool save_text_file(const std::string& path, std::string_view content);

void apply_style();

ImU32 tile_fill(const core::CellView& cell);
ImU32 tile_outline(const core::CellView& cell);
void draw_tile_symbol(ImDrawList& draw_list, const ImVec2& cell_min, const ImVec2& cell_max, char symbol,
                      float font_size, ImU32 color = IM_COL32(245, 242, 233, 255));
}  // namespace type_erasure::gui
