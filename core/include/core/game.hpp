#pragma once

#include "core/map.hpp"

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
    Commit,
    Undo
};

class Game
{
public:
    virtual ~Game() = default;

    virtual bool apply_event(Event event) = 0;
    virtual bool solved() const = 0;
    virtual MapView view() const = 0;
    virtual std::string to_json() const = 0;

    static std::unique_ptr<Game> create_default();
    static std::unique_ptr<Game> from_json(std::string_view json_text);
};
}  // namespace core
