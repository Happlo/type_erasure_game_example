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

struct point_t
{
    int x {};
    int y {};
};

struct empty_t
{
};

struct player_t
{
};

struct plus_t
{
};

struct equal_t
{
};

char glyph(const empty_t&)
{
    return '.';
}

char glyph(const player_t&)
{
    return '@';
}

char glyph(const plus_t&)
{
    return '+';
}

char glyph(const equal_t&)
{
    return '=';
}

char glyph(const int& x)
{
    return static_cast<char>('0' + x);
}

template <typename T>
bool is_empty(const T&)
{
    return false;
}

bool is_empty(const empty_t&)
{
    return true;
}

template <typename T>
bool is_player(const T&)
{
    return false;
}

bool is_player(const player_t&)
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

struct object_properties_t
{
    char glyph {'?'};
    bool is_empty {false};
    bool is_player {false};
    bool is_pushable {false};
    bool blocks_movement {false};
    int number_value {-1};
};

template <typename T>
object_properties_t create_properties(const T& x, bool is_pushable, bool blocks_movement)
{
    return {::glyph(x), ::is_empty(x), ::is_player(x), is_pushable, blocks_movement, ::number_value(x)};
}

class object_t
{
public:
    template <typename T>
    object_t(T x, bool is_pushable = false, bool blocks_movement = false)
        : self_(make_shared<model<T>>(move(x), is_pushable, blocks_movement))
    {}

    object_properties_t properties() const
    {
        return self_->properties_();
    }

private:
    struct concept_t
    {
        virtual ~concept_t() = default;
        virtual object_properties_t properties_() const = 0;
    };

    template <typename T>
    struct model : concept_t
    {
        explicit model(T x, bool is_pushable, bool blocks_movement) : data_(move(x))
        {
            properties = create_properties(data_, is_pushable, blocks_movement);
        }

        object_properties_t properties_() const override
        {
            return properties;
        }

        T data_;
        object_properties_t properties {};
    };

    shared_ptr<const concept_t> self_;
};

template <typename T>
class make_object
{
public:
    explicit make_object(T x) : value_(move(x))
    {
    }

    make_object pushable() const
    {
        make_object out = *this;
        out.is_pushable_ = true;
        return out;
    }

    make_object blocking() const
    {
        make_object out = *this;
        out.blocks_movement_ = true;
        return out;
    }

    object_t build() &&
    {
        return object_t(move(value_), is_pushable_, blocks_movement_);
    }

    operator object_t() &&
    {
        return move(*this).build();
    }

private:
    T value_;
    bool is_pushable_ {false};
    bool blocks_movement_ {false};
};

struct world_t
{
    int commits_left {6};
    int undos_left {6};
    vector<vector<object_t>> grid;
};


using history_t = vector<world_t>;

void commit(history_t& x)
{
    assert(!x.empty());
    x.push_back(x.back());
}

void undo(history_t& x)
{
    assert(!x.empty());
    x.pop_back();
}

world_t& current(history_t& x)
{
    assert(!x.empty());
    return x.back();
}

int grid_height(const world_t& w)
{
    return static_cast<int>(w.grid.size());
}

int grid_width(const world_t& w)
{
    if (w.grid.empty()) return 0;
    return static_cast<int>(w.grid[0].size());
}

bool in_bounds(const world_t& w, const point_t& p)
{
    return p.x >= 0 && p.y >= 0 && p.x < grid_width(w) && p.y < grid_height(w);
}

object_t& cell(world_t& w, const point_t& p)
{
    return w.grid[p.y][p.x];
}

const object_t& cell(const world_t& w, const point_t& p)
{
    return w.grid[p.y][p.x];
}

point_t lhs_slot()
{
    return {1, 1};
}

point_t rhs_slot()
{
    return {3, 1};
}

point_t result_slot()
{
    return {5, 1};
}

point_t find_player(const world_t& w)
{
    for (int y = 0; y < grid_height(w); ++y)
    {
        for (int x = 0; x < grid_width(w); ++x)
        {
            point_t p {x, y};
            if (cell(w, p).properties().is_player) return p;
        }
    }
    return {-1, -1};
}

void draw_world(const world_t& w)
{
    cout << "\nWorld commits/undos: " << w.commits_left << "/" << w.undos_left << "\n\n";

    for (int y = 0; y < grid_height(w); ++y)
    {
        for (int x = 0; x < grid_width(w); ++x)
        {
            point_t p {x, y};
            cout << cell(w, p).properties().glyph << ' ';
        }
        cout << '\n';
    }

    cout << "\nCommands: w/a/s/d, commit, undo, help, quit\n";
    cout << "Goal: place numbers so [slot]+[slot]=[slot] is true.\n";
    cout << "Equation row is y=1 with '+' and '=' fixed in place.\n";
}

bool solved_equation(const world_t& w)
{
    int lhs = cell(w, lhs_slot()).properties().number_value;
    int rhs = cell(w, rhs_slot()).properties().number_value;
    int result = cell(w, result_slot()).properties().number_value;

    if (lhs < 0 || rhs < 0 || result < 0) return false;
    return lhs + rhs == result;
}

bool try_move_player(world_t& w, int dx, int dy)
{
    point_t player = find_player(w);
    if (player.x < 0) return false;

    point_t next {player.x + dx, player.y + dy};
    if (!in_bounds(w, next)) return false;

    const object_t next_object = cell(w, next);
    const auto next_props = next_object.properties();
    if (next_props.blocks_movement) return false;

    if (next_props.is_empty)
    {
        cell(w, player) = object_t(empty_t {});
        cell(w, next) = object_t(player_t {});
        return true;
    }

    if (!next_props.is_pushable) return false;

    point_t pushed {next.x + dx, next.y + dy};
    if (!in_bounds(w, pushed)) return false;

    const object_t pushed_object = cell(w, pushed);
    if (!pushed_object.properties().is_empty) return false;

    cell(w, pushed) = next_object;
    cell(w, next) = object_t(player_t {});
    cell(w, player) = object_t(empty_t {});
    return true;
}

bool world_commit(history_t& h)
{
    auto& w = current(h);
    if (w.commits_left <= 0) return false;
    int commits_after = w.commits_left - 1;
    commit(h);
    current(h).commits_left = commits_after;
    return true;
}

bool world_undo(history_t& h)
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

struct raw_mode_guard_t
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

    ~raw_mode_guard_t()
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

world_t make_world()
{
    constexpr int width = 9;
    constexpr int height = 7;

    world_t w;
    w.grid.assign(static_cast<size_t>(height),
                  vector<object_t>(static_cast<size_t>(width), empty_t {}));

    cell(w, {0, 6}) = object_t(player_t {});
    cell(w, {2, 1}) = make_object(plus_t {}).blocking();
    cell(w, {5, 1}) = make_object(equal_t {}).blocking();

    cell(w, {4, 4}) = make_object(1).pushable();
    cell(w, {7, 5}) = make_object(2).pushable();
    cell(w, {6, 3}) = make_object(3).pushable();
    cell(w, {2, 5}) = make_object(4).pushable();

    return w;
}

int main()
{
    history_t h {make_world()};

    cout << "Time Grid\n";
    show_help();

    raw_mode_guard_t raw_mode;
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
