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

struct Cell
{
    bool occupied {false};
    char symbol {'*'};
    bool pushable {false};
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
    char symbol {'*'};
    bool pushable {false};
};

bool is_player_symbol(const char symbol)
{
    return symbol == '^' || symbol == 'v' || symbol == '<' || symbol == '>';
}

char glyph_for_cell(const Cell& cell)
{
    return cell.occupied ? cell.symbol : ' ';
}

Cell build_cell_from_brush(const Brush& brush)
{
    Cell cell;
    cell.occupied = true;
    cell.symbol = brush.symbol;
    cell.pushable = brush.pushable;
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
            if (cell.occupied && is_player_symbol(cell.symbol))
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
    tile["symbol"] = std::string(1, glyph_for_cell(cell));

    if (cell.pushable) tile["pushable"] = true;
    return tile;
}

nlohmann::json map_to_json_object(const BuilderMap& map)
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
            if (!cell.occupied) continue;
            root["tiles"].push_back(tile_to_json(cell, x, y));
        }
    }

    return root;
}

std::string map_to_json(const BuilderMap& map)
{
    return map_to_json_object(map).dump(2);
}

std::optional<BuilderMap> map_from_json(const std::string& text, std::string& error_message)
{
    try
    {
        std::unique_ptr<core::Game> game = core::Game::from_json(text);
        const core::MapView view = game->view();

        BuilderMap map;
        map.width = view.width;
        map.height = view.height;
        map.commits = view.commits_left;
        map.undos = view.undos_left;
        map.cells.assign(static_cast<size_t>(map.width * map.height), Cell {});

        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const core::CellView& cell_view = view.at(x, y);
                if (std::holds_alternative<core::Empty>(cell_view.properties))
                {
                    map.at(x, y) = Cell {};
                    continue;
                }
                Cell cell;
                cell.occupied = true;
                cell.symbol = cell_view.symbol;
                if (const auto* obj = std::get_if<core::Object>(&cell_view.properties))
                {
                    cell.pushable = obj->is_pushable;
                }
                map.at(x, y) = cell;
            }
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

    map.at(0, 6).occupied = true;
    map.at(0, 6).symbol = 'v';
    map.at(0, 6).pushable = false;

    map.at(2, 1).occupied = true;
    map.at(2, 1).symbol = '+';
    map.at(2, 1).pushable = true;

    map.at(4, 1).occupied = true;
    map.at(4, 1).symbol = '=';
    map.at(4, 1).pushable = true;

    map.at(4, 4).occupied = true;
    map.at(4, 4).symbol = '1';
    map.at(4, 4).pushable = true;

    map.at(7, 5).occupied = true;
    map.at(7, 5).symbol = '2';
    map.at(7, 5).pushable = true;

    map.at(6, 3).occupied = true;
    map.at(6, 3).symbol = '3';
    map.at(6, 3).pushable = true;

    map.at(2, 5).occupied = true;
    map.at(2, 5).symbol = '4';
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
    int resize_width = map.width;
    int resize_height = map.height;

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

        ImGui::InputInt("Width", &resize_width);
        ImGui::InputInt("Height", &resize_height);
        if (resize_width < 1) resize_width = 1;
        if (resize_height < 1) resize_height = 1;
        if (resize_width > 64) resize_width = 64;
        if (resize_height > 64) resize_height = 64;
        if (ImGui::Button("Resize Map"))
        {
            map.resize(resize_width, resize_height);
            status = "Resized map.";
        }

        ImGui::InputInt("Commits", &map.commits);
        ImGui::InputInt("Undos", &map.undos);

        ImGui::Separator();
        ImGui::Text("Brush");
        char symbol_buffer[2] {brush.symbol, '\0'};
        ImGui::InputText("Symbol", symbol_buffer, sizeof(symbol_buffer));
        brush.symbol = symbol_buffer[0] == '\0' ? '*' : symbol_buffer[0];
        ImGui::Checkbox("Pushable", &brush.pushable);
        ImGui::TextUnformatted("Tips: use ^ v < > for player facing, 0..9 for numbers.");

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
                    resize_width = map.width;
                    resize_height = map.height;
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
                    if (is_player_symbol(cell.symbol)) ensure_single_player(map, x, y);
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
