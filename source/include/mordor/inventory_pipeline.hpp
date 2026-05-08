#pragma once

#include "mordor/components.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/visibility.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mordor {

enum class ItemTargetKind : uint8_t
{
    None = 0U,
    Entity = 1U,
    Tile = 2U,
};

struct InventoryItemEntry
{
    EntityId m_owner_entity_id{k_invalid_entity_id};
    uint32_t m_item_id{0U};
    uint16_t m_quantity{0U};
};

struct ItemSpec
{
    uint32_t m_item_id{0U};
    ItemTargetKind m_target_kind{ItemTargetKind::None};
    uint16_t m_action_point_cost{0U};
    uint16_t m_max_range_tiles{0U};
    bool m_allow_empty_tile_target{false};
    bool m_consumes_on_use{false};
};

struct ItemUseRequest
{
    EntityId m_issuer_entity_id{k_invalid_entity_id};
    uint32_t m_item_id{0U};
    bool m_has_target_entity{false};
    bool m_has_target_tile{false};
    EntityId m_target_entity_id{k_invalid_entity_id};
    TileCoord m_target_tile{};
};

enum class ItemUseIssueStatus : uint8_t
{
    Accepted = 0U,
    RejectedUnknownItem = 1U,
    RejectedInvalidIssuer = 2U,
    RejectedIssuerHasNoInventory = 3U,
    RejectedItemNotInInventory = 4U,
    RejectedInsufficientItemQuantity = 5U,
    RejectedInsufficientActionPoints = 6U,
    RejectedInvalidTarget = 7U,
    RejectedTargetEntityNotFound = 8U,
    RejectedOutOfRange = 9U,
    RejectedBlockedTileTarget = 10U,
    RejectedQueueFull = 11U,
};

struct ItemUseIssueResult
{
    ItemUseIssueStatus m_status{ItemUseIssueStatus::RejectedUnknownItem};
    uint16_t m_action_point_cost{0U};
    std::size_t m_queue_size{0U};
};

struct ItemUseIntent
{
    EntityId m_issuer_entity_id{k_invalid_entity_id};
    uint32_t m_item_id{0U};
    bool m_has_target_entity{false};
    bool m_has_target_tile{false};
    EntityId m_target_entity_id{k_invalid_entity_id};
    TileCoord m_target_tile{};
    bool m_consumes_on_use{false};
};

struct ItemUseQueue
{
    std::vector<ItemUseIntent> m_intents{};
    std::size_t m_max_size{64U};
};

bool has_inventory_item(
    const std::vector<InventoryItemEntry>& inventory_items,
    EntityId owner_entity_id,
    uint32_t item_id,
    uint16_t minimum_quantity);

void clear_item_use_queue(ItemUseQueue& queue);

ItemUseIssueResult issue_item_use_request(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const std::vector<ItemSpec>& item_specs,
    const std::vector<InventoryItemEntry>& inventory_items,
    const ItemUseRequest& request,
    ItemUseQueue& queue);

} // namespace mordor