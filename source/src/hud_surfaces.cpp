#include "mordor/hud_surfaces.hpp"

#include <algorithm>
#include <array>

namespace mordor {

namespace {

void append_rect(std::vector<ScreenOverlayRect>& rects, int x, int y, int w, int h, float r, float g, float b)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    rects.push_back(ScreenOverlayRect{
        .m_x = x,
        .m_y = y,
        .m_width = w,
        .m_height = h,
        .m_r = r,
        .m_g = g,
        .m_b = b,
    });
}

void append_digit(
    std::vector<ScreenOverlayRect>& rects,
    int x,
    int y,
    int digit,
    int scale,
    float r,
    float g,
    float b)
{
    if (digit < 0 || digit > 9)
    {
        return;
    }

    // Seven-segment mask bits: top, upper-left, upper-right, middle, lower-left, lower-right, bottom.
    static constexpr std::array<uint8_t, 10> k_digit_masks{
        0b1110111, // 0
        0b0010010, // 1
        0b1011101, // 2
        0b1011011, // 3
        0b0111010, // 4
        0b1101011, // 5
        0b1101111, // 6
        0b1010010, // 7
        0b1111111, // 8
        0b1111011, // 9
    };

    const uint8_t mask = k_digit_masks[static_cast<std::size_t>(digit)];

    const int thickness = std::max(1, scale);
    const int segment = std::max(3, 4 * scale);

    if ((mask & 0b1000000U) != 0U) // top
    {
        append_rect(rects, x + thickness, y + (segment * 2) + (thickness * 2), segment, thickness, r, g, b);
    }
    if ((mask & 0b0100000U) != 0U) // upper-left
    {
        append_rect(rects, x, y + segment + thickness, thickness, segment, r, g, b);
    }
    if ((mask & 0b0010000U) != 0U) // upper-right
    {
        append_rect(rects, x + segment + thickness, y + segment + thickness, thickness, segment, r, g, b);
    }
    if ((mask & 0b0001000U) != 0U) // middle
    {
        append_rect(rects, x + thickness, y + segment + thickness, segment, thickness, r, g, b);
    }
    if ((mask & 0b0000100U) != 0U) // lower-left
    {
        append_rect(rects, x, y, thickness, segment, r, g, b);
    }
    if ((mask & 0b0000010U) != 0U) // lower-right
    {
        append_rect(rects, x + segment + thickness, y, thickness, segment, r, g, b);
    }
    if ((mask & 0b0000001U) != 0U) // bottom
    {
        append_rect(rects, x + thickness, y, segment, thickness, r, g, b);
    }
}

void append_number(
    std::vector<ScreenOverlayRect>& rects,
    int x,
    int y,
    uint32_t value,
    int scale,
    float r,
    float g,
    float b)
{
    const int step = (6 * scale) + 2;

    if (value >= 100U)
    {
        append_digit(rects, x, y, static_cast<int>((value / 100U) % 10U), scale, r, g, b);
        append_digit(rects, x + step, y, static_cast<int>((value / 10U) % 10U), scale, r, g, b);
        append_digit(rects, x + (step * 2), y, static_cast<int>(value % 10U), scale, r, g, b);
        return;
    }

    if (value >= 10U)
    {
        append_digit(rects, x, y, static_cast<int>((value / 10U) % 10U), scale, r, g, b);
        append_digit(rects, x + step, y, static_cast<int>(value % 10U), scale, r, g, b);
        return;
    }

    append_digit(rects, x, y, static_cast<int>(value), scale, r, g, b);
}

void append_fraction_label(
    std::vector<ScreenOverlayRect>& rects,
    int x,
    int y,
    uint32_t value,
    uint32_t max_value)
{
    const float label_r = 0.95F;
    const float label_g = 0.95F;
    const float label_b = 0.95F;
    const int scale = 1;
    const int step = (6 * scale) + 2;

    append_number(rects, x, y, value, scale, label_r, label_g, label_b);

    // Slash separator between current and max values.
    append_rect(rects, x + (step * 3), y + 2, 1, 7, label_r, label_g, label_b);
    append_number(rects, x + (step * 4), y, max_value, scale, label_r, label_g, label_b);
}

void append_legend_chip(
    std::vector<ScreenOverlayRect>& rects,
    int x,
    int y,
    float r,
    float g,
    float b)
{
    append_rect(rects, x, y, 8, 8, r, g, b);
}

} // namespace

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
    const int party_bar_width = static_cast<int>(static_cast<float>(bar_width) * party_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 10,
        party_bar_width,
        bar_height,
        0.24F,
        0.70F,
        0.88F));
    append_legend_chip(rects, origin_x + panel_width - 58, party_y + 12, 0.24F, 0.70F, 0.88F);
    append_fraction_label(rects, origin_x + panel_width - 46, party_y + 10, static_cast<uint32_t>(metrics.m_party_queue_size), 8U);

    const float ability_intensity = std::min(1.0F, static_cast<float>(metrics.m_ability_queue_size) / 8.0F);
    const int ability_bar_width = static_cast<int>(static_cast<float>(bar_width) * ability_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 30,
        ability_bar_width,
        bar_height,
        0.93F,
        0.67F,
        0.25F));
    append_legend_chip(rects, origin_x + panel_width - 58, party_y + 32, 0.93F, 0.67F, 0.25F);
    append_fraction_label(rects, origin_x + panel_width - 46, party_y + 30, static_cast<uint32_t>(metrics.m_ability_queue_size), 8U);

    const float selection_intensity = std::min(1.0F, static_cast<float>(metrics.m_selected_actor_count) / 4.0F);
    const int selection_bar_width = static_cast<int>(static_cast<float>(bar_width) * selection_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        party_y + 50,
        selection_bar_width,
        bar_height,
        metrics.m_has_primary_actor ? 0.34F : 0.22F,
        metrics.m_has_primary_actor ? 0.86F : 0.27F,
        0.39F));
    append_legend_chip(
        rects,
        origin_x + panel_width - 58,
        party_y + 52,
        metrics.m_has_primary_actor ? 0.34F : 0.22F,
        metrics.m_has_primary_actor ? 0.86F : 0.27F,
        0.39F);
    append_fraction_label(rects, origin_x + panel_width - 46, party_y + 50, metrics.m_selected_actor_count, 4U);

    const float item_queue_intensity = std::min(1.0F, static_cast<float>(metrics.m_item_queue_size) / 8.0F);
    const int item_queue_bar_width = static_cast<int>(static_cast<float>(bar_width) * item_queue_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 10,
        item_queue_bar_width,
        bar_height,
        0.89F,
        0.45F,
        0.31F));
    append_legend_chip(rects, origin_x + panel_width - 58, inventory_y + 12, 0.89F, 0.45F, 0.31F);
    append_fraction_label(rects, origin_x + panel_width - 46, inventory_y + 10, static_cast<uint32_t>(metrics.m_item_queue_size), 8U);

    const float item_quantity_intensity =
        std::min(1.0F, static_cast<float>(metrics.m_inventory_total_quantity) / 12.0F);
    const int item_quantity_bar_width = static_cast<int>(static_cast<float>(bar_width) * item_quantity_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 30,
        item_quantity_bar_width,
        bar_height,
        0.95F,
        0.82F,
        0.36F));
    append_legend_chip(rects, origin_x + panel_width - 58, inventory_y + 32, 0.95F, 0.82F, 0.36F);
    append_fraction_label(rects, origin_x + panel_width - 46, inventory_y + 30, metrics.m_inventory_total_quantity, 12U);

    const float entry_count_intensity =
        std::min(1.0F, static_cast<float>(metrics.m_inventory_entry_count) / 6.0F);
    const int entry_count_bar_width = static_cast<int>(static_cast<float>(bar_width) * entry_count_intensity);
    rects.push_back(make_panel(
        origin_x + bar_margin,
        inventory_y + 50,
        entry_count_bar_width,
        bar_height,
        0.60F,
        0.70F,
        0.94F));
    append_legend_chip(rects, origin_x + panel_width - 58, inventory_y + 52, 0.60F, 0.70F, 0.94F);
    append_fraction_label(rects, origin_x + panel_width - 46, inventory_y + 50, metrics.m_inventory_entry_count, 6U);

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