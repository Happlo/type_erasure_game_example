#pragma once

#include "equation_result.hpp"
#include "map.hpp"

#include <filesystem>
#include <memory>

namespace core
{
enum class Event
{
    MoveUp,
    MoveLeft,
    MoveDown,
    MoveRight,
    PickItem,
    DropItem,
    Undo
};

class Game
{
  public:
    virtual ~Game() = default;

    virtual GameResult apply_event(Event event) = 0;
    virtual MapView view() const = 0;

    static std::unique_ptr<Game> load_from_file(const std::filesystem::path &path);
};
} // namespace core
