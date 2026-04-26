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

### 1) Authored World Definition
The handcrafted map remains the source asset for layout and authored metadata.

It is loaded into three runtime products:
1. Static geometry records used by rendering.
2. Simulation cell data used for blocking, occupancy, and rule queries.
3. Scene-node definitions used to construct runtime scene state.

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

### 3) Spatial Index
Use a loose octree over world-space node bounds for broadphase queries.

The octree indexes renderable and pickable nodes, not the authored file directly.

Why this is the baseline:
1. Better fit than a pure flat list once the map grows beyond debug-scale scenes.
2. More flexible than a strict tile-only lookup because later scenes will include vertical layering, props, triggers, and attachments.
3. Keeps clipping, picking, overlap probes, and nearby-node discovery on the same broadphase primitive.

Operational rules:
1. Static nodes are inserted once during map load.
2. Dynamic nodes are reinserted only when their world bounds move outside the current loose-octree tolerance.
3. Octree leaves store compact node-handle lists rather than duplicating payload data.
4. Broadphase results always require a narrower follow-up check against bounds, cell occupancy, or specific gameplay rules.

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
1. Node transforms and bounds propagate from parent to child through explicit dirty flags.
2. Static-world rebuild happens on map load only.
3. Dynamic actor and interactable attachments publish transform changes through simulation-owned systems.
4. Picking and culling read from the latest committed scene state for the frame.

## Phase 2 Contracts
Phase 2 systems should build against these contracts:
1. Actors and interactables reference `scene_node_id` when they need a world attachment point.
2. Blocking, occupancy, and rule evaluation reference simulation spatial records, not scene-graph parentage.
3. Renderer-facing clipping and picking services consume scene bounds from the octree-backed scene state.
4. Map loading must emit both simulation cell data and scene-node construction data from the same authored asset.

## Deferred Details
These are intentionally left for later implementation work:
1. Exact octree depth, leaf capacity, and rebalance thresholds.
2. Final narrow-phase primitive set for tile, prop, and actor picking.
3. Occlusion-culling strategy beyond broadphase clipping.
4. Serialization layout for scene-node payload records.
5. Editor workflows for live scene editing.