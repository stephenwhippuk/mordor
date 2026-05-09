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
3. Dynamic scene nodes are non-blocking by default unless they explicitly opt into `BlocksMovement`.
4. Wall/octree static geometry remains physical-blocking unless an explicit gameplay rule marks a visual-only exception (for example, an illusory wall).

## Consequences
1. Positive: Keys, floor switches, and similar interactables can remain visible and interactable without preventing traversal.
2. Positive: Blocking interactables can be opt-in and deterministic, independent of render symbol.
3. Positive: Illusory wall behavior is representable without violating rendering contracts.
4. Tradeoff: Systems must evaluate both layers instead of assuming renderability implies collision.
5. Operational impact: Movement, occlusion, and interaction pipelines must keep layer semantics aligned in tests and documentation.

## Follow-Up Actions
1. Add explicit data fields in map/entity schemas to author visual-only versus physically blocking geometry.
2. Extend visibility helpers to support visual-only occluders and configurable occlusion participation.
3. Add acceptance tests for illusory walls and mixed blocking/non-blocking interactables in the same tile neighborhood.