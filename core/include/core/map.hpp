#pragma once

#include <cstddef>
#include <optional>
#include <vector>
#include <variant>

namespace core
{

struct Empty
{
    char symbol{' '};
};

struct Object
{
    char symbol;
    bool is_pushable {false};
    bool is_pickable {false};
};

struct Player
{
    char symbol;
    std::vector<Object> inventory;
};

using CellView = std::variant<Empty, Player, Object>;

inline char symbol_of(const CellView& cell)
{
    return std::visit([](const auto& value) { return value.symbol; }, cell);
}

struct MapView
{
    int width {0};
    int height {0};
    int commits_left {0};
    int undos_left {0};
    std::optional<Player> player;
    std::vector<CellView> cells;

    const CellView& at(const int x, const int y) const
    {
        return cells[static_cast<size_t>(y * width + x)];
    }
};
}  // namespace core
