# Phase Backlog and Dependencies

## Purpose
Provide implementation-level work items with explicit dependencies across development phases.

## Backlog Fields
1. Task ID
2. Description
3. Depends On
4. Priority
5. Status
6. Deliverable Link

## Phase 0 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P0-01 | Set up build system and target profiles | None | High | Complete | plan/development-phases.md |
| P0-02 | Add logging, assertions, and profiling hooks | P0-01 | High | Complete | decisions/0001-logging-library.md |
| P0-03 | Configure formatting, linting, static analysis | P0-01 | Medium | Complete | plan/development-phases.md |
| P0-04 | Create initial decision records for rendering and data model | P0-01 | High | Complete | decisions/0002-rendering-baseline.md |

## Phase 1 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P1-01 | Implement fixed tick simulation loop | P0-02 | High | Complete | plan/development-phases.md |
| P1-02 | Build OpenGL renderer bootstrap and map draw path | P0-04, P1-01 | High | Complete | architecture/engine-architecture.md |
| P1-03 | Implement isometric camera controls | P1-02 | High | Complete | architecture/engine-architecture.md |
| P1-04 | Add input binding/action mapping layer | P1-01 | Medium | Complete | plan/development-phases.md |
| P1-05 | Load and render handcrafted dungeon test map | P1-02, P1-03 | High | Complete | execution/next-steps.md |
| P1-06 | Define world scene graph and spatial query baseline | P1-05 | High | Complete | decisions/0005-scene-spatial-structure.md |
| P1-07 | Implement scene-graph storage and map-emitted scene data | P1-06 | High | Complete | architecture/world-scene-structure.md |
| P1-08 | Add scene picking helpers and debug query instrumentation | P1-07 | Medium | Complete | architecture/world-scene-structure.md |

### Known Limitations — P1-03

**Camera rotation in the scissor-based debug draw path is visually incorrect.**
Rotation is applied to each tile's centre position, but the tile rectangle itself is axis-aligned (width/height are only scaled, not rotated). Tiles therefore do not rotate with the camera. This was accepted as a known defect for the debug path.
Fix options when a real quad/shader pipeline exists (P1-05 or later):
- (a) Disable rotation in this path until a proper pipeline is in place, or
- (b) Compute the rotated rectangle's AABB and use that for the scissor rect as an approximation.
Must be resolved before rotation is considered production-quality. Tracked for P1-05 or the first shader-based tile draw task.

## Phase 2 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P2-01 | Define actor and interactable component schema | P1-06 | High | Complete | architecture/world-scene-structure.md |
| P2-02 | Implement interaction state machines (door/chest/trap) | P2-01 | High | Complete | plan/development-phases.md |
| P2-03 | Implement key and switch logic model | P2-02 | High | Complete | plan/basic-engine-release.md |
| P2-04 | Implement blocking and occupancy rules | P1-06, P2-01 | Medium | Complete | architecture/world-scene-structure.md |
| P2-05 | Add baseline unit tests for simulation rules | P2-02, P2-03, P2-04 | Medium | Complete | quality/testing-strategy.md |

## Phase 3 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P3-01 | Implement line-of-sight and occlusion checks | P2-04 | High | Complete | plan/development-phases.md |
| P3-02 | Add directional hearing event system | P2-01 | Medium | Complete | architecture/engine-architecture.md |
| P3-03 | Build fog-of-war explored and visible map state | P3-01 | High | Complete | plan/basic-engine-release.md |
| P3-04 | Add perception debug overlays | P3-01, P3-02, P3-03 | Medium | Complete | quality/testing-strategy.md |

## Phase 4 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P4-01 | Implement party selection and command issuing | P3-03 | High | Complete | plan/development-phases.md |
| P4-02 | Implement ability-driven action resolution pipeline | P2-02, P4-01 | High | Complete | plan/development-phases.md |
| P4-03 | Implement inventory model and map-targeted item use | P2-03, P4-01 | High | Complete | plan/basic-engine-release.md |
| P4-04 | Implement baseline HUD for party and inventory | P4-01, P4-03 | Medium | Complete | architecture/engine-architecture.md |

