# Immediate Next Steps (2-3 Weeks)

## Goal
Start Phase 5 implementation by replacing the debug draw path with a proper 3D renderer that preserves interaction readability.

## Work Items
1. Implement shader/VBO floor and wall rendering from map data.
2. Implement angled isometric-perspective camera controls with tunable defaults.
3. Add mouse-driven tile/entity selection and stable picking resolution.
4. Add wall fade/hide occlusion handling so foreground geometry does not block practical selection.
5. Add render submission and culling metrics for baseline performance tracking.

## Success Criteria
1. Handcrafted map renders as stable 3D floor/wall geometry from an isometric angle.
2. Mouse interactions can select intended entities/tiles in cluttered scenes.
3. Occlusion handling preserves readability without globally hiding world geometry.
4. Existing command/ability/inventory/HUD baselines remain green while renderer changes land.

## Follow-On
1. Begin Phase 6 generation once rendering and mouse interaction contracts are stable.
