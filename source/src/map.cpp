#include "mordor/map.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mordor {

namespace {

bool is_supported_tile(char symbol)
{
    return symbol == '#' || symbol == '.' || symbol == 'D' || symbol == 'W';
}

DungeonMap::EntityKind entity_kind_for_token(const std::string& token)
{
    if (token == "key")
    {
        return DungeonMap::EntityKind::Key;
    }
    if (token == "switch")
    {
        return DungeonMap::EntityKind::Switch;
    }
    if (token == "prop")
    {
        return DungeonMap::EntityKind::Prop;
    }
    if (token == "item")
    {
        return DungeonMap::EntityKind::Item;
    }
    if (token == "npc")
    {
        return DungeonMap::EntityKind::Npc;
    }
    if (token == "spawn")
    {
        return DungeonMap::EntityKind::Spawn;
    }

    return DungeonMap::EntityKind::Unknown;
}

bool parse_bool_token(const std::string& token, bool& out_value)
{
    if (token == "1" || token == "true")
    {
        out_value = true;
        return true;
    }

    if (token == "0" || token == "false")
    {
        out_value = false;
        return true;
    }

    return false;
}

bool parse_entity_record_line(
    const std::string& line,
    DungeonMap::EntityPlacement& out_entity)
{
    std::istringstream record(line);
    std::string marker{};
    std::string kind_token{};
    int col = 0;
    int row = 0;
    std::string symbol_token{};
    std::string solid_token{};
    std::string movable_token{};
    if (!(record >> marker >> kind_token >> col >> row >> symbol_token >> solid_token >> movable_token))
    {
        return false;
    }

    if (marker != "@entity" || symbol_token.size() != 1)
    {
        return false;
    }

    const DungeonMap::EntityKind kind = entity_kind_for_token(kind_token);
    if (kind == DungeonMap::EntityKind::Unknown)
    {
        return false;
    }

    bool is_solid = false;
    bool is_movable = false;
    if (!parse_bool_token(solid_token, is_solid) || !parse_bool_token(movable_token, is_movable))
    {
        return false;
    }

    out_entity = DungeonMap::EntityPlacement{
        .m_kind = kind,
        .m_col = col,
        .m_row = row,
        .m_debug_symbol = symbol_token[0],
        .m_collision_mask = is_solid ? k_tile_collision_solid : k_tile_collision_none,
        .m_movable = is_movable,
    };
    return true;
}

bool tile_blocks_physical_symbol(char symbol)
{
    return symbol == '#' || symbol == 'D';
}

uint32_t tile_collision_mask_for_symbol(char symbol)
{
    return tile_blocks_physical_symbol(symbol) ? k_tile_collision_solid : k_tile_collision_none;
}

bool tile_blocks_visual_symbol(char symbol)
{
    return symbol == '#' || symbol == 'D' || symbol == 'W';
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
    return !dungeon_tile_blocks_physical(map.m_tiles[tile_index(map, col, row)]);
}

void set_tile(DungeonMap& map, int col, int row, char symbol, bool blocks)
{
    if (!is_in_bounds(map, col, row))
    {
        return;
    }

    DungeonTile& tile = map.m_tiles[tile_index(map, col, row)];
    tile.m_symbol = symbol;
    tile.m_collision_mask = blocks ? k_tile_collision_solid : k_tile_collision_none;
}

void add_entity_placement(
    DungeonMap& map,
    DungeonMap::EntityKind kind,
    int col,
    int row,
    char debug_symbol,
    uint32_t collision_mask,
    bool movable)
{
    if (!is_in_bounds(map, col, row))
    {
        return;
    }

    map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = kind,
        .m_col = col,
        .m_row = row,
        .m_debug_symbol = debug_symbol,
        .m_collision_mask = collision_mask,
        .m_movable = movable,
    });
}

bool has_entity_placement_at(
    const DungeonMap& map,
    DungeonMap::EntityKind kind,
    int col,
    int row)
{
    return std::any_of(
        map.m_entity_placements.begin(),
        map.m_entity_placements.end(),
        [kind, col, row](const DungeonMap::EntityPlacement& entity) {
            return entity.m_kind == kind && entity.m_col == col && entity.m_row == row;
        });
}

struct RoomRect
{
    int m_x{0};
    int m_y{0};
    int m_w{0};
    int m_h{0};
};

