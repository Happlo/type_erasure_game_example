#include "core/game.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class TileKind
{
    Empty,
    Player,
    Number,
    Plus,
    Equals,
    Wall,
    Symbol
};

struct Cell
{
    TileKind kind {TileKind::Empty};
    bool pushable {false};
    bool blocking {false};
    int number_value {0};
    char symbol {'*'};
    int facing_index {1};  // 0=north,1=south,2=west,3=east
};

struct BuilderMap
{
    int width {9};
    int height {7};
    int commits {6};
    int undos {6};
    std::vector<Cell> cells;

    Cell& at(int x, int y)
    {
        return cells[static_cast<size_t>(y * width + x)];
    }

    const Cell& at(int x, int y) const
    {
        return cells[static_cast<size_t>(y * width + x)];
    }

    void resize(int new_width, int new_height)
    {
        std::vector<Cell> next(static_cast<size_t>(new_width * new_height));
        for (int y = 0; y < new_height && y < height; ++y)
        {
            for (int x = 0; x < new_width && x < width; ++x)
            {
                next[static_cast<size_t>(y * new_width + x)] = at(x, y);
            }
        }
        width = new_width;
        height = new_height;
        cells = std::move(next);
    }
};

struct Brush
{
    TileKind kind {TileKind::Empty};
    bool pushable {false};
    bool blocking {false};
    int number_value {1};
    char symbol {'*'};
    int facing_index {1};
};

const char* facing_to_string(int facing_index)
{
    switch (facing_index)
    {
        case 0: return "north";
        case 1: return "south";
        case 2: return "west";
        case 3: return "east";
        default: return "south";
    }
}

char glyph_for_cell(const Cell& cell)
{
    switch (cell.kind)
    {
        case TileKind::Empty: return ' ';
        case TileKind::Player:
            if (cell.facing_index == 0) return '^';
            if (cell.facing_index == 1) return 'v';
            if (cell.facing_index == 2) return '<';
            if (cell.facing_index == 3) return '>';
            return 'v';
        case TileKind::Number: return static_cast<char>('0' + cell.number_value);
        case TileKind::Plus: return '+';
        case TileKind::Equals: return '=';
        case TileKind::Wall: return '#';
        case TileKind::Symbol: return cell.symbol;
    }
    return '?';
}

const char* kind_label(TileKind kind)
{
    switch (kind)
    {
        case TileKind::Empty: return "Empty";
        case TileKind::Player: return "Player";
        case TileKind::Number: return "Number";
        case TileKind::Plus: return "Plus";
        case TileKind::Equals: return "Equals";
        case TileKind::Wall: return "Wall";
        case TileKind::Symbol: return "Symbol";
    }
    return "Unknown";
}

Cell build_cell_from_brush(const Brush& brush)
{
    Cell cell;
    cell.kind = brush.kind;
    cell.pushable = brush.pushable;
    cell.blocking = brush.blocking;
    cell.number_value = brush.number_value;
    cell.symbol = brush.symbol;
    cell.facing_index = brush.facing_index;

    if (cell.kind == TileKind::Wall)
    {
        cell.blocking = true;
    }
    if (cell.kind == TileKind::Empty)
    {
        cell.pushable = false;
        cell.blocking = false;
    }

    return cell;
}

void ensure_single_player(BuilderMap& map, int keep_x, int keep_y)
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            if (x == keep_x && y == keep_y) continue;
            Cell& cell = map.at(x, y);
            if (cell.kind == TileKind::Player)
            {
                cell = Cell {};
            }
        }
    }
}

nlohmann::json tile_to_json(const Cell& cell, int x, int y)
{
    nlohmann::json tile;
    tile["x"] = x;
    tile["y"] = y;

    switch (cell.kind)
    {
        case TileKind::Player:
            tile["kind"] = "player";
            tile["facing"] = facing_to_string(cell.facing_index);
            break;
        case TileKind::Number:
            tile["kind"] = "number";
            tile["value"] = cell.number_value;
            break;
        case TileKind::Plus: tile["kind"] = "plus"; break;
        case TileKind::Equals: tile["kind"] = "equals"; break;
        case TileKind::Wall: tile["kind"] = "wall"; break;
        case TileKind::Symbol:
            tile["kind"] = "symbol";
            tile["glyph"] = std::string(1, cell.symbol);
            break;
        case TileKind::Empty:
            tile["kind"] = "empty";
            break;
    }

    if (cell.pushable) tile["pushable"] = true;
    if (cell.blocking && cell.kind != TileKind::Wall) tile["blocking"] = true;
    return tile;
}

