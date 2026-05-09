# Immediate Next Steps (2-3 Weeks)

## Goal
Complete Phase 5 rendering baseline by adding occlusion handling and performance metrics.

## Work Items
1. Add wall fade/hide occlusion handling so foreground geometry does not block practical selection.
2. Add render submission and culling metrics for baseline performance tracking.

## Success Criteria
1. Handcrafted map renders as stable 3D floor/wall geometry from an isometric angle.
2. Mouse interactions can select intended entities/tiles in cluttered scenes.
3. Occlusion handling preserves readability without globally hiding world geometry.
4. Existing command/ability/inventory/HUD baselines remain green while renderer changes land.

## Follow-On
1. Begin Phase 6 generation once rendering and mouse interaction contracts are stable.

## Carry-Over Rendering Issues (From Quick Playtest)
1. Wall mesh over-generation: world mesh currently emits full wall side faces per blocked tile, including interior faces between adjacent wall tiles. This creates unnecessary polygons and becomes visible during occlusion fades.
2. Occlusion anchor ambiguity: wall fade currently appears camera-position driven rather than clearly actor/party-position driven. Actor location and whether camera motion is coupled to actor state are not yet explicit in runtime diagnostics.

## Follow-Up Actions
1. Add a mesh optimization pass that suppresses interior wall faces shared by neighboring blocked tiles.
2. Define and document occlusion source-of-truth (camera vs active party member), then update fade calculation accordingly.
3. Add debug telemetry that logs occlusion reference position and active party tile so behavior can be verified during movement tests.
