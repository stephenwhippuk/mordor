#include "mordor/map.hpp"

#include <algorithm>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace mordor {

namespace {

bool is_supported_tile(char symbol)
{
    return symbol == '#' || symbol == '.' || symbol == 'D' || symbol == 'K' || symbol == 'S';
}

bool tile_blocks_movement(char symbol)
{
    return symbol == '#' || symbol == 'D';
}

std::size_t tile_index(const DungeonMap& map, int col, int row)
{
    return static_cast<std::size_t>(row * map.m_width + col);
}

bool is_in_bounds(const DungeonMap& map, int col, int row)
{
    return col >= 0 && row >= 0 && col < map.m_width && row < map.m_height;
}

bool tile_is_walkable(const DungeonMap& map, int col, int row)
{
    if (!is_in_bounds(map, col, row))
    {
        return false;
    }
    return !map.m_tiles[tile_index(map, col, row)].m_blocks_movement;
}

void set_tile(DungeonMap& map, int col, int row, char symbol, bool blocks)
{
    if (!is_in_bounds(map, col, row))
    {
        return;
    }

    DungeonTile& tile = map.m_tiles[tile_index(map, col, row)];
    tile.m_symbol = symbol;
    tile.m_blocks_movement = blocks;
}

struct RoomRect
{
    int m_x{0};
    int m_y{0};
    int m_w{0};
    int m_h{0};
};

bool rooms_overlap_with_margin(const RoomRect& a, const RoomRect& b, int margin)
{
    return !(a.m_x + a.m_w + margin <= b.m_x
        || b.m_x + b.m_w + margin <= a.m_x
        || a.m_y + a.m_h + margin <= b.m_y
        || b.m_y + b.m_h + margin <= a.m_y);
}

void carve_room(DungeonMap& map, const RoomRect& room)
{
    for (int row = room.m_y; row < room.m_y + room.m_h; ++row)
    {
        for (int col = room.m_x; col < room.m_x + room.m_w; ++col)
        {
            set_tile(map, col, row, '.', false);
        }
    }
}

void carve_h_corridor(DungeonMap& map, int x0, int x1, int y)
{
    const int min_x = std::min(x0, x1);
    const int max_x = std::max(x0, x1);
    for (int col = min_x; col <= max_x; ++col)
    {
        set_tile(map, col, y, '.', false);
    }
}

void carve_v_corridor(DungeonMap& map, int y0, int y1, int x)
{
    const int min_y = std::min(y0, y1);
    const int max_y = std::max(y0, y1);
    for (int row = min_y; row <= max_y; ++row)
    {
        set_tile(map, x, row, '.', false);
    }
}

std::pair<int, int> room_center(const RoomRect& room)
{
    return {
        room.m_x + (room.m_w / 2),
        room.m_y + (room.m_h / 2),
    };
}

std::pair<int, int> find_nearest_walkable(
    const DungeonMap& map,
    int preferred_col,
    int preferred_row,
    int avoid_col,
    int avoid_row)
{
    if (tile_is_walkable(map, preferred_col, preferred_row)
        && !(preferred_col == avoid_col && preferred_row == avoid_row))
    {
        return {preferred_col, preferred_row};
    }

    const int max_radius = std::max(map.m_width, map.m_height);
    for (int radius = 1; radius <= max_radius; ++radius)
    {
        const int min_col = preferred_col - radius;
        const int max_col = preferred_col + radius;
        const int min_row = preferred_row - radius;
        const int max_row = preferred_row + radius;

        for (int row = min_row; row <= max_row; ++row)
        {
            for (int col = min_col; col <= max_col; ++col)
            {
                if (col != min_col && col != max_col && row != min_row && row != max_row)
                {
                    continue;
                }
                if ((col == avoid_col && row == avoid_row) || !tile_is_walkable(map, col, row))
                {
                    continue;
                }
                return {col, row};
            }
        }
    }

    return {preferred_col, preferred_row};
}

void place_key_switch_door_constraint(
    DungeonMap& map,
    const std::vector<RoomRect>& rooms,
    std::mt19937& rng)
{
    if (rooms.size() < 2U)
    {
        return;
    }

    const auto [a_col, a_row] = room_center(rooms[0]);
    const auto [b_col, b_row] = room_center(rooms[1]);

    // Door is placed on the connecting path segment toward the second room.
    int door_col = b_col;
    int door_row = a_row;
    if (door_row == b_row)
    {
        door_col = a_col + ((b_col > a_col) ? 1 : -1);
    }
    else
    {
        door_row = a_row + ((b_row > a_row) ? 1 : -1);
    }

    if (!is_in_bounds(map, door_col, door_row))
    {
        return;
    }

    set_tile(map, door_col, door_row, 'D', true);

    const auto [key_col, key_row] = find_nearest_walkable(map, a_col, a_row, door_col, door_row);
    const auto [switch_col, switch_row] = find_nearest_walkable(map, b_col, b_row, door_col, door_row);

    set_tile(map, key_col, key_row, 'K', false);
    set_tile(map, switch_col, switch_row, 'S', false);

    std::uniform_int_distribution<uint32_t> key_id_dist(1U, 0x00FFFFFFU);
    map.m_generated_constraints.push_back(DungeonMap::DoorConstraint{
        .m_key_id = key_id_dist(rng),
        .m_door_col = door_col,
        .m_door_row = door_row,
        .m_key_col = key_col,
        .m_key_row = key_row,
        .m_switch_col = switch_col,
        .m_switch_row = switch_row,
    });
}

} // namespace

bool load_handcrafted_dungeon_map(const std::string& path, DungeonMap& out_map)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    std::vector<std::string> rows{};
    std::string line{};
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty() || line.front() == ';')
        {
            continue;
        }

        rows.push_back(line);
    }

    if (rows.empty())
    {
        return false;
    }

    const auto width_it = std::max_element(
        rows.begin(),
        rows.end(),
        [](const std::string& lhs, const std::string& rhs) {
            return lhs.size() < rhs.size();
        });
    const int width = static_cast<int>(width_it->size());
    if (width <= 0)
    {
        return false;
    }

    DungeonMap parsed{};
    parsed.m_width = width;
    parsed.m_height = static_cast<int>(rows.size());
    parsed.m_tiles.reserve(
        static_cast<std::size_t>(parsed.m_width) * static_cast<std::size_t>(parsed.m_height));

    for (int row = 0; row < parsed.m_height; ++row)
    {
        const std::string& src = rows[static_cast<std::size_t>(row)];
        for (int col = 0; col < parsed.m_width; ++col)
        {
            char symbol = '.';
            if (col < static_cast<int>(src.size()))
            {
                symbol = src[static_cast<std::size_t>(col)];
            }

            if (!is_supported_tile(symbol))
            {
                symbol = '.';
            }

            parsed.m_tiles.push_back(DungeonTile{
                .m_col = col,
                .m_row = row,
                .m_blocks_movement = tile_blocks_movement(symbol),
                .m_symbol = symbol,
            });
        }
    }

    parsed.m_generated_constraints.clear();

    out_map = std::move(parsed);
    return true;
}

bool generate_room_corridor_dungeon_map(const DungeonGenerationConfig& config, DungeonMap& out_map)
{
    if (config.m_width < 8 || config.m_height < 8
        || config.m_room_count <= 0
        || config.m_min_room_size < 3
        || config.m_max_room_size < config.m_min_room_size)
    {
        return false;
    }

    DungeonMap generated{};
    generated.m_width = config.m_width;
    generated.m_height = config.m_height;
    generated.m_tiles.reserve(
        static_cast<std::size_t>(generated.m_width) * static_cast<std::size_t>(generated.m_height));

    for (int row = 0; row < generated.m_height; ++row)
    {
        for (int col = 0; col < generated.m_width; ++col)
        {
            generated.m_tiles.push_back(DungeonTile{
                .m_col = col,
                .m_row = row,
                .m_blocks_movement = true,
                .m_symbol = '#',
            });
        }
    }

    std::mt19937 rng(config.m_seed);
    std::uniform_int_distribution<int> room_w_dist(config.m_min_room_size, config.m_max_room_size);
    std::uniform_int_distribution<int> room_h_dist(config.m_min_room_size, config.m_max_room_size);

    std::vector<RoomRect> rooms{};
    rooms.reserve(static_cast<std::size_t>(config.m_room_count));

    int attempts = 0;
    const int max_attempts = config.m_room_count * 20;
    while (static_cast<int>(rooms.size()) < config.m_room_count && attempts < max_attempts)
    {
        ++attempts;

        const int room_w = room_w_dist(rng);
        const int room_h = room_h_dist(rng);
        if (room_w >= generated.m_width - 2 || room_h >= generated.m_height - 2)
        {
            continue;
        }

        std::uniform_int_distribution<int> x_dist(1, generated.m_width - room_w - 1);
        std::uniform_int_distribution<int> y_dist(1, generated.m_height - room_h - 1);
        const RoomRect candidate{
            .m_x = x_dist(rng),
            .m_y = y_dist(rng),
            .m_w = room_w,
            .m_h = room_h,
        };

        bool overlaps = false;
        for (const RoomRect& existing : rooms)
        {
            if (rooms_overlap_with_margin(candidate, existing, 1))
            {
                overlaps = true;
                break;
            }
        }
        if (overlaps)
        {
            continue;
        }

        carve_room(generated, candidate);
        rooms.push_back(candidate);
    }

    if (rooms.empty())
    {
        return false;
    }

    // Connect room centers in insertion order with deterministic L-shaped corridors.
    std::bernoulli_distribution horiz_first_dist(0.5);
    for (std::size_t i = 1; i < rooms.size(); ++i)
    {
        const auto [x0, y0] = room_center(rooms[i - 1]);
        const auto [x1, y1] = room_center(rooms[i]);

        if (horiz_first_dist(rng))
        {
            carve_h_corridor(generated, x0, x1, y0);
            carve_v_corridor(generated, y0, y1, x1);
        }
        else
        {
            carve_v_corridor(generated, y0, y1, x0);
            carve_h_corridor(generated, x0, x1, y1);
        }
    }

    if (config.m_enable_key_switch_constraints)
    {
        place_key_switch_door_constraint(generated, rooms, rng);
    }

    out_map = std::move(generated);
    return true;
}

} // namespace mordor
