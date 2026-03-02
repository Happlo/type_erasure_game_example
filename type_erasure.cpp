// Small grid game inspired by commit/undo history snapshots.

#include <cassert>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <termios.h>
#include <unistd.h>

using namespace std;

struct Point
{
    int x {};
    int y {};
};

struct Empty
{
};

struct Player
{
    enum class Facing
    {
        north,
        south,
        west,
        east
    };

    Facing facing {Facing::south};

    static Player from_delta(int dx, int dy)
    {
        if (dx == 0 && dy < 0) return {Facing::north};
        if (dx == 0 && dy > 0) return {Facing::south};
        if (dx < 0 && dy == 0) return {Facing::west};
        if (dx > 0 && dy == 0) return {Facing::east};
        return {};
    }

    friend char glyph(const Player& player)
    {
        switch (player.facing)
        {
            case Facing::north: return '^';
            case Facing::south: return 'v';
            case Facing::west: return '<';
            case Facing::east: return '>';
        }
        return '@';
    }
};

char glyph(const Empty&)
{
    return ' ';
}

char glyph(const int& x)
{
    if (0 < x || x > 9 )
    {
        throw std::runtime_error("int must be between 0 and 9 to generate a glyph");
    }
    return static_cast<char>('0' + x);
}

char glyph(const char& x)
{
    return x;
}

template <typename T>
bool is_empty(const T&)
{
    return false;
}

bool is_empty(const Empty&)
{
    return true;
}

template <typename T>
bool is_player(const T&)
{
    return false;
}

bool is_player(const Player&)
{
    return true;
}

template <typename T>
int number_value(const T&)
{
    return -1;
}

int number_value(const int& x)
{
    return x;
}

struct ObjectProperties
{
    char glyph {'?'};
    bool is_empty {false};
    bool is_player {false};
    bool is_pushable {false};
    bool blocks_movement {false};
    int number_value {-1};
};

template <typename T>
ObjectProperties create_properties(const T& x, bool is_pushable, bool blocks_movement)
{
    return {glyph(x), ::is_empty(x), ::is_player(x), is_pushable, blocks_movement, ::number_value(x)};
}

class Object
{
public:
    template <typename T>
    Object(T x, bool is_pushable = false, bool blocks_movement = false)
        : self_(make_shared<Model<T>>(move(x), is_pushable, blocks_movement))
    {}

    ObjectProperties properties() const
    {
        return self_->properties_();
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual ObjectProperties properties_() const = 0;
    };

    template <typename T>
    struct Model : Concept
    {
        explicit Model(T x, bool is_pushable, bool blocks_movement) : data_(move(x))
        {
            properties = create_properties(data_, is_pushable, blocks_movement);
        }

        ObjectProperties properties_() const override
        {
            auto properties2 = create_properties(data_, properties.is_pushable, properties.blocks_movement);
            return properties2;
            // return properties;
        }

        T data_;
        ObjectProperties properties {};
    };

    shared_ptr<const Concept> self_;
};

template <typename T>
class MakeObject
{
public:
    explicit MakeObject(T x) : value_(move(x))
    {
    }

    MakeObject pushable() const
    {
        MakeObject out = *this;
        out.is_pushable_ = true;
        return out;
    }

    MakeObject blocking() const
    {
        MakeObject out = *this;
        out.blocks_movement_ = true;
        return out;
    }

    Object build() &&
    {
        return Object(move(value_), is_pushable_, blocks_movement_);
    }

    operator Object() &&
    {
        return move(*this).build();
    }

private:
    T value_;
    bool is_pushable_ {false};
    bool blocks_movement_ {false};
};

struct World
{
    int commits_left {6};
    int undos_left {6};
    vector<vector<Object>> grid;
};


using History = vector<World>;

void commit(History& x)
{
    assert(!x.empty());
    x.push_back(x.back());
}

void undo(History& x)
{
    assert(!x.empty());
    x.pop_back();
}

World& current(History& x)
{
    assert(!x.empty());
    return x.back();
}

int grid_height(const World& w)
{
    return static_cast<int>(w.grid.size());
}

int grid_width(const World& w)
{
    if (w.grid.empty()) return 0;
    return static_cast<int>(w.grid[0].size());
}

bool in_bounds(const World& w, const Point& p)
{
    return p.x >= 0 && p.y >= 0 && p.x < grid_width(w) && p.y < grid_height(w);
}

Object& cell(World& w, const Point& p)
{
    return w.grid[p.y][p.x];
}

const Object& cell(const World& w, const Point& p)
{
    return w.grid[p.y][p.x];
}

Point find_player(const World& w)
{
    for (int y = 0; y < grid_height(w); ++y)
    {
        for (int x = 0; x < grid_width(w); ++x)
        {
            Point p {x, y};
            if (cell(w, p).properties().is_player) return p;
        }
    }
    throw std::runtime_error("Player not found");
}

void draw_world(const World& w)
{
    cout << "\nWorld commits/undos: " << w.commits_left << "/" << w.undos_left << "\n\n";

    for (int y = 0; y < grid_height(w); ++y)
    {
        for (int x = 0; x < grid_width(w); ++x)
        {
            Point p {x, y};
            cout << cell(w, p).properties().glyph << ' ';
        }
        cout << '\n';
    }

    cout << "\nCommands: w/a/s/d, commit, undo, help, quit\n";
    cout << "Goal: make the row containing '=' form a true equation.\n";
    cout << "The '+' and '=' tiles stay fixed in place.\n";
}

