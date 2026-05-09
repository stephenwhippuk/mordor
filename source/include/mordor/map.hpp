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
};

struct DungeonGenerationConfig
{
    int m_width{64};
    int m_height{64};
    int m_room_count{10};
    int m_min_room_size{4};
    int m_max_room_size{10};
    uint32_t m_seed{1337U};
};

bool load_handcrafted_dungeon_map(const std::string& path, DungeonMap& out_map);
bool generate_room_corridor_dungeon_map(const DungeonGenerationConfig& config, DungeonMap& out_map);

} // namespace mordor
