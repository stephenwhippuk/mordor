#pragma once

#include "mordor/ability_pipeline.hpp"
#include "mordor/inventory_pipeline.hpp"
#include "mordor/party_commands.hpp"
#include "mordor/renderer.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

struct HudPanelMetrics
{
    uint16_t m_selected_actor_count{0U};
    bool m_has_primary_actor{false};
    std::size_t m_party_queue_size{0U};
    std::size_t m_ability_queue_size{0U};
    std::size_t m_item_queue_size{0U};
    uint16_t m_inventory_entry_count{0U};
    uint16_t m_inventory_total_quantity{0U};
};

HudPanelMetrics build_hud_panel_metrics(
    const PartySelectionState& selection,
    const PartyCommandQueue& party_queue,
    const AbilityExecutionQueue& ability_queue,
    const ItemUseQueue& item_queue,
    const std::vector<InventoryItemEntry>& inventory_items,
    EntityId inventory_owner_entity_id);

std::vector<ScreenOverlayRect> build_baseline_hud_surfaces(
    const HudPanelMetrics& metrics,
    int framebuffer_width,
    int framebuffer_height);

} // namespace mordor