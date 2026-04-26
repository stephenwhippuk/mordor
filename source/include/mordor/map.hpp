#pragma once

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

bool load_handcrafted_dungeon_map(const std::string& path, DungeonMap& out_map);

} // namespace mordor
