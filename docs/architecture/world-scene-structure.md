# World Scene Structure

## Purpose
Define the minimum world and scene structure required before Phase 2 systems attach actors, interactables, and later gameplay data to the rendered world.

## Design Goals
1. Support efficient view clipping and future occlusion-friendly broadphase queries.
2. Support deterministic picking and interaction targeting without per-frame brute-force scans.
3. Keep authored map data separate from runtime scene-node state.
4. Allow actors and interactables to bind to stable scene-node identities.
5. Preserve a simple path for static-world optimization while keeping dynamic objects movable.

## Runtime Model

### Layer Policy
The runtime uses two distinct spatial/semantic layers:
1. Visual-Occlusion Layer: controls rendering participation and optional occlusion contribution.
2. Physical-Blocking Layer: controls movement/collision blocking.

Rules:
1. A node may be visible but non-blocking (for example keys or floor switches).
2. A node may be both visible and blocking (for example statues or large props).
3. Static world walls are blocking by default unless authored as explicit visual-only exceptions (for example illusory walls).
4. Interaction eligibility is independent from blocking; non-blocking objects can still be interactable anchors.

### 1) Authored World Definition
The handcrafted map remains the source asset for layout and authored metadata.

It is loaded into three runtime products:
1. Static geometry records used by rendering.
2. Simulation cell data used for blocking, occupancy, and rule queries.
3. Scene-node definitions used to construct runtime scene state.

Authoring contract:
1. The tile grid defines mesh/collision only (visual symbols plus collision bitmask data).
2. Non-mesh entities (keys, switches, props, items, NPC markers, spawn anchors) are authored in a separate map-associated entity-placement table.
3. Entity placements carry explicit collision bits (`solid` currently) plus a `movable` flag.
4. Non-movable solid entity placements merge into static occupancy and wall-collision build data; movable solid entities remain dynamic blockers.
5. Handcrafted map loading uses strict table-based non-mesh entity authoring (`@entity ...` records) and does not accept legacy entity tile symbols.

Handcrafted entity record format:
1. Record syntax: `@entity <kind> <col> <row> <symbol> <solid> <movable>`
2. `<kind>` values: `key`, `switch`, `prop`, `item`, `npc`, `spawn`
3. `<solid>` values: `0|1` or `false|true`
4. `<movable>` values: `0|1` or `false|true`
5. `col,row` are zero-based tile coordinates and must be in bounds on a non-blocking tile.
6. Duplicate records at the same `(kind,col,row)` are rejected.

Examples:
1. `@entity key 4 7 K 0 0`
2. `@entity prop 12 5 T 1 0`
3. `@entity spawn 2 2 A 0 1`

### 2) Scene Graph
Use a scene graph with stable `scene_node_id` handles.

Each node owns:
1. Parent/child relationship.
2. Local transform.
3. Derived world transform.
4. Local bounds and cached world bounds.
5. Category flags such as `static_world`, `dynamic_actor`, `interactable_anchor`, and `debug_only`.
6. Optional payload handle to render, collision, or gameplay-facing data.

Initial node families:
1. World root.
2. Sector or chunk nodes grouping nearby static map geometry.
3. Static geometry nodes for floors, walls, ramps, and large props.
4. Anchor nodes for interactables and spawn points.
5. Dynamic attachment nodes for actors, projectiles, and temporary effects.

Rules:
1. Static map nodes are created once at map load and only change on explicit world edits.
2. Dynamic nodes update through deterministic simulation state, not render-thread side effects.
3. Parenting is structural, not gameplay-authoritative. Simulation blocking remains outside the scene graph.
4. Visual markers for runtime entities and interactables should be emitted as separate scene nodes rather than baked into the static world mesh.
5. Static-mesh collision checks should treat dynamic scene-node bounds as the query primitive and the wall octree as the acceleration structure for merged world geometry.
6. Movement blocking is opt-in for dynamic visuals and must not be inferred from render symbol alone.

### 3) Spatial Index
Use a loose octree over world-space node bounds for broadphase queries.

The octree indexes renderable and pickable nodes, not the authored file directly.

Why this is the baseline:
1. Better fit than a pure flat list once the map grows beyond debug-scale scenes.
2. More flexible than a strict tile-only lookup because later scenes will include vertical layering, props, triggers, and attachments.
3. Keeps clipping, picking, overlap probes, and nearby-node discovery on the same broadphase primitive.

Operational rules:
1. Static map nodes are built at map load and inserted into the scene index during scene construction.
2. Runtime visual node add/move operations currently trigger a full scene-index rebuild to preserve deterministic behavior.
3. Octree leaves store compact node-handle lists rather than duplicating payload data.
4. Broadphase query results always require a narrower follow-up check against bounds, cell occupancy, or specific gameplay rules.
5. Incremental reinsertion based on loose-octree tolerance remains a planned optimization, not the current runtime path.

### 4) Simulation Spatial Data
Do not use the octree as the authoritative gameplay blocking model.

Simulation keeps separate deterministic spatial data:
1. Tile or cell blocking map.
2. Occupancy records for actors and interactables.
3. Optional navigation or reachability overlays later.

This split avoids forcing gameplay rules to depend on renderer-oriented hierarchy behavior.

## Query Paths

### Picking
1. Convert cursor position into a world-space pick ray or isometric pick volume.
2. Query the loose octree for candidate nodes whose world bounds intersect the pick primitive.
3. Run narrow-phase tests against node bounds and, where needed, map-cell metadata.
4. Resolve to a stable `scene_node_id`, then map to gameplay handles if the node owns an interactable or actor payload.

### View Clipping
1. Build the camera frustum or isometric clip volume.
2. Query the loose octree for candidate world nodes.
3. Perform bounds culling before render submission.
4. Keep sector or chunk nodes coarse enough to reduce traversal overhead but small enough to avoid giant always-visible bounds.

### Interaction Neighborhood Queries
1. Use simulation cell data first for blocking and occupancy.
2. Use octree lookups for nearby anchor discovery, debug overlays, and non-grid-aligned scene content.
3. Resolve ambiguous hits through gameplay rules rather than scene-graph ordering.

## Update Model
1. Node transforms and bounds propagate from parent to child via recursive world-bounds updates on mutation.
2. Static-world scene construction happens at map load; runtime visual node mutations currently rebuild the scene index for correctness.
3. Dynamic actor and interactable attachments publish transform changes through simulation-owned systems.
4. Picking and culling read from the latest committed scene state for the frame.
5. Static mesh collision uses an acceleration structure derived from merged wall surfaces, while dynamic scene nodes update their own bounds independently.
6. Runtime visual nodes are generic enough to cover follow-on items, NPCs, and temporary gameplay markers without requiring new static-mesh geometry paths.

## Phase 2 Contracts
Phase 2 systems should build against these contracts:
1. Actors and interactables reference `scene_node_id` when they need a world attachment point.
2. Blocking, occupancy, and rule evaluation reference simulation spatial records, not scene-graph parentage.
3. Renderer-facing clipping and picking services consume scene bounds from the octree-backed scene state.
4. Map loading must emit both simulation cell data and entity-placement-driven scene-node construction data from the same authored asset.

## Deferred Details
These are intentionally left for later implementation work:
1. Exact octree depth, leaf capacity, and rebalance thresholds.
2. Final narrow-phase primitive set for tile, prop, and actor picking.
3. Occlusion-culling strategy beyond broadphase clipping.
4. Serialization layout for scene-node payload records.
5. Editor workflows for live scene editing.