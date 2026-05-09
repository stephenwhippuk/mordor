#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mordor {

struct DungeonTile
{
    int m_col{0};
    int m_row{0};
    bool m_blocks_movement{false};
    char m_symbol{'.'};
};

struct DungeonMap
{
    int m_width{0};
    int m_height{0};
    std::vector<DungeonTile> m_tiles{};
    struct DoorConstraint
    {
        uint32_t m_key_id{0U};
        int m_door_col{0};
        int m_door_row{0};
        int m_key_col{0};
        int m_key_row{0};
        int m_switch_col{0};
        int m_switch_row{0};
    };
    std::vector<DoorConstraint> m_generated_constraints{};

    struct PrefabPlacement
    {
        uint32_t m_prefab_id{0U};
        int m_origin_col{0};
        int m_origin_row{0};
        int m_width{0};
        int m_height{0};
    };
    std::vector<PrefabPlacement> m_prefab_placements{};
};

struct DungeonGenerationConfig
{
    int m_width{64};
    int m_height{64};
    int m_room_count{10};
    int m_min_room_size{4};
    int m_max_room_size{10};
    uint32_t m_seed{1337U};
    bool m_enable_key_switch_constraints{true};
    bool m_enable_prefab_insertion{true};
    int m_prefab_attempt_count{2};
};

struct DungeonValidationReport
{
    bool m_is_valid{false};
    bool m_is_solvable_with_unlocks{false};
    bool m_constraints_valid{false};
    int m_walkable_tile_count{0};
    int m_reachable_without_unlocks{0};
    int m_reachable_with_unlocks{0};
    std::vector<std::string> m_issues{};
};

bool load_handcrafted_dungeon_map(const std::string& path, DungeonMap& out_map);
bool generate_room_corridor_dungeon_map(const DungeonGenerationConfig& config, DungeonMap& out_map);
bool validate_generated_dungeon_map(const DungeonMap& map, DungeonValidationReport& out_report);

} // namespace mordor
