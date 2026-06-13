#pragma once

#include "core/map.hpp"
#include "game.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace core
{
struct Brush
{
    char symbol{'*'};
    Object::ManipulationLevel manipulation_level{Object::ManipulationLevel::None};
};

class MapBuilder
{
  public:
    virtual ~MapBuilder() = default;

    virtual const MapView &view() const = 0;

    virtual MapBuilder &set_commits_left(int commits_left) = 0;
    virtual MapBuilder &set_undos_left(int undos_left) = 0;

    virtual MapBuilder &apply_brush(int x, int y, const Brush &brush) = 0;
    virtual MapBuilder &clear_cell(int x, int y) = 0;

    virtual std::unique_ptr<Game> create_game() const = 0;
    virtual void save_to_file(const std::filesystem::path &path) const = 0;

    static std::unique_ptr<MapBuilder> create_default(
        const std::filesystem::path &save_directory = {});
    static std::unique_ptr<MapBuilder> create(
        const std::filesystem::path &save_directory = {});
    static std::unique_ptr<MapBuilder> load_from_file(const std::filesystem::path &path);
    static const std::vector<char> &solver_operators();
    static const std::vector<char> &equation_delimiters();
};
} // namespace core
