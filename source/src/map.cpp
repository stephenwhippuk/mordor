#include "mordor/map.hpp"
#include "mordor/log.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace mordor {

namespace {

bool is_supported_tile(char symbol)
{
    return symbol == '#' || symbol == '.' || symbol == 'D';
}

bool tile_blocks_movement(char symbol)
{
    return symbol == '#' || symbol == 'D';
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
                MORDOR_LOG_WARN(
                    "Unsupported tile symbol '{}' at row {} col {}; treating as floor",
                    symbol,
                    row,
                    col);
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

    out_map = std::move(parsed);
    return true;
}

} // namespace mordor
