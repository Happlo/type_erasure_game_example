#pragma once

#include "equation_result.hpp"
#include "map.hpp"

#include <memory>
#include <string>
#include <string_view>

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
    Commit,
    Undo
};

class Game
{
  public:
    virtual ~Game() = default;

    virtual EquationResult apply_event(Event event) = 0;
    virtual MapView view() const = 0;
    virtual std::string to_json() const = 0;

    static std::unique_ptr<Game> from_json(std::string_view json_text);
};
} // namespace core
