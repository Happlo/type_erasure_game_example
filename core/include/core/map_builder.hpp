#pragma once

#include "core/map.hpp"

#include <memory>
#include <optional>
#include <string>

namespace core
{
struct Brush
{
    char symbol{'*'};
    bool pushable{false};
    bool pickable{false};
};

class MapBuilder
{
  public:
    virtual ~MapBuilder() = default;

    virtual const MapView &view() const = 0;
    virtual const CellView &at(int x, int y) const = 0;

    virtual void set_commits_left(int commits_left) = 0;
    virtual void set_undos_left(int undos_left) = 0;

    virtual void resize(int new_width, int new_height) = 0;
    virtual void apply_brush(int x, int y, const Brush &brush) = 0;
    virtual void clear_cell(int x, int y) = 0;

    virtual std::string to_json() const = 0;

    static std::unique_ptr<MapBuilder> create_default();
    static std::unique_ptr<MapBuilder> create(int width, int height);
    static std::optional<std::unique_ptr<MapBuilder>> from_json(const std::string &text,
                                                                std::string &error_message);
};
} // namespace core