## Phase 5 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P5-01 | Implement shader/VBO world rendering for map walls and floors | P1-05, P1-07 | High | Complete | plan/rendering-engine-plan.md |
| P5-02 | Implement tunable isometric-perspective camera rig | P1-03, P5-01 | High | Complete | plan/rendering-engine-plan.md |
| P5-03 | Implement mouse-driven tile/entity selection and picking | P1-08, P5-01 | High | Complete | plan/rendering-engine-plan.md |
| P5-04 | Implement occlusion-aware wall fade/hide for interaction readability | P3-01, P5-02, P5-03 | High | Complete | plan/rendering-engine-plan.md |
| P5-05 | Add frustum culling and render submission metrics | P1-08, P5-01 | Medium | Complete | plan/rendering-engine-plan.md |
| P5-06 | Rework occlusion to actor-centric corridor and add keyboard player-move probe controls | P5-04 | High | Complete | execution/next-steps.md |
| P5-07 | Merge wall CSG surfaces to visible polygons only (top and boundary runs) | P5-01, P5-04 | High | Complete | architecture/engine-architecture.md |
| P5-08 | Static mesh lifetime + wall-surface octree baseline for collision/occlusion queries | P5-07 | High | Complete | architecture/engine-architecture.md |
| P5-09 | Move player/key/switch markers to separate scene nodes and keep static mesh marker-free | P5-08 | High | Complete | decisions/0007-static-mesh-and-dynamic-scene-nodes.md |
| P5-10 | Route player movement collision through wall octree and generalize runtime visual nodes for future item/NPC rendering | P5-09 | High | Complete | decisions/0007-static-mesh-and-dynamic-scene-nodes.md |
| P5-11 | Formalize dual-layer map semantics: visual-occlusion vs physical-blocking, with explicit node blocking flags | P5-10 | High | Complete | decisions/0008-visual-occlusion-vs-physical-blocking-layers.md |
| P5-12 | Add authored visual-only wall data fields and illusory-wall acceptance coverage | P5-11 | High | Complete | decisions/0008-visual-occlusion-vs-physical-blocking-layers.md |
| P5-13 | Switch map physical layer to collision bitmask fields and route occupancy/collision builds through bit checks | P5-12 | High | Complete | decisions/0008-visual-occlusion-vs-physical-blocking-layers.md |
| P5-14 | Move map-authored non-mesh entities to a dedicated entity placement table and spawn runtime visuals from that table | P5-13 | High | Complete | architecture/world-scene-structure.md |
| P5-15 | Add explicit solid/non-solid and movable/non-movable flags to entity placements | P5-14 | High | Complete | decisions/0008-visual-occlusion-vs-physical-blocking-layers.md |
| P5-16 | Merge non-movable solid entities into static occupancy/wall-collision data while keeping movable solids dynamic | P5-15 | High | Complete | architecture/world-scene-structure.md |
| P5-17 | Enforce strict table-based handcrafted entity authoring and reject legacy in-grid entity symbols | P5-16 | High | Complete | architecture/world-scene-structure.md |

### Known Limitations — Post P5

1. Previously listed Post-P5 rendering limitations were addressed by P5-06, P5-07, and subsequent P5-13 through P5-17 layering/collision improvements.
2. Remaining rendering and readability improvements are tracked in [docs/execution/next-steps.md](next-steps.md).

## Phase 6 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P6-01 | Implement room/corridor generation algorithm | P5-01 | High | Complete | plan/development-phases.md |
| P6-02 | Add key-lock and switch-door generator constraints | P6-01 | High | Complete | research/research-tracks.md |
| P6-03 | Add prefab insertion support for complex scenes | P6-01 | Medium | Complete | research/research-tracks.md |
| P6-04 | Build validation pass for reachability and solvability | P6-01, P6-02 | High | Complete | quality/testing-strategy.md |

## Phase 7 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P7-01 | Create scripted NPC behavior interface | P4-02 | High | Not Started | plan/development-phases.md |
| P7-02 | Implement investigate and patrol behavior | P7-01, P3-02 | Medium | Not Started | plan/development-phases.md |
| P7-03 | Implement chase and disengage behavior | P7-01, P3-01 | Medium | Not Started | plan/development-phases.md |
| P7-04 | Expand encounter and item content sets | P7-02, P7-03 | Medium | Not Started | plan/basic-engine-release.md |

## Phase 8 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P8-01 | Implement content validators and authoring helpers | P6-04 | Medium | Not Started | plan/development-phases.md |
| P8-02 | Implement robust save/load and migration checks | P4-03 | High | Not Started | quality/testing-strategy.md |
| P8-03 | Establish performance budget checks and profiling runs | P1-01 | High | Not Started | quality/testing-strategy.md |
| P8-04 | Build regression suite and release candidate checklist | P8-01, P8-02, P8-03 | High | Not Started | plan/basic-engine-release.md |

## Dependency Rules
1. Do not start High priority tasks with unresolved High dependencies.
2. Re-evaluate dependencies at each phase transition.
3. If a dependency shifts, update this file and the milestone tracker in the same change.
