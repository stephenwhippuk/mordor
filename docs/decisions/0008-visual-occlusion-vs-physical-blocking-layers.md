# 0008: Visual-Occlusion and Physical-Blocking Layer Separation

Status: Accepted  
Date: 2026-05-10

## Context
Runtime now supports dynamic scene-node visuals and wall-octree collision queries, but gameplay readability and movement semantics require a stricter rule: seeing an object should not automatically mean that object blocks movement. We need an explicit two-layer model so the engine can represent cases such as illusory walls, floor switches, and collectible keys while still allowing intentionally blocking interactables such as statues or heavy props.

## Options Considered
1. Keep a single unified layer where any visible geometry can block movement.
2. Keep static walls separate but let dynamic visuals implicitly decide blocking by symbol/type.
3. Define two explicit layers: visual-occlusion and physical-blocking, with per-entity blocking flags.

## Decision
Adopt option 3.

The runtime model now treats visibility and movement as separate concerns:
1. Visual-occlusion layer controls what is rendered and what can occlude line of sight/readability.
2. Physical-blocking layer controls whether movement/collision is blocked.
3. Map physical blocking is authored as a collision bitmask layer (`solid` bit currently), independent of visual symbol placement.
4. Dynamic scene nodes are non-blocking by default unless they explicitly opt into `BlocksMovement`.
5. Wall/octree static geometry remains physical-blocking unless an explicit gameplay rule marks a visual-only exception (for example, an illusory wall).
6. Baseline authored support now includes visual-only wall tiles (`W`) that render as wall occluders while remaining non-blocking for occupancy and physical wall-octree collision.
7. Map-authored non-mesh entities are represented in a dedicated entity placement table rather than tile symbol codes; runtime visual scene nodes are spawned from that table.
8. Entity placements carry explicit `solid` and `movable` semantics: non-movable solids merge into static collision/occupancy data, while movable solids stay in dynamic collision paths.
9. Handcrafted content now uses strict entity-table records for non-mesh entities and does not normalize legacy entity tile symbols from map rows.

## Consequences
1. Positive: Keys, floor switches, and similar interactables can remain visible and interactable without preventing traversal.
2. Positive: Blocking interactables can be opt-in and deterministic, independent of render symbol.
3. Positive: Illusory wall behavior is representable without violating rendering contracts.
4. Tradeoff: Systems must evaluate both layers instead of assuming renderability implies collision.
5. Operational impact: Movement, occlusion, and interaction pipelines must keep layer semantics aligned in tests and documentation.

## Follow-Up Actions
1. Extend explicit visual-only versus physically blocking authoring fields from map tiles into entity/interactable schemas.
2. Extend visibility helpers to support configurable occlusion participation by layer and symbol.
3. Merge non-movable solid scene nodes into static collision data generation, while keeping movable nodes in dynamic collision paths.
4. Add expanded acceptance tests for mixed illusory walls, blocking props, and non-blocking interactables in the same tile neighborhood.