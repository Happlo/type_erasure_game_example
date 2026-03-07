#include "core/game.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace
{
struct RawModeGuard
{
    bool active {false};
    termios old_mode {};

    bool enable()
    {
        if (!isatty(STDIN_FILENO)) return false;
        if (tcgetattr(STDIN_FILENO, &old_mode) != 0) return false;

        termios raw = old_mode;
        raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
        active = true;
        return true;
    }

    ~RawModeGuard()
    {
        if (active) tcsetattr(STDIN_FILENO, TCSANOW, &old_mode);
    }
};

char read_key()
{
    char ch = '\0';
    if (read(STDIN_FILENO, &ch, 1) != 1) return '\0';
    return ch;
}

std::string read_command_raw(char first)
{
    std::string line;
    if (first == '\r' || first == '\n')
    {
        std::cout << '\n';
        return line;
    }

    line.push_back(first);
    std::cout << first << std::flush;

    while (true)
    {
        const char ch = read_key();
        if (ch == '\0') return line;

        if (ch == '\r' || ch == '\n')
        {
            std::cout << '\n';
            return line;
        }

        if (ch == 127 || ch == '\b')
        {
            if (!line.empty())
            {
                line.pop_back();
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }

        if (std::isprint(static_cast<unsigned char>(ch)))
        {
            line.push_back(ch);
            std::cout << ch << std::flush;
        }
    }
}

void show_help()
{
    std::cout << "w/a/s/d   Move player\n";
    std::cout << "          Walk into a number to push it one cell\n";
    std::cout << "commit    Save whole-world snapshot (costs 1 world commit)\n";
    std::cout << "undo      Restore previous world snapshot (costs 1 world undo)\n";
    std::cout << "quit      Exit\n";
}
}  // namespace

int main()
{
    core::Game game;

    std::cout << "Time Grid\n";
    show_help();

    RawModeGuard raw_mode;
    if (!raw_mode.enable())
    {
        std::cerr << "Failed to enable raw terminal input.\n";
        return 1;
    }
    std::cout << "Immediate input enabled: movement keys w/a/s/d do not require Enter.\n";

    const auto handle_line = [&](const std::string& line) -> bool {
        if (line == "quit") return false;
        if (line == "help")
        {
            show_help();
            return true;
        }

        if (line == "w") { game.apply_event(core::Event::MoveUp); return true; }
        if (line == "a") { game.apply_event(core::Event::MoveLeft); return true; }
        if (line == "s") { game.apply_event(core::Event::MoveDown); return true; }
        if (line == "d") { game.apply_event(core::Event::MoveRight); return true; }

        if (line == "commit")
        {
            if (!game.apply_event(core::Event::Commit)) std::cout << "Cannot commit world.\n";
            return true;
        }

        if (line == "undo")
        {
            if (!game.apply_event(core::Event::Undo)) std::cout << "Cannot undo world.\n";
            return true;
        }

        std::cout << "Unknown command. Type 'help'.\n";
        return true;
    };

    while (true)
    {
        std::cout << game.render();

        if (game.solved())
        {
            std::cout << "Equation solved.\n";
            break;
        }

        std::cout << "> " << std::flush;
        const char key = read_key();
        if (key == '\0') break;

        if (key == 'w' || key == 'a' || key == 's' || key == 'd')
        {
            std::cout << key << '\n';
            const std::string one(1, key);
            if (!handle_line(one)) break;
            continue;
        }

        const std::string line = read_command_raw(key);
        if (!handle_line(line)) break;
    }

    return 0;
}
