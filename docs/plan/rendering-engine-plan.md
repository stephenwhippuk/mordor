# Rendering Engine Integration Plan

## Purpose
Define the implementation path from the current debug/scissor map draw path to a production-capable 3D isometric renderer that preserves gameplay readability and supports mouse-driven selection.

## Scope
1. Proper 3D rendering for floors and walls from map data.
2. Isometric-style camera from an angled perspective (not top-down).
3. Occlusion-handling strategy so walls do not block interaction readability.
4. Mouse interaction path for entity/tile selection and gameplay command issuing.

## Non-Goals
1. Advanced post-processing and visual polish.
2. Final art pipeline and high-fidelity materials.
3. Full content-generation integration (handled in later phases).

## Workstream

### R0: Contract Lock and Baseline Decisions
Deliverables:
1. Accepted decision record for camera, occlusion behavior, and mouse-picking policy.
2. Rendering-layer contracts for world submission, HUD compositing, and picking queries.

Validation:
1. Decision record linked from backlog and architecture docs.
2. Public API boundaries documented in headers and architecture notes.

### R1: 3D World Geometry Pipeline
Deliverables:
1. Shader + VBO/VAO draw path replacing scissor-clear world rendering for map geometry.
2. Floor and wall geometry generation from tile map and scene data.
3. Deterministic draw ordering and render-state setup.

Validation:
1. Handcrafted map renders as 3D floor and wall geometry in runtime.
2. Unit/integration checks remain green.

### R2: Isometric Camera Rig
Deliverables:
1. Perspective camera with configurable pitch/yaw/zoom tuned for isometric gameplay readability.
2. Existing input binding layer extended for camera orbit/zoom/pan controls.
3. Configurable defaults in code constants for rapid iteration.

Validation:
1. Camera can orbit and zoom while preserving stable interaction framing.
2. Runtime diagnostics expose camera state for tuning.

### R3: Mouse Selection and Picking
Deliverables:
1. Screen-to-world ray generation from mouse coordinates.
2. Scene-assisted hit query path for candidate tile/entity selection.
3. Stable priority rules when multiple candidates overlap (nearest valid gameplay target first).

Validation:
1. Mouse can select reachable entities and tiles in a dense scene.
2. Selection behavior is deterministic and testable.

### R4: Occlusion-Aware Readability
Deliverables:
1. Wall occlusion response states: visible, faded, or hidden.
2. Camera-to-selection corridor handling so foreground walls do not block interaction feedback.
3. Debug overlays for occluders and selected targets.

Validation:
1. Player can maintain interaction visibility in cluttered rooms without disabling walls globally.
2. Selection remains readable while preserving scene context.

### R5: Culling and Render Metrics
Deliverables:
1. Frustum culling using scene/spatial structures.
2. Submission metrics for visible chunks/nodes and draw counts.
3. Baseline frame-time instrumentation around render passes.

Validation:
1. Reduced submission set in non-visible areas.
2. Metrics logged and suitable for regression tracking.

## Test Strategy
1. Unit tests for selection ordering and target-resolution contracts.
2. Integration tests for camera + picking + occlusion interplay on handcrafted maps.
3. Runtime debug checks for culling counts and visibility state transitions.

## Exit Criteria
1. 3D walls/floors render in a proper isometric-perspective view.
2. Mouse-driven tile/entity selection works reliably.
3. Foreground occluders no longer block practical selection readability.
4. Culling and rendering metrics are available for performance tracking.