std::string map_to_json(const BuilderMap& map)
{
    nlohmann::json root;
    root["version"] = 1;
    root["size"] = { {"width", map.width}, {"height", map.height} };
    root["resources"] = { {"commits", map.commits}, {"undos", map.undos} };
    root["tiles"] = nlohmann::json::array();

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const Cell& cell = map.at(x, y);
            if (cell.kind == TileKind::Empty) continue;
            root["tiles"].push_back(tile_to_json(cell, x, y));
        }
    }

    return root.dump(2);
}

std::optional<BuilderMap> map_from_json(const std::string& text, std::string& error_message)
{
    try
    {
        const nlohmann::json root = nlohmann::json::parse(text);
        BuilderMap map;
        map.width = root.at("size").at("width").get<int>();
        map.height = root.at("size").at("height").get<int>();
        if (map.width <= 0 || map.height <= 0)
        {
            error_message = "Map width and height must be > 0";
            return std::nullopt;
        }

        map.commits = root.value("resources", nlohmann::json::object()).value("commits", 6);
        map.undos = root.value("resources", nlohmann::json::object()).value("undos", 6);
        map.cells.assign(static_cast<size_t>(map.width * map.height), Cell {});

        for (const auto& tile : root.at("tiles"))
        {
            const int x = tile.at("x").get<int>();
            const int y = tile.at("y").get<int>();
            if (x < 0 || y < 0 || x >= map.width || y >= map.height)
            {
                error_message = "Tile is out of bounds";
                return std::nullopt;
            }

            Cell cell;
            const std::string kind = tile.at("kind").get<std::string>();
            if (kind == "player")
            {
                cell.kind = TileKind::Player;
                const std::string facing = tile.value("facing", std::string("south"));
                if (facing == "north") cell.facing_index = 0;
                else if (facing == "south") cell.facing_index = 1;
                else if (facing == "west") cell.facing_index = 2;
                else if (facing == "east") cell.facing_index = 3;
                else
                {
                    error_message = "Invalid player facing";
                    return std::nullopt;
                }
            }
            else if (kind == "number")
            {
                cell.kind = TileKind::Number;
                cell.number_value = tile.at("value").get<int>();
                if (cell.number_value < 0 || cell.number_value > 9)
                {
                    error_message = "Number tile value must be 0..9";
                    return std::nullopt;
                }
            }
            else if (kind == "plus") cell.kind = TileKind::Plus;
            else if (kind == "equals") cell.kind = TileKind::Equals;
            else if (kind == "wall") cell.kind = TileKind::Wall;
            else if (kind == "symbol")
            {
                cell.kind = TileKind::Symbol;
                const std::string glyph = tile.value("glyph", std::string("*"));
                if (glyph.size() != 1)
                {
                    error_message = "Symbol glyph must be exactly one character";
                    return std::nullopt;
                }
                cell.symbol = glyph[0];
            }
            else if (kind == "empty")
            {
                cell.kind = TileKind::Empty;
            }
            else
            {
                error_message = "Unknown tile kind: " + kind;
                return std::nullopt;
            }

            cell.pushable = tile.value("pushable", false);
            cell.blocking = tile.value("blocking", false);
            if (cell.kind == TileKind::Wall) cell.blocking = true;

            map.at(x, y) = cell;
        }

        bool found_player = false;
        for (const Cell& cell : map.cells)
        {
            if (cell.kind == TileKind::Player)
            {
                if (found_player)
                {
                    error_message = "Map must have exactly one player";
                    return std::nullopt;
                }
                found_player = true;
            }
        }
        if (!found_player)
        {
            error_message = "Map must contain a player";
            return std::nullopt;
        }

        return map;
    }
    catch (const std::exception& ex)
    {
        error_message = ex.what();
        return std::nullopt;
    }
}

BuilderMap make_default_map()
{
    BuilderMap map;
    map.cells.assign(static_cast<size_t>(map.width * map.height), Cell {});

    map.at(0, 6).kind = TileKind::Player;
    map.at(0, 6).facing_index = 1;

    map.at(2, 1).kind = TileKind::Plus;
    map.at(2, 1).pushable = true;

    map.at(4, 1).kind = TileKind::Equals;
    map.at(4, 1).pushable = true;

    map.at(4, 4).kind = TileKind::Number;
    map.at(4, 4).number_value = 1;
    map.at(4, 4).pushable = true;

    map.at(7, 5).kind = TileKind::Number;
    map.at(7, 5).number_value = 2;
    map.at(7, 5).pushable = true;

    map.at(6, 3).kind = TileKind::Number;
    map.at(6, 3).number_value = 3;
    map.at(6, 3).pushable = true;

    map.at(2, 5).kind = TileKind::Number;
    map.at(2, 5).number_value = 4;
    map.at(2, 5).pushable = true;

    return map;
}

