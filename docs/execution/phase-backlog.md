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
| P3-01 | Implement line-of-sight and occlusion checks | P2-04 | High | Not Started | plan/development-phases.md |
| P3-02 | Add directional hearing event system | P2-01 | Medium | Not Started | architecture/engine-architecture.md |
| P3-03 | Build fog-of-war explored and visible map state | P3-01 | High | Not Started | plan/basic-engine-release.md |
| P3-04 | Add perception debug overlays | P3-01, P3-02, P3-03 | Medium | Not Started | quality/testing-strategy.md |

## Phase 4 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P4-01 | Implement party selection and command issuing | P3-03 | High | Not Started | plan/development-phases.md |
| P4-02 | Implement ability-driven action resolution pipeline | P2-02, P4-01 | High | Not Started | plan/development-phases.md |
| P4-03 | Implement inventory model and map-targeted item use | P2-03, P4-01 | High | Not Started | plan/basic-engine-release.md |
| P4-04 | Implement baseline HUD for party and inventory | P4-01, P4-03 | Medium | Not Started | architecture/engine-architecture.md |

## Phase 5 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P5-01 | Implement room/corridor generation algorithm | P4-02 | High | Not Started | plan/development-phases.md |
| P5-02 | Add key-lock and switch-door generator constraints | P5-01 | High | Not Started | research/research-tracks.md |
| P5-03 | Add prefab insertion support for complex scenes | P5-01 | Medium | Not Started | research/research-tracks.md |
| P5-04 | Build validation pass for reachability and solvability | P5-01, P5-02 | High | Not Started | quality/testing-strategy.md |

## Phase 6 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P6-01 | Create scripted NPC behavior interface | P4-02 | High | Not Started | plan/development-phases.md |
| P6-02 | Implement investigate and patrol behavior | P6-01, P3-02 | Medium | Not Started | plan/development-phases.md |
| P6-03 | Implement chase and disengage behavior | P6-01, P3-01 | Medium | Not Started | plan/development-phases.md |
| P6-04 | Expand encounter and item content sets | P6-02, P6-03 | Medium | Not Started | plan/basic-engine-release.md |

## Phase 7 Backlog

| Task ID | Description | Depends On | Priority | Status | Deliverable Link |
|---|---|---|---|---|---|
| P7-01 | Implement content validators and authoring helpers | P5-04 | Medium | Not Started | plan/development-phases.md |
| P7-02 | Implement robust save/load and migration checks | P4-03 | High | Not Started | quality/testing-strategy.md |
| P7-03 | Establish performance budget checks and profiling runs | P1-01 | High | Not Started | quality/testing-strategy.md |
| P7-04 | Build regression suite and release candidate checklist | P7-01, P7-02, P7-03 | High | Not Started | plan/basic-engine-release.md |

## Dependency Rules
1. Do not start High priority tasks with unresolved High dependencies.
2. Re-evaluate dependencies at each phase transition.
3. If a dependency shifts, update this file and the milestone tracker in the same change.