struct PrefabTemplate
{
    uint32_t m_id{0U};
    std::vector<std::string> m_rows{};
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

const std::vector<PrefabTemplate>& builtin_prefabs()
{
    static const std::vector<PrefabTemplate> prefabs{
        PrefabTemplate{
            .m_id = 1U,
            .m_rows = {
                ".....",
                ".PPP.",
                ".P.P.",
                ".PPP.",
                ".....",
            },
        },
        PrefabTemplate{
            .m_id = 2U,
            .m_rows = {
                ".......",
                "..PPP..",
                "..P.P..",
                "PPP.PPP",
                "..P.P..",
                "..PPP..",
                ".......",
            },
        },
    };
    return prefabs;
}

bool can_place_prefab(const DungeonMap& map, const PrefabTemplate& prefab, int origin_col, int origin_row)
{
    const int prefab_h = static_cast<int>(prefab.m_rows.size());
    const int prefab_w = prefab_h > 0 ? static_cast<int>(prefab.m_rows[0].size()) : 0;
    if (prefab_w <= 0 || prefab_h <= 0)
    {
        return false;
    }

    for (int r = 0; r < prefab_h; ++r)
    {
        for (int c = 0; c < prefab_w; ++c)
        {
            const int col = origin_col + c;
            const int row = origin_row + r;
            if (!is_in_bounds(map, col, row))
            {
                return false;
            }

            const DungeonTile& tile = map.m_tiles[tile_index(map, col, row)];
            // Only place on existing walkable carved space and never overwrite constraint markers.
            if (dungeon_tile_blocks_physical(tile)
                || tile.m_symbol == 'D'
                || tile.m_symbol == 'K'
                || tile.m_symbol == 'S')
            {
                return false;
            }

            if (std::any_of(
                    map.m_entity_placements.begin(),
                    map.m_entity_placements.end(),
                    [col, row](const DungeonMap::EntityPlacement& entity) {
                        return entity.m_col == col && entity.m_row == row;
                    }))
            {
                return false;
            }
        }
    }

    return true;
}

void apply_prefab(DungeonMap& map, const PrefabTemplate& prefab, int origin_col, int origin_row)
{
    const int prefab_h = static_cast<int>(prefab.m_rows.size());
    const int prefab_w = prefab_h > 0 ? static_cast<int>(prefab.m_rows[0].size()) : 0;

    for (int r = 0; r < prefab_h; ++r)
    {
        for (int c = 0; c < prefab_w; ++c)
        {
            const char sym = prefab.m_rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
            if (sym == 'P')
            {
                set_tile(map, origin_col + c, origin_row + r, '.', false);
                add_entity_placement(
                    map,
                    DungeonMap::EntityKind::Prop,
                    origin_col + c,
                    origin_row + r,
                    'P',
                    k_tile_collision_none,
                    false);
            }
            else
            {
                set_tile(map, origin_col + c, origin_row + r, '.', false);
            }
        }
    }

    map.m_prefab_placements.push_back(DungeonMap::PrefabPlacement{
        .m_prefab_id = prefab.m_id,
        .m_origin_col = origin_col,
        .m_origin_row = origin_row,
        .m_width = prefab_w,
        .m_height = prefab_h,
    });
}

void place_prefabs(
    DungeonMap& map,
    const std::vector<RoomRect>& rooms,
    std::mt19937& rng,
    const DungeonGenerationConfig& config)
{
    if (!config.m_enable_prefab_insertion || config.m_prefab_attempt_count <= 0 || rooms.empty())
    {
        return;
    }

    const std::vector<PrefabTemplate>& prefabs = builtin_prefabs();
    if (prefabs.empty())
    {
        return;
    }

    std::uniform_int_distribution<int> prefab_dist(0, static_cast<int>(prefabs.size() - 1));

    int placed = 0;
    // Prefer later rooms to avoid interfering with early key/switch/door constraints.
    for (std::size_t room_idx = std::min<std::size_t>(2, rooms.size()); room_idx < rooms.size(); ++room_idx)
    {
        if (placed >= config.m_prefab_attempt_count)
        {
            break;
        }

        const RoomRect& room = rooms[room_idx];
        const PrefabTemplate& prefab = prefabs[static_cast<std::size_t>(prefab_dist(rng))];
        const int prefab_h = static_cast<int>(prefab.m_rows.size());
        const int prefab_w = prefab_h > 0 ? static_cast<int>(prefab.m_rows[0].size()) : 0;
        if (prefab_w <= 0 || prefab_h <= 0 || room.m_w < prefab_w || room.m_h < prefab_h)
        {
            continue;
        }

        const int origin_col = room.m_x + (room.m_w - prefab_w) / 2;
        const int origin_row = room.m_y + (room.m_h - prefab_h) / 2;
        if (!can_place_prefab(map, prefab, origin_col, origin_row))
        {
            continue;
        }

        apply_prefab(map, prefab, origin_col, origin_row);
        ++placed;
    }
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

    set_tile(map, key_col, key_row, '.', false);
    set_tile(map, switch_col, switch_row, '.', false);
    add_entity_placement(
        map,
        DungeonMap::EntityKind::Key,
        key_col,
        key_row,
        'K',
        k_tile_collision_none,
        false);
    add_entity_placement(
        map,
        DungeonMap::EntityKind::Switch,
        switch_col,
        switch_row,
        'S',
        k_tile_collision_none,
        false);

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

struct FloodFillResult
{
    std::vector<uint8_t> m_visited{};
    int m_reachable_walkable{0};
};

bool is_tile_passable_for_validation(const DungeonTile& tile, bool treat_doors_as_open)
{
    if (!dungeon_tile_blocks_physical(tile))
    {
        return true;
    }
    return treat_doors_as_open && tile.m_symbol == 'D';
}

int find_first_walkable_tile_index(const DungeonMap& map)
{
    for (int i = 0; i < static_cast<int>(map.m_tiles.size()); ++i)
    {
        if (!dungeon_tile_blocks_physical(map.m_tiles[static_cast<std::size_t>(i)]))
        {
            return i;
        }
    }
    return -1;
}

FloodFillResult flood_fill_from_start(const DungeonMap& map, int start_index, bool treat_doors_as_open)
{
    FloodFillResult result{};
    result.m_visited.assign(map.m_tiles.size(), 0U);

    if (start_index < 0 || start_index >= static_cast<int>(map.m_tiles.size()))
    {
        return result;
    }

    std::queue<int> open{};
    result.m_visited[static_cast<std::size_t>(start_index)] = 1U;
    open.push(start_index);

    while (!open.empty())
    {
        const int idx = open.front();
        open.pop();

        const DungeonTile& tile = map.m_tiles[static_cast<std::size_t>(idx)];
        if (!dungeon_tile_blocks_physical(tile))
        {
            ++result.m_reachable_walkable;
        }

        const int col = tile.m_col;
        const int row = tile.m_row;
        const int neighbor_cols[4] = {col + 1, col - 1, col, col};
        const int neighbor_rows[4] = {row, row, row + 1, row - 1};

        for (int n = 0; n < 4; ++n)
        {
            const int nc = neighbor_cols[n];
            const int nr = neighbor_rows[n];
            if (!is_in_bounds(map, nc, nr))
            {
                continue;
            }

            const int nidx = nr * map.m_width + nc;
            const std::size_t nidx_u = static_cast<std::size_t>(nidx);
            if (result.m_visited[nidx_u] != 0U)
            {
                continue;
            }

            const DungeonTile& neighbor = map.m_tiles[nidx_u];
            if (!is_tile_passable_for_validation(neighbor, treat_doors_as_open))
            {
                continue;
            }

            result.m_visited[nidx_u] = 1U;
            open.push(nidx);
        }
    }

    return result;
}

void add_issue(DungeonValidationReport& report, const std::string& message)
{
    report.m_issues.push_back(message);
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
    std::vector<std::string> entity_records{};
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

        if (line.rfind("@entity", 0) == 0)
        {
            entity_records.push_back(line);
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
    parsed.m_entity_placements.clear();

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
                return false;
            }

            parsed.m_tiles.push_back(DungeonTile{
                .m_col = col,
                .m_row = row,
                .m_collision_mask = tile_collision_mask_for_symbol(symbol),
                .m_symbol = symbol,
            });
        }
    }

    for (const std::string& record : entity_records)
    {
        DungeonMap::EntityPlacement entity{};
        if (!parse_entity_record_line(record, entity) || !is_in_bounds(parsed, entity.m_col, entity.m_row))
        {
            return false;
        }

        if (has_entity_placement_at(parsed, entity.m_kind, entity.m_col, entity.m_row))
        {
            return false;
        }

        const DungeonTile& tile = parsed.m_tiles[tile_index(parsed, entity.m_col, entity.m_row)];
        if (dungeon_tile_blocks_physical(tile))
        {
            return false;
        }

        parsed.m_entity_placements.push_back(entity);
    }

    parsed.m_generated_constraints.clear();
    parsed.m_prefab_placements.clear();

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
    generated.m_entity_placements.clear();

    for (int row = 0; row < generated.m_height; ++row)
    {
        for (int col = 0; col < generated.m_width; ++col)
        {
            generated.m_tiles.push_back(DungeonTile{
                .m_col = col,
                .m_row = row,
                .m_collision_mask = k_tile_collision_solid,
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

    place_prefabs(generated, rooms, rng, config);

    out_map = std::move(generated);
    return true;
}

bool validate_generated_dungeon_map(const DungeonMap& map, DungeonValidationReport& out_report)
{
    DungeonValidationReport report{};

    if (map.m_width <= 0 || map.m_height <= 0)
    {
        add_issue(report, "map dimensions must be positive");
        out_report = std::move(report);
        return false;
    }

    const std::size_t expected_size =
        static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height);
    if (map.m_tiles.size() != expected_size)
    {
        add_issue(report, "tile array size does not match map dimensions");
        out_report = std::move(report);
        return false;
    }

    int walkable_count = 0;
    for (const DungeonTile& tile : map.m_tiles)
    {
        if (!dungeon_tile_blocks_physical(tile))
        {
            ++walkable_count;
        }
    }
    report.m_walkable_tile_count = walkable_count;

    if (walkable_count == 0)
    {
        add_issue(report, "map contains no walkable tiles");
        out_report = std::move(report);
        return false;
    }

    const int start_idx = find_first_walkable_tile_index(map);
    const FloodFillResult strict_fill = flood_fill_from_start(map, start_idx, false);
    const FloodFillResult unlocked_fill = flood_fill_from_start(map, start_idx, true);

    report.m_reachable_without_unlocks = strict_fill.m_reachable_walkable;
    report.m_reachable_with_unlocks = unlocked_fill.m_reachable_walkable;
    report.m_is_solvable_with_unlocks = (unlocked_fill.m_reachable_walkable == walkable_count);

    if (!report.m_is_solvable_with_unlocks)
    {
        add_issue(report, "walkable tiles are disconnected even when doors are treated as open");
    }

    bool constraints_valid = true;
    for (const DungeonMap::DoorConstraint& c : map.m_generated_constraints)
    {
        if (!is_in_bounds(map, c.m_door_col, c.m_door_row)
            || !is_in_bounds(map, c.m_key_col, c.m_key_row)
            || !is_in_bounds(map, c.m_switch_col, c.m_switch_row))
        {
            constraints_valid = false;
            add_issue(report, "constraint coordinates are out of bounds");
            continue;
        }

        const DungeonTile& door_tile = map.m_tiles[tile_index(map, c.m_door_col, c.m_door_row)];
        const DungeonTile& key_tile = map.m_tiles[tile_index(map, c.m_key_col, c.m_key_row)];
        const DungeonTile& switch_tile = map.m_tiles[tile_index(map, c.m_switch_col, c.m_switch_row)];

        if (c.m_key_id == 0U)
        {
            constraints_valid = false;
            add_issue(report, "constraint key id must be non-zero");
        }
        if (door_tile.m_symbol != 'D' || !dungeon_tile_blocks_physical(door_tile))
        {
            constraints_valid = false;
            add_issue(report, "constraint door tile is not a locked door marker");
        }
        if (!has_entity_placement_at(map, DungeonMap::EntityKind::Key, c.m_key_col, c.m_key_row)
            || dungeon_tile_blocks_physical(key_tile))
        {
            constraints_valid = false;
            add_issue(report, "constraint key tile is invalid");
        }
        if (!has_entity_placement_at(map, DungeonMap::EntityKind::Switch, c.m_switch_col, c.m_switch_row)
            || dungeon_tile_blocks_physical(switch_tile))
        {
            constraints_valid = false;
            add_issue(report, "constraint switch tile is invalid");
        }

        const int key_idx = c.m_key_row * map.m_width + c.m_key_col;
        if (key_idx >= 0 && key_idx < static_cast<int>(strict_fill.m_visited.size())
            && strict_fill.m_visited[static_cast<std::size_t>(key_idx)] == 0U)
        {
            constraints_valid = false;
            add_issue(report, "constraint key is not reachable before unlocks");
        }
    }

    report.m_constraints_valid = constraints_valid;
    report.m_is_valid = report.m_is_solvable_with_unlocks && constraints_valid && report.m_issues.empty();
    out_report = std::move(report);
    return out_report.m_is_valid;
}

bool dungeon_tile_has_collision_bits(const DungeonTile& tile, uint32_t bits)
{
    return (tile.m_collision_mask & bits) == bits;
}

bool dungeon_tile_blocks_physical(const DungeonTile& tile)
{
    return dungeon_tile_has_collision_bits(tile, k_tile_collision_solid);
}

bool dungeon_tile_blocks_visual(const DungeonTile& tile)
{
    return tile_blocks_visual_symbol(tile.m_symbol);
}

bool dungeon_entity_has_collision_bits(const DungeonMap::EntityPlacement& entity, uint32_t bits)
{
    return (entity.m_collision_mask & bits) == bits;
}

bool dungeon_entity_blocks_physical(const DungeonMap::EntityPlacement& entity)
{
    return dungeon_entity_has_collision_bits(entity, k_tile_collision_solid);
}

} // namespace mordor
