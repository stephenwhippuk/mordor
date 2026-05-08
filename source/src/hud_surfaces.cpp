#include "mordor/hud_surfaces.hpp"

#include <algorithm>

namespace mordor {

HudPanelMetrics build_hud_panel_metrics(
    const PartySelectionState& selection,
    const PartyCommandQueue& party_queue,
    const AbilityExecutionQueue& ability_queue,
    const ItemUseQueue& item_queue,
    const std::vector<InventoryItemEntry>& inventory_items,
    EntityId inventory_owner_entity_id)
{
    HudPanelMetrics metrics{};
    metrics.m_selected_actor_count = static_cast<uint16_t>(std::min<std::size_t>(selection.m_selected_actor_ids.size(), 65535U));
    metrics.m_has_primary_actor = selection.m_primary_actor_id != k_invalid_entity_id;
    metrics.m_party_queue_size = party_queue.m_intents.size();
    metrics.m_ability_queue_size = ability_queue.m_requests.size();
    metrics.m_item_queue_size = item_queue.m_intents.size();

    for (const InventoryItemEntry& entry : inventory_items)
    {
        if (entry.m_owner_entity_id != inventory_owner_entity_id)
        {
            continue;
        }

        if (entry.m_quantity == 0U)
        {
            continue;
        }

        ++metrics.m_inventory_entry_count;
        const uint32_t next_total = static_cast<uint32_t>(metrics.m_inventory_total_quantity)
            + static_cast<uint32_t>(entry.m_quantity);
        metrics.m_inventory_total_quantity = static_cast<uint16_t>(std::min<uint32_t>(next_total, 65535U));
    }

    return metrics;
}

std::vector<ScreenOverlayRect> build_baseline_hud_surfaces(
    const HudPanelMetrics& metrics,
    int framebuffer_width,
    int framebuffer_height)
{
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return {};
    }

    const int margin = 16;
    const int panel_width = std::clamp(framebuffer_width / 4, 240, 320);
    const int party_panel_height = 92;
    const int inventory_panel_height = 92;
    const int gap = 12;

    const int total_height = party_panel_height + gap + inventory_panel_height;
    const int origin_x = margin;
    const int origin_y = std::max(margin, framebuffer_height - margin - total_height);

    const auto make_panel = [](int x, int y, int w, int h, float r, float g, float b) {
        return ScreenOverlayRect{
            .m_x = x,
            .m_y = y,
            .m_width = w,
            .m_height = h,
            .m_r = r,
            .m_g = g,
            .m_b = b,
        };
    };

    std::vector<ScreenOverlayRect> rects{};
    rects.reserve(8);

    const int party_y = origin_y + inventory_panel_height + gap;
    rects.push_back(make_panel(origin_x, party_y, panel_width, party_panel_height, 0.10F, 0.13F, 0.17F));

    const int inventory_y = origin_y;
    rects.push_back(make_panel(origin_x, inventory_y, panel_width, inventory_panel_height, 0.11F, 0.10F, 0.16F));

    const int bar_margin = 10;
    const int bar_width = panel_width - (bar_margin * 2);
    const int bar_height = 12;

    const float party_intensity = std::min(1.0F, static_cast<float>(metrics.m_party_queue_size) / 8.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 10,
        static_cast<int>(static_cast<float>(bar_width) * party_intensity),
        bar_height,
        0.24F,
        0.70F,
        0.88F));

    const float ability_intensity = std::min(1.0F, static_cast<float>(metrics.m_ability_queue_size) / 8.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 30,
        static_cast<int>(static_cast<float>(bar_width) * ability_intensity),
        bar_height,
        0.93F,
        0.67F,
        0.25F));

    const float selection_intensity = std::min(1.0F, static_cast<float>(metrics.m_selected_actor_count) / 4.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 50,
        static_cast<int>(static_cast<float>(bar_width) * selection_intensity),
        bar_height,
        metrics.m_has_primary_actor ? 0.34F : 0.22F,
        metrics.m_has_primary_actor ? 0.86F : 0.27F,
        0.39F));

    const float item_queue_intensity = std::min(1.0F, static_cast<float>(metrics.m_item_queue_size) / 8.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 10,
        static_cast<int>(static_cast<float>(bar_width) * item_queue_intensity),
        bar_height,
        0.89F,
        0.45F,
        0.31F));

    const float item_quantity_intensity =
        std::min(1.0F, static_cast<float>(metrics.m_inventory_total_quantity) / 12.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 30,
        static_cast<int>(static_cast<float>(bar_width) * item_quantity_intensity),
        bar_height,
        0.95F,
        0.82F,
        0.36F));

    const float entry_count_intensity =
        std::min(1.0F, static_cast<float>(metrics.m_inventory_entry_count) / 6.0F);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 50,
        static_cast<int>(static_cast<float>(bar_width) * entry_count_intensity),
        bar_height,
        0.60F,
        0.70F,
        0.94F));

    rects.erase(
        std::remove_if(
            rects.begin(),
            rects.end(),
            [](const ScreenOverlayRect& rect) {
                return rect.m_width <= 0 || rect.m_height <= 0;
            }),
        rects.end());

    return rects;
}

} // namespace mordor