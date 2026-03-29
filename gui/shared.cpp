#include "shared.hpp"

#include <cfloat>
#include <fstream>
#include <iterator>
#include <type_traits>

namespace type_erasure::gui
{
namespace
{
ImVec4 color_from_rgb(const int r, const int g, const int b)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}
}  // namespace

bool load_text_file(const std::string& path, std::string& content)
{
    std::ifstream input(path);
    if (!input) return false;
    content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

bool save_text_file(const std::string& path, const std::string_view content)
{
    std::ofstream output(path);
    if (!output) return false;
    output << content;
    return static_cast<bool>(output);
}

void apply_style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.TabRounding = 10.0f;
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_ChildBg] = color_from_rgb(24, 29, 35);
    colors[ImGuiCol_PopupBg] = color_from_rgb(24, 29, 35);
    colors[ImGuiCol_FrameBg] = color_from_rgb(36, 43, 51);
    colors[ImGuiCol_FrameBgHovered] = color_from_rgb(52, 61, 73);
    colors[ImGuiCol_FrameBgActive] = color_from_rgb(67, 79, 94);
    colors[ImGuiCol_TitleBg] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_TitleBgActive] = color_from_rgb(16, 20, 24);
    colors[ImGuiCol_Button] = color_from_rgb(180, 91, 63);
    colors[ImGuiCol_ButtonHovered] = color_from_rgb(205, 113, 81);
    colors[ImGuiCol_ButtonActive] = color_from_rgb(151, 72, 48);
    colors[ImGuiCol_Header] = color_from_rgb(45, 54, 64);
    colors[ImGuiCol_HeaderHovered] = color_from_rgb(63, 73, 85);
    colors[ImGuiCol_HeaderActive] = color_from_rgb(78, 90, 105);
    colors[ImGuiCol_Text] = color_from_rgb(235, 233, 225);
    colors[ImGuiCol_TextDisabled] = color_from_rgb(143, 148, 154);
    colors[ImGuiCol_Border] = color_from_rgb(62, 68, 76);
}

ImU32 tile_fill(const core::CellView& cell)
{
    return std::visit(
        [](const auto& value) -> ImU32
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, core::Empty>)
            {
                return IM_COL32(31, 37, 43, 255);
            }
            else if constexpr (std::is_same_v<T, core::Player>)
            {
                return IM_COL32(236, 177, 79, 255);
            }
            else
            {
                if (value.symbol == '=' || value.symbol == '+') return IM_COL32(108, 167, 124, 255);
                if (value.is_pickable) return IM_COL32(97, 147, 196, 255);
                if (value.is_pushable) return IM_COL32(189, 112, 143, 255);
                return IM_COL32(116, 123, 132, 255);
            }
        },
        cell);
}

ImU32 tile_outline(const core::CellView& cell)
{
    return std::visit(
        [](const auto& value) -> ImU32
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, core::Player>)
            {
                return IM_COL32(255, 226, 157, 255);
            }
            else if constexpr (std::is_same_v<T, core::Object>)
            {
                if (value.symbol == '=' || value.symbol == '+') return IM_COL32(170, 223, 170, 255);
            }
            return IM_COL32(72, 79, 88, 255);
        },
        cell);
}

void draw_tile_symbol(ImDrawList& draw_list, const ImVec2& cell_min, const ImVec2& cell_max, const char symbol,
                      const float font_size, const ImU32 color)
{
    const char glyph[2] = {symbol, '\0'};
    ImFont* font = ImGui::GetFont();
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, glyph);
    const ImVec2 text_pos(
        cell_min.x + ((cell_max.x - cell_min.x) - text_size.x) * 0.5f,
        cell_min.y + ((cell_max.y - cell_min.y) - text_size.y) * 0.5f);
    draw_list.AddText(font, font_size, text_pos, color, glyph);
}
}  // namespace type_erasure::gui
