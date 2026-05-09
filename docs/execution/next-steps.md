# Immediate Next Steps (2-3 Weeks)

## Goal
Continue Phase 5 by building camera, picking, and occlusion handling on top of the new shader/VBO world-rendering baseline.

## Work Items
1. Implement angled isometric-perspective camera controls with tunable defaults.
2. Add mouse-driven tile/entity selection and stable picking resolution.
3. Add wall fade/hide occlusion handling so foreground geometry does not block practical selection.
4. Add render submission and culling metrics for baseline performance tracking.

## Success Criteria
1. Handcrafted map renders as stable 3D floor/wall geometry from an isometric angle.
2. Mouse interactions can select intended entities/tiles in cluttered scenes.
3. Occlusion handling preserves readability without globally hiding world geometry.
4. Existing command/ability/inventory/HUD baselines remain green while renderer changes land.

## Follow-On
1. Begin Phase 6 generation once rendering and mouse interaction contracts are stable.
