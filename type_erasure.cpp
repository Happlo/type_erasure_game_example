// Small grid game inspired by commit/undo history snapshots.

#include <cassert>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>

using namespace std;

struct point_t
{
    int x {};
    int y {};
};

bool operator==(const point_t& a, const point_t& b)
{
    return a.x == b.x && a.y == b.y;
}

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

struct number_t
{
    string name;
    int value {};
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

char glyph(const number_t& x)
{
    return static_cast<char>('0' + x.value);
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
bool is_pushable(const T&)
{
    return false;
}

bool is_pushable(const number_t&)
{
    return true;
}

template <typename T>
bool blocks_movement(const T&)
{
    return false;
}

bool blocks_movement(const plus_t&)
{
    return true;
}

bool blocks_movement(const equal_t&)
{
    return true;
}

template <typename T>
int number_value(const T&)
{
    return -1;
}

int number_value(const number_t& x)
{
    return x.value;
}

class object_t
{
public:
    template <typename T>
    object_t(T x) : self_(make_shared<model<T>>(move(x)))
    {
    }

    object_t() : object_t(empty_t {})
    {
    }

    char glyph() const
    {
        return self_->glyph_();
    }

    bool is_empty() const
    {
        return self_->is_empty_();
    }

    bool is_player() const
    {
        return self_->is_player_();
    }

    bool is_pushable() const
    {
        return self_->is_pushable_();
    }

    bool blocks_movement() const
    {
        return self_->blocks_movement_();
    }

    int number_value() const
    {
        return self_->number_value_();
    }

private:
    struct concept_t
    {
        virtual ~concept_t() = default;
        virtual char glyph_() const = 0;
        virtual bool is_empty_() const = 0;
        virtual bool is_player_() const = 0;
        virtual bool is_pushable_() const = 0;
        virtual bool blocks_movement_() const = 0;
        virtual int number_value_() const = 0;
    };

    template <typename T>
    struct model : concept_t
    {
        explicit model(T x) : data_(move(x))
        {
        }

        char glyph_() const override
        {
            return ::glyph(data_);
        }

        bool is_empty_() const override
        {
            return ::is_empty(data_);
        }

        bool is_player_() const override
        {
            return ::is_player(data_);
        }

        bool is_pushable_() const override
        {
            return ::is_pushable(data_);
        }

        bool blocks_movement_() const override
        {
            return ::blocks_movement(data_);
        }

        int number_value_() const override
        {
            return ::number_value(data_);
        }

        T data_;
    };

    shared_ptr<const concept_t> self_;
};

template <typename T>
using history_t = vector<T>;

template <typename T>
void commit(history_t<T>& x)
{
    assert(!x.empty());
    x.push_back(x.back());
}

template <typename T>
void undo(history_t<T>& x)
{
    assert(!x.empty());
    x.pop_back();
}

template <typename T>
T& current(history_t<T>& x)
{
    assert(!x.empty());
    return x.back();
}

struct world_t
{
    int commits_left {6};
    int undos_left {6};
    vector<vector<object_t>> grid;
};

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

point_t plus_cell()
{
    return {2, 1};
}

point_t equal_cell()
{
    return {4, 1};
}

point_t find_player(const world_t& w)
{
    for (int y = 0; y < grid_height(w); ++y)
    {
        for (int x = 0; x < grid_width(w); ++x)
        {
            point_t p {x, y};
            if (cell(w, p).is_player()) return p;
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
            cout << cell(w, p).glyph() << ' ';
        }
        cout << '\n';
    }

    cout << "\nCommands: w/a/s/d, commit, undo, help, quit\n";
    cout << "Goal: place numbers so [slot]+[slot]=[slot] is true.\n";
    cout << "Equation row is y=1 with '+' and '=' fixed in place.\n";
}

bool solved_equation(const world_t& w)
{
    int lhs = cell(w, lhs_slot()).number_value();
    int rhs = cell(w, rhs_slot()).number_value();
    int result = cell(w, result_slot()).number_value();

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
    if (next_object.blocks_movement()) return false;

    if (next_object.is_empty())
    {
        cell(w, player) = object_t(empty_t {});
        cell(w, next) = object_t(player_t {});
        return true;
    }

    if (!next_object.is_pushable()) return false;

    point_t pushed {next.x + dx, next.y + dy};
    if (!in_bounds(w, pushed)) return false;

    const object_t pushed_object = cell(w, pushed);
    if (!pushed_object.is_empty()) return false;

    cell(w, pushed) = next_object;
    cell(w, next) = object_t(player_t {});
    cell(w, player) = object_t(empty_t {});
    return true;
}

bool world_commit(history_t<world_t>& h)
{
    auto& w = current(h);
    if (w.commits_left <= 0) return false;
    int commits_after = w.commits_left - 1;
    commit(h);
    current(h).commits_left = commits_after;
    return true;
}

bool world_undo(history_t<world_t>& h)
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

void place(world_t& w, const point_t& p, object_t x)
{
    cell(w, p) = move(x);
}

world_t make_world()
{
    constexpr int width = 9;
    constexpr int height = 7;

    world_t w;
    w.grid.assign(static_cast<size_t>(height),
                  vector<object_t>(static_cast<size_t>(width), object_t(empty_t {})));

    place(w, {0, 6}, player_t {});
    place(w, plus_cell(), plus_t {});
    place(w, equal_cell(), equal_t {});

    place(w, {4, 4}, number_t {"one", 1});
    place(w, {7, 5}, number_t {"two", 2});
    place(w, {6, 3}, number_t {"three", 3});
    place(w, {2, 5}, number_t {"four", 4});

    return w;
}

int main()
{
    history_t<world_t> h {make_world()};

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
