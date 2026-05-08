# 0006: Camera, Occlusion, and Mouse Picking Policy

Status: Accepted  
Date: 2026-05-09

## Context
The engine currently renders map data through a temporary debug/scissor path. Before dungeon generation begins, rendering and interaction readability must be stabilized for an angled isometric-style view. We need a clear policy for:
1. Camera style and configurability.
2. Foreground wall occlusion behavior.
3. Mouse-driven entity/tile selection and picking contracts.

## Options Considered
1. Keep current debug draw path and defer 3D/picking until after generation.
2. Implement full 3D rendering but keep keyboard-only targeting.
3. Implement 3D rendering with explicit occlusion readability rules and mouse picking before generation.

## Decision
Adopt option 3.

The roadmap now inserts a dedicated rendering phase before generation. The renderer must support:
1. Shader/VBO 3D map rendering for floors and walls.
2. Tunable angled isometric-perspective camera.
3. Mouse ray picking for tile/entity selection.
4. Occlusion-aware wall fade/hide behavior to preserve selection readability.

## Consequences
1. Positive: Interaction readability improves before procedural complexity is introduced.
2. Positive: Gameplay command loops can transition to real mouse-driven flows earlier.
3. Tradeoff: Generation work starts later and requires roadmap renumbering.
4. Operational impact: Additional rendering and interaction integration tests are required.

## Follow-Up Actions
1. Execute Phase 5 rendering tasks in `docs/plan/rendering-engine-plan.md`.
2. Add unit/integration coverage for picking resolution and occlusion handling.
3. Keep simulation as authority for target validity while renderer handles visual readability and interaction affordances.
