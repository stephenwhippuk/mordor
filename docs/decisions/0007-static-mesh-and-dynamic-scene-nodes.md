# 0007: Static Mesh and Dynamic Scene Node Ownership

Status: Accepted  
Date: 2026-05-10

## Context
The renderer now builds a merged static world mesh for floors and walls, and a wall-surface octree is available for collision and occlusion acceleration. That optimization exposed a correctness problem: player and interactable markers were still baked into the static mesh, so the player orb could not move visually without rebuilding the world mesh. The runtime also needs a clear ownership model for future entities, keys, switches, and similar gameplay-facing objects.

## Options Considered
1. Keep all visible markers baked into the static world mesh and rebuild mesh content when runtime positions change.
2. Split only the player marker out of the static mesh and leave interactable markers baked into map geometry.
3. Keep floors/walls in a static mesh node and represent player/interactable markers as separate scene nodes with independent transforms.

## Decision
Adopt option 3.

The runtime now treats the merged world mesh as static scene content and keeps player, key, and switch markers as separate scene nodes. The same runtime visual-node path is also the baseline for future items and NPCs.

Rules:
1. Floors, walls, and door polygons remain in the static world mesh.
2. Player, key, switch, and future movable or gameplay-significant markers are rendered from scene nodes, not baked into static mesh geometry.
3. Player movement updates scene-node transforms rather than mutating world-mesh content.
4. Static-mesh collision queries use the wall-surface octree, while dynamic scene nodes maintain their own bounds in the scene graph.
5. Movement checks against the static mesh should be expressed as dynamic scene-node bounds versus wall-octree overlap tests rather than as mesh rebuild side effects.

## Consequences
1. Positive: Dynamic markers can move independently without rebuilding or re-uploading the static world mesh.
2. Positive: Scene graph ownership now matches future entity/interactable needs and keeps collision/readability queries aligned with runtime transforms.
3. Tradeoff: The renderer now has a separate dynamic-marker submission path in addition to the static mesh path.
4. Operational impact: Scene-node transform updates must keep spatial-index bounds in sync so picking and culling stay correct.
5. Operational impact: Collision helpers must keep shared wall-octree queries available to gameplay-facing movement and visibility systems.

## Follow-Up Actions
1. Add an explicit static-mesh scene node payload contract instead of relying on the current renderer/world-mesh bootstrap coupling.
2. Extend wall-octree-backed collision helpers from the current player movement path to additional dynamic entities and visibility checks.
3. Extend separate scene-node rendering to additional runtime objects such as items, traps, and NPCs.