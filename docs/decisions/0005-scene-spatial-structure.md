# 0005: World Scene and Spatial Query Structure

Status: Accepted  
Date: 2026-04-26

## Context
Phase 2 needs a stable world attachment model before actors, interactables, and later gameplay payloads can be introduced. The current map-loading and debug-render path can draw a handcrafted dungeon, but it does not yet define how world geometry, anchors, picking targets, clipping bounds, and future dynamic objects should be represented together at runtime.

We need a baseline that:
1. Supports efficient clipping and picking.
2. Provides stable scene identities that gameplay systems can reference.
3. Keeps deterministic blocking and occupancy logic separate from renderer-oriented hierarchy concerns.
4. Scales past the current debug map without forcing brute-force scans.

## Options Considered
1. Flat render list with ad hoc lookup tables for picking and gameplay attachments.
2. Single uniform spatial grid used for rendering, picking, and authoritative gameplay state.
3. Scene graph with stable node handles, backed by a loose octree for broadphase queries, while simulation keeps separate occupancy and blocking data.

## Decision
Use a **scene graph with stable `scene_node_id` handles**, backed by a **loose octree** for clipping and picking broadphase queries, while keeping **simulation-owned cell and occupancy data** as the authoritative gameplay spatial model.

## Rationale
1. A scene graph gives actors, interactables, and future props stable attachment points without coupling gameplay directly to render payloads.
2. A loose octree is a better broadphase fit than a flat list once the world grows beyond the current sandbox and starts mixing static geometry with dynamic nodes.
3. Keeping blocking and occupancy outside the scene graph preserves deterministic gameplay logic and avoids using renderer-friendly hierarchy as a hidden gameplay authority.
4. The split allows static-world optimization now while leaving room for later vertical layering, triggers, and non-grid-aligned content.

## Consequences
1. Positive: picking, clipping, and nearby-node queries share one broadphase structure instead of diverging into unrelated ad hoc paths.
2. Positive: Phase 2 actor and interactable schemas can reference scene nodes without redefining world identity.
3. Positive: map loading has a clear contract to emit both simulation data and scene-construction data from the same authored asset.
4. Tradeoff: runtime world state now has three related representations to keep in sync: authored map data, scene-node state, and simulation spatial records.
5. Tradeoff: dynamic-node updates need explicit dirty propagation and octree reinsertion rules.

## Follow-Up Actions
1. Implement scene-graph storage and scene-node handle allocation in engine code.
2. Extend map loading so it emits scene-node construction data alongside simulation cell records.
3. Define Phase 2 actor and interactable schemas to reference `scene_node_id` where world attachment is required.
4. Add debug visualization for scene bounds, octree population, and pick candidates.
5. Revisit octree depth and leaf-capacity thresholds once real scene sizes and actor counts are measurable.