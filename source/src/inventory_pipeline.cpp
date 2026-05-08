#include "mordor/inventory_pipeline.hpp"

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

const ActorComponent* find_actor(const RuntimeComponents& components, EntityId entity_id)
{
    auto it = std::find_if(
        components.m_actors.begin(),
        components.m_actors.end(),
        [entity_id](const ActorComponent& actor) {
            return actor.m_entity_id == entity_id;
        });
    return it != components.m_actors.end() ? &(*it) : nullptr;
}

const InventoryComponent* find_inventory(const RuntimeComponents& components, EntityId entity_id)
{
    auto it = std::find_if(
        components.m_inventories.begin(),
        components.m_inventories.end(),
        [entity_id](const InventoryComponent& inventory) {
            return inventory.m_entity_id == entity_id;
        });
    return it != components.m_inventories.end() ? &(*it) : nullptr;
}

const TransformComponent* find_transform(const RuntimeComponents& components, EntityId entity_id)
{
    auto it = std::find_if(
        components.m_transforms.begin(),
        components.m_transforms.end(),
        [entity_id](const TransformComponent& transform) {
            return transform.m_entity_id == entity_id;
        });
    return it != components.m_transforms.end() ? &(*it) : nullptr;
}

const ItemSpec* find_item_spec(const std::vector<ItemSpec>& specs, uint32_t item_id)
{
    auto it = std::find_if(
        specs.begin(),
        specs.end(),
        [item_id](const ItemSpec& spec) {
            return spec.m_item_id == item_id;
        });
    return it != specs.end() ? &(*it) : nullptr;
}

const InventoryItemEntry* find_item_entry(
    const std::vector<InventoryItemEntry>& inventory_items,
    EntityId owner_entity_id,
    uint32_t item_id)
{
    auto it = std::find_if(
        inventory_items.begin(),
        inventory_items.end(),
        [owner_entity_id, item_id](const InventoryItemEntry& entry) {
            return entry.m_owner_entity_id == owner_entity_id && entry.m_item_id == item_id;
        });
    return it != inventory_items.end() ? &(*it) : nullptr;
}

bool entity_exists(const RuntimeComponents& components, EntityId entity_id)
{
    if (entity_id == k_invalid_entity_id)
    {
        return false;
    }

    return std::find(components.m_entities.begin(), components.m_entities.end(), entity_id)
        != components.m_entities.end();
}

TileCoord transform_to_tile(const TransformComponent& transform)
{
    return TileCoord{
        .m_col = static_cast<int>(std::floor(transform.m_local_offset.m_x / k_scene_tile_world_size)),
        .m_row = static_cast<int>(std::floor(transform.m_local_offset.m_y / k_scene_tile_world_size)),
    };
}

int tile_distance(TileCoord from, TileCoord to)
{
    const int delta_col = std::abs(to.m_col - from.m_col);
    const int delta_row = std::abs(to.m_row - from.m_row);
    return std::max(delta_col, delta_row);
}

} // namespace

bool has_inventory_item(
    const std::vector<InventoryItemEntry>& inventory_items,
    EntityId owner_entity_id,
    uint32_t item_id,
    uint16_t minimum_quantity)
{
    auto it = std::find_if(
        inventory_items.begin(),
        inventory_items.end(),
        [owner_entity_id, item_id](const InventoryItemEntry& entry) {
            return entry.m_owner_entity_id == owner_entity_id && entry.m_item_id == item_id;
        });

    if (it == inventory_items.end())
    {
        return false;
    }

    return it->m_quantity >= minimum_quantity;
}

void clear_item_use_queue(ItemUseQueue& queue)
{
    queue.m_intents.clear();
}

ItemUseIssueResult issue_item_use_request(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const std::vector<ItemSpec>& item_specs,
    const std::vector<InventoryItemEntry>& inventory_items,
    const ItemUseRequest& request,
    ItemUseQueue& queue)
{
    ItemUseIssueResult result{};
    result.m_queue_size = queue.m_intents.size();

    const ItemSpec* spec = find_item_spec(item_specs, request.m_item_id);
    if (spec == nullptr)
    {
        result.m_status = ItemUseIssueStatus::RejectedUnknownItem;
        return result;
    }

    result.m_action_point_cost = spec->m_action_point_cost;

    const ActorComponent* issuer = find_actor(components, request.m_issuer_entity_id);
    if (issuer == nullptr)
    {
        result.m_status = ItemUseIssueStatus::RejectedInvalidIssuer;
        return result;
    }

    const InventoryComponent* inventory = find_inventory(components, request.m_issuer_entity_id);
    if (inventory == nullptr)
    {
        result.m_status = ItemUseIssueStatus::RejectedIssuerHasNoInventory;
        return result;
    }

    const InventoryItemEntry* item_entry =
        find_item_entry(inventory_items, request.m_issuer_entity_id, request.m_item_id);
    if (item_entry == nullptr)
    {
        result.m_status = ItemUseIssueStatus::RejectedItemNotInInventory;
        return result;
    }

    if (item_entry->m_quantity == 0U)
    {
        result.m_status = ItemUseIssueStatus::RejectedInsufficientItemQuantity;
        return result;
    }

    if (issuer->m_current_action_points < spec->m_action_point_cost)
    {
        result.m_status = ItemUseIssueStatus::RejectedInsufficientActionPoints;
        return result;
    }

    if (queue.m_intents.size() >= queue.m_max_size)
    {
        result.m_status = ItemUseIssueStatus::RejectedQueueFull;
        return result;
    }

    const TransformComponent* issuer_transform = find_transform(components, request.m_issuer_entity_id);
    if (issuer_transform == nullptr)
    {
        result.m_status = ItemUseIssueStatus::RejectedInvalidIssuer;
        return result;
    }

    const TileCoord issuer_tile = transform_to_tile(*issuer_transform);

    if (spec->m_target_kind == ItemTargetKind::None)
    {
        if (request.m_has_target_entity || request.m_has_target_tile)
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }
    }
    else if (spec->m_target_kind == ItemTargetKind::Entity)
    {
        if (!request.m_has_target_entity || request.m_has_target_tile)
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (!entity_exists(components, request.m_target_entity_id))
        {
            result.m_status = ItemUseIssueStatus::RejectedTargetEntityNotFound;
            return result;
        }

        const TransformComponent* target_transform = find_transform(components, request.m_target_entity_id);
        if (target_transform == nullptr)
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }

        const TileCoord target_tile = transform_to_tile(*target_transform);
        if (tile_distance(issuer_tile, target_tile) > static_cast<int>(spec->m_max_range_tiles))
        {
            result.m_status = ItemUseIssueStatus::RejectedOutOfRange;
            return result;
        }
    }
    else if (spec->m_target_kind == ItemTargetKind::Tile)
    {
        if (!request.m_has_target_tile || request.m_has_target_entity)
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (!in_occupancy_bounds(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (tile_distance(issuer_tile, request.m_target_tile) > static_cast<int>(spec->m_max_range_tiles))
        {
            result.m_status = ItemUseIssueStatus::RejectedOutOfRange;
            return result;
        }

        if (is_tile_blocked(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
        {
            result.m_status = ItemUseIssueStatus::RejectedBlockedTileTarget;
            return result;
        }

        if (!spec->m_allow_empty_tile_target
            && !is_tile_occupied(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
        {
            result.m_status = ItemUseIssueStatus::RejectedInvalidTarget;
            return result;
        }
    }

    queue.m_intents.push_back(ItemUseIntent{
        .m_issuer_entity_id = request.m_issuer_entity_id,
        .m_item_id = request.m_item_id,
        .m_has_target_entity = request.m_has_target_entity,
        .m_has_target_tile = request.m_has_target_tile,
        .m_target_entity_id = request.m_target_entity_id,
        .m_target_tile = request.m_target_tile,
        .m_consumes_on_use = spec->m_consumes_on_use,
    });
    result.m_status = ItemUseIssueStatus::Accepted;
    result.m_queue_size = queue.m_intents.size();
    return result;
}

} // namespace mordor