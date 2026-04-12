#include "object.hpp"

#include <stdexcept>

namespace core
{
char glyph(const Empty &) { return ' '; }

char glyph(const Object &value) { return value.symbol; }

CellView view(const Empty &value) { return Empty{.symbol = glyph(value)}; }

CellView view(const Object &value) { return value; }
} // namespace core

namespace core::internal
{
char glyph(const int &value)
{
    if (value < 0 || 9 < value)
    {
        throw std::runtime_error("int must be between 0 and 9 to generate a glyph");
    }
    return static_cast<char>('0' + value);
}

char glyph(const char &value) { return value; }
} // namespace core::internal
