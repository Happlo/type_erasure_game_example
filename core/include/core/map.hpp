#pragma once

#include <cstddef>
#include <vector>
#include <variant>

namespace core
{

using Symbol = char;
struct Empty {};

using CellView = std::variant<Empty, Symbol>;

struct MapView
{
    int width {0};
    int height {0};
    int commits_left {0};
    int undos_left {0};
    std::vector<CellView> cells;

    const CellView& at(const int x, const int y) const
    {
        return cells[static_cast<size_t>(y * width + x)];
    }
};
}  // namespace core