bool equation_is_correct(const std::string& equation)
{
    string compact;
    for (char ch : equation)
    {
        if (ch != ' ') compact.push_back(ch);
    }

    if (compact.size() != 5) return false;
    if (!isdigit(static_cast<unsigned char>(compact[0]))) return false;
    if (compact[1] != '+') return false;
    if (!isdigit(static_cast<unsigned char>(compact[2]))) return false;
    if (compact[3] != '=') return false;
    if (!isdigit(static_cast<unsigned char>(compact[4]))) return false;

    int lhs = compact[0] - '0';
    int rhs = compact[2] - '0';
    int result = compact[4] - '0';
    return lhs + rhs == result;
}

bool solved_equation(const World& w)
{
    for (int y = 0; y < grid_height(w); ++y)
    {
        string row;
        row.reserve(static_cast<size_t>(grid_width(w)));
        for (int x = 0; x < grid_width(w); ++x)
        {
            row.push_back(cell(w, {x, y}).properties().glyph);
        }

        if (row.find('=') != string::npos) return equation_is_correct(row);
    }

    return false;
}

bool try_move_player(World& w, int dx, int dy)
{
    Point player = find_player(w);
    const Player moved_player = Player::from_delta(dx, dy);

    Point next {player.x + dx, player.y + dy};
    if (!in_bounds(w, next)) return false;

    const Object next_object = cell(w, next);
    const auto next_props = next_object.properties();
    if (next_props.blocks_movement) return false;

    if (next_props.is_empty)
    {
        cell(w, player) = Object(Empty {});
        cell(w, next) = Object(moved_player);
        return true;
    }

    if (!next_props.is_pushable) return false;

    Point pushed {next.x + dx, next.y + dy};
    if (!in_bounds(w, pushed)) return false;

    const Object pushed_object = cell(w, pushed);
    if (!pushed_object.properties().is_empty) return false;

    cell(w, pushed) = next_object;
    cell(w, next) = Object(moved_player);
    cell(w, player) = Object(Empty {});
    return true;
}

bool world_commit(History& h)
{
    auto& w = current(h);
    if (w.commits_left <= 0) return false;
    int commits_after = w.commits_left - 1;
    commit(h);
    current(h).commits_left = commits_after;
    return true;
}

bool world_undo(History& h)
{
    auto& w = current(h);
    if (w.undos_left <= 0) return false;
    if (h.size() <= 1) return false;
    int undos_after = w.undos_left - 1;
    undo(h);
    current(h).undos_left = undos_after;
    return true;
}

void show_help()
{
    cout << "w/a/s/d   Move player\n";
    cout << "          Walk into a number to push it one cell\n";
    cout << "commit    Save whole-world snapshot (costs 1 world commit)\n";
    cout << "undo      Restore previous world snapshot (costs 1 world undo)\n";
    cout << "quit      Exit\n";
}

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

string read_command_raw(char first)
{
    string line;
    if (first == '\r' || first == '\n')
    {
        cout << '\n';
        return line;
    }

    line.push_back(first);
    cout << first << flush;

    while (true)
    {
        char ch = read_key();
        if (ch == '\0') return line;

        if (ch == '\r' || ch == '\n')
        {
            cout << '\n';
            return line;
        }

        if (ch == 127 || ch == '\b')
        {
            if (!line.empty())
            {
                line.pop_back();
                cout << "\b \b" << flush;
            }
            continue;
        }

        if (isprint(static_cast<unsigned char>(ch)))
        {
            line.push_back(ch);
            cout << ch << flush;
        }
    }
}

World make_world()
{
    constexpr size_t width = 9;
    constexpr size_t height = 7;

    World w;
    w.grid.assign(height, vector<Object>(width, Empty {}));

    cell(w, {0, 6}) = MakeObject(Player {});
    cell(w, {2, 1}) = MakeObject('+').pushable();
    cell(w, {4, 1}) = MakeObject('=').pushable();

    cell(w, {4, 4}) = MakeObject(1).pushable();
    cell(w, {7, 5}) = MakeObject(2).pushable();
    cell(w, {6, 3}) = MakeObject(3).pushable();
    cell(w, {2, 5}) = MakeObject(4).pushable();

    return w;
}

int main()
{
    History h {make_world()};

    cout << "Time Grid\n";
    show_help();

    RawModeGuard raw_mode;
    if (!raw_mode.enable())
    {
        cerr << "Failed to enable raw terminal input.\n";
        return 1;
    }
    cout << "Immediate input enabled: movement keys w/a/s/d do not require Enter.\n";

    auto handle_line = [&](const string& line) -> bool {
        if (line == "quit") return false;
        if (line == "help")
        {
            show_help();
            return true;
        }

        if (line == "w") { try_move_player(current(h), 0, -1); return true; }
        if (line == "a") { try_move_player(current(h), -1, 0); return true; }
        if (line == "s") { try_move_player(current(h), 0, 1); return true; }
        if (line == "d") { try_move_player(current(h), 1, 0); return true; }

        if (line == "commit")
        {
            if (!world_commit(h)) cout << "Cannot commit world.\n";
            return true;
        }

        if (line == "undo")
        {
            if (!world_undo(h)) cout << "Cannot undo world.\n";
            return true;
        }

        cout << "Unknown command. Type 'help'.\n";
        return true;
    };

    string line;
    while (true)
    {
        const auto& w = current(h);
        draw_world(w);

        if (solved_equation(w))
        {
            cout << "Equation solved.\n";
            break;
        }

        cout << "> " << flush;

        char key = read_key();
        if (key == '\0') break;

        if (key == 'w' || key == 'a' || key == 's' || key == 'd')
        {
            cout << key << '\n';
            string one(1, key);
            if (!handle_line(one)) break;
            continue;
        }

        line = read_command_raw(key);
        if (!handle_line(line)) break;
    }

    return 0;
}
