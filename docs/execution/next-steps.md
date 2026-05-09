# Immediate Next Steps (2-3 Weeks)

## Goal
Continue Phase 5 by adding mouse picking and occlusion handling to the tunable camera system.

## Work Items
1. Implement mouse-driven tile/entity selection and stable picking resolution.
2. Add wall fade/hide occlusion handling so foreground geometry does not block practical selection.
3. Add render submission and culling metrics for baseline performance tracking.

## Success Criteria
1. Handcrafted map renders as stable 3D floor/wall geometry from an isometric angle.
2. Mouse interactions can select intended entities/tiles in cluttered scenes.
3. Occlusion handling preserves readability without globally hiding world geometry.
4. Existing command/ability/inventory/HUD baselines remain green while renderer changes land.

## Follow-On
1. Begin Phase 6 generation once rendering and mouse interaction contracts are stable.
