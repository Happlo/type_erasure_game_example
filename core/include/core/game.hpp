#pragma once

#include <memory>
#include <string>

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
    Game();
    Game(const Game& other);
    Game& operator=(const Game& other);
    Game(Game&& other) noexcept;
    Game& operator=(Game&& other) noexcept;
    ~Game();

    bool apply_event(Event event);
    bool solved() const;
    std::string render() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace core