bool save_file(const std::string& path, const std::string& content)
{
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

bool load_file(const std::string& path, std::string& content)
{
    std::ifstream in(path);
    if (!in) return false;
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Type Erasure Map Builder", nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    BuilderMap map = make_default_map();
    Brush brush;

    std::array<char, 256> file_path {};
    std::snprintf(file_path.data(), file_path.size(), "%s", "maps/try_map.json");

    std::string status = "Ready.";

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Tools");

        int new_width = map.width;
        int new_height = map.height;
        ImGui::InputInt("Width", &new_width);
        ImGui::InputInt("Height", &new_height);
        if (new_width < 1) new_width = 1;
        if (new_height < 1) new_height = 1;
        if (new_width > 64) new_width = 64;
        if (new_height > 64) new_height = 64;
        if (ImGui::Button("Resize Map"))
        {
            map.resize(new_width, new_height);
            status = "Resized map.";
        }

        ImGui::InputInt("Commits", &map.commits);
        ImGui::InputInt("Undos", &map.undos);

        ImGui::Separator();
        ImGui::Text("Brush");
        const std::array<TileKind, 7> kinds {
            TileKind::Empty,
            TileKind::Player,
            TileKind::Number,
            TileKind::Plus,
            TileKind::Equals,
            TileKind::Wall,
            TileKind::Symbol
        };

        for (TileKind kind : kinds)
        {
            const bool selected = brush.kind == kind;
            if (ImGui::Selectable(kind_label(kind), selected))
            {
                brush.kind = kind;
            }
        }

        if (brush.kind == TileKind::Number)
        {
            ImGui::InputInt("Number Value", &brush.number_value);
            if (brush.number_value < 0) brush.number_value = 0;
            if (brush.number_value > 9) brush.number_value = 9;
        }
        if (brush.kind == TileKind::Symbol)
        {
            char symbol_buffer[2] {brush.symbol, '\0'};
            ImGui::InputText("Symbol", symbol_buffer, sizeof(symbol_buffer));
            brush.symbol = symbol_buffer[0] == '\0' ? '*' : symbol_buffer[0];
        }
        if (brush.kind == TileKind::Player)
        {
            static const char* directions[] = {"north", "south", "west", "east"};
            ImGui::Combo("Facing", &brush.facing_index, directions, 4);
        }

        ImGui::Checkbox("Pushable", &brush.pushable);
        ImGui::Checkbox("Blocking", &brush.blocking);

        ImGui::Separator();
        ImGui::InputText("File", file_path.data(), file_path.size());
        if (ImGui::Button("Save JSON"))
        {
            const std::string json_text = map_to_json(map);
            if (!save_file(file_path.data(), json_text))
            {
                status = "Failed to save file.";
            }
            else
            {
                status = "Saved map JSON.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load JSON"))
        {
            std::string text;
            if (!load_file(file_path.data(), text))
            {
                status = "Failed to load file.";
            }
            else
            {
                std::string error;
                auto parsed = map_from_json(text, error);
                if (!parsed)
                {
                    status = "Invalid map JSON: " + error;
                }
                else
                {
                    map = *parsed;
                    status = "Loaded map JSON.";
                }
            }
        }

        if (ImGui::Button("Validate with core parser"))
        {
            try
            {
                (void)core::Game::from_json(map_to_json(map));
                status = "Core parser validation passed.";
            }
            catch (const std::exception& ex)
            {
                status = std::string("Core parser validation failed: ") + ex.what();
            }
        }

        ImGui::TextWrapped("%s", status.c_str());

        ImGui::End();

        ImGui::Begin("Grid");
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                Cell& cell = map.at(x, y);
                const char glyph = glyph_for_cell(cell);
                char label[32];
                std::snprintf(label, sizeof(label), "%c##%d_%d", glyph, x, y);

                if (ImGui::Button(label, ImVec2(28.0f, 28.0f)))
                {
                    cell = build_cell_from_brush(brush);
                    if (cell.kind == TileKind::Player) ensure_single_player(map, x, y);
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    cell = Cell {};
                }

                if (x + 1 < map.width) ImGui::SameLine();
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
