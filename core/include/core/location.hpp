
#pragma once

namespace core
{
struct Location
{
    int x{0};
    int y{0};

    auto operator<=>(const Location &) const = default;
};
} // namespace core
