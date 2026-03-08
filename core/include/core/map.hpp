#pragma once

#include <cstddef>
#include <vector>
#include <variant>

namespace core
{

struct Empty{};

struct Player{};

struct Object{};

struct CellView
{
    char symbol;
    std::variant<Empty, Player, Object> properties;
};

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
