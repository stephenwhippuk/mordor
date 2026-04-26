# Immediate Next Steps (2-3 Weeks)

## Goal
Turn the rendered sandbox into a scene-aware runtime that is ready for Phase 2 interaction work.

## Work Items
1. Implement scene-graph storage and stable `scene_node_id` allocation.
2. Add loose-octree broadphase support for scene bounds, clipping candidates, and picking candidates.
3. Extend map loading to emit both simulation cell data and scene-node construction data.
4. Define actor and interactable schemas against scene-node attachment points.
5. Implement blocking and occupancy rules over simulation-owned spatial records.
6. Add debug views for scene bounds, octree population, and picking diagnostics.
7. Implement first interaction state machines (door/chest/trap).

## Success Criteria
1. The handcrafted dungeon loads into both scene-query structures and simulation spatial records from one authored asset.
2. Picking and clipping can query stable scene nodes without brute-force world scans.
3. Actor and interactable work can begin without redefining world attachment or occupancy ownership.

## Follow-On
1. Start Phase 2 actor/interactable implementation once the above runtime scaffolding is in place.
