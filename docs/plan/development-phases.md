# Development Phases

## Goal
Define a delivery order that prioritizes engine correctness and a playable baseline before scale-up.

## Phase Plan

### Phase 0: Foundation and Decision Gate
Deliverables:
1. Build system and repository baseline.
2. Coding standards and static analysis setup.
3. Logging/assertion/profiling hooks.
4. Initial technical decisions recorded.

Exit criteria:
1. Engine boots into an empty scene and renders frame timing/debug text.

### Phase 1: Engine Skeleton and World Rendering MVP
Deliverables:
1. Fixed simulation step and decoupled render loop.
2. OpenGL world renderer for dungeon geometry and entities.
3. Isometric camera movement controls.
4. Input binding layer.

Exit criteria:
1. Hand-authored dungeon map loads and renders with collision debug overlays.

### Phase 2: Core World Simulation
Deliverables:
1. Components for actors and interactables.
2. State machines for door/chest/trap interactions.
3. Spatial blocking and occupancy logic.

Exit criteria:
1. One controllable actor can traverse and interact with world mechanics.

### Phase 3: Perception and Visibility
Deliverables:
1. Line-of-sight and occlusion.
2. Directional hearing events with strength/confidence.
3. Fog-of-war for explored and visible state.
4. Stubs for advanced senses.

Exit criteria:
1. Perception changes both gameplay targeting and rendered information.

### Phase 4: RPG Loop and Party Control
Deliverables:
1. Party selection and command issuing.
2. Ability-driven action resolution.
3. Inventory model and item interactions.
4. Baseline HUD for commands and status.

Exit criteria:
1. Complete exploration-interaction-loot loop is playable.

### Phase 5: Dungeon Generation v1
Deliverables:
1. Rule-based room and corridor generator.
2. Key-lock and switch-door constraint support.
3. Prefab insertion support for complex set pieces.
4. Reachability/solvability validation pass.

Exit criteria:
1. Generated dungeons are mechanically coherent and completable.

### Phase 6: AI and Content Expansion
Deliverables:
1. Scripted NPC behavior framework.
2. Investigate/patrol/chase/disengage tactical behavior.
3. Expanded encounter and item content.

Exit criteria:
1. Stable session with multiple encounter patterns and no progression blockers.

### Phase 7: Tooling and Hardening
Deliverables:
1. Authoring helpers and content validators.
2. Save/load reliability pass.
3. Performance and memory stabilization.
4. Regression suite and release checklist.

Exit criteria:
1. Engine is stable as a reusable base for additional projects.

## Sequencing Principles
1. Prioritize deterministic simulation before large feature breadth.
2. Lock interaction primitives before procedural complexity.
3. Add sophisticated AI only after perception is robust.
4. Delay rich editor development until schemas are stable.
