#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mordor {

constexpr uint32_t k_tile_collision_none = 0U;
constexpr uint32_t k_tile_collision_solid = 1U << 0;

struct DungeonTile
{
    int m_col{0};
    int m_row{0};
    uint32_t m_collision_mask{k_tile_collision_none};
    char m_symbol{'.'};
};

struct DungeonMap
{
    enum class EntityKind : uint8_t
    {
        Unknown = 0,
        Key,
        Switch,
        Prop,
        Item,
        Npc,
        Spawn,
    };

    struct EntityPlacement
    {
        EntityKind m_kind{EntityKind::Unknown};
        int m_col{0};
        int m_row{0};
        char m_debug_symbol{'?'};
        uint32_t m_collision_mask{k_tile_collision_none};
        bool m_movable{false};
    };

    int m_width{0};
    int m_height{0};
    std::vector<DungeonTile> m_tiles{};
    std::vector<EntityPlacement> m_entity_placements{};
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
bool dungeon_tile_has_collision_bits(const DungeonTile& tile, uint32_t bits);
bool dungeon_tile_blocks_physical(const DungeonTile& tile);
bool dungeon_tile_blocks_visual(const DungeonTile& tile);
bool dungeon_entity_has_collision_bits(const DungeonMap::EntityPlacement& entity, uint32_t bits);
bool dungeon_entity_blocks_physical(const DungeonMap::EntityPlacement& entity);

} // namespace mordor
