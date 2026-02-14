// Small grid game inspired by commit/undo history snapshots.

#include <cassert>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>

using namespace std;

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

struct point_t
{
    int x {};
    int y {};
};

bool operator==(const point_t& a, const point_t& b)
{
    return a.x == b.x && a.y == b.y;
}

struct temporal_object_t
{
    string name;
    int value {};
    point_t pos {};
};

struct world_t
{
    int width {9};
    int height {7};
    point_t player_pos {0, 6};
    int commits_left {6};
    int undos_left {6};
    vector<temporal_object_t> objects;
};

bool in_bounds(const world_t& w, const point_t& p)
{
    return p.x >= 0 && p.y >= 0 && p.x < w.width && p.y < w.height;
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

bool blocked_static_cell(const point_t& p)
{
    return p == plus_cell() || p == equal_cell();
}

int object_at(const world_t& w, const point_t& p)
{
    for (size_t i = 0; i < w.objects.size(); ++i)
        if (w.objects[i].pos == p) return static_cast<int>(i);
    return -1;
}

void draw_world(const world_t& w)
{
    cout << "\nWorld commits/undos: " << w.commits_left << "/" << w.undos_left << "\n";
    for (size_t i = 0; i < w.objects.size(); ++i)
    {
        const auto& o = w.objects[i];
        cout << "Object " << i << " [" << o.name << "] at (" << o.pos.x << "," << o.pos.y << ")\n";
    }
    cout << "\n";

    for (int y = 0; y < w.height; ++y)
    {
        for (int x = 0; x < w.width; ++x)
        {
            point_t p {x, y};
            char c = '.';

            if (p == plus_cell()) c = '+';
            if (p == equal_cell()) c = '=';

            int idx = object_at(w, p);
            if (idx >= 0) c = static_cast<char>('0' + w.objects[idx].value);

            if (p == w.player_pos) c = '@';

            cout << c << ' ';
        }
        cout << '\n';
    }

    cout << "\nCommands: w/a/s/d, commit, undo, help, quit\n";
    cout << "Goal: place numbers so [slot]+[slot]=[slot] is true.\n";
    cout << "Equation row is y=1 with '+' and '=' fixed in place.\n";
}

bool solved_equation(const world_t& w)
{
    int lhs_i = object_at(w, lhs_slot());
    int rhs_i = object_at(w, rhs_slot());
    int result_i = object_at(w, result_slot());

    if (lhs_i < 0 || rhs_i < 0 || result_i < 0) return false;

    int lhs = w.objects[lhs_i].value;
    int rhs = w.objects[rhs_i].value;
    int result = w.objects[result_i].value;
    return lhs + rhs == result;
}

bool try_move_player(world_t& w, int dx, int dy)
{
    point_t next {w.player_pos.x + dx, w.player_pos.y + dy};
    if (!in_bounds(w, next)) return false;
    if (blocked_static_cell(next)) return false;

    int object_index = object_at(w, next);
    if (object_index < 0)
    {
        w.player_pos = next;
        return true;
    }

    point_t pushed {next.x + dx, next.y + dy};
    if (!in_bounds(w, pushed)) return false;
    if (blocked_static_cell(pushed)) return false;
    if (object_at(w, pushed) >= 0) return false;
    if (pushed == w.player_pos) return false;

    w.objects[object_index].pos = pushed;
    w.player_pos = next;
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

world_t make_world()
{
    world_t w;
    w.player_pos = {0, 6};
    w.objects = {
        {"one", 1, {4, 4}},
        {"two", 2, {7, 5}},
        {"three", 3, {6, 3}},
        {"four", 4, {2, 5}},
    };
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
        auto& w = current(h);
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
