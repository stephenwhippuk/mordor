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

Entry gate:
1. World scene structure baseline accepted so actor and interactable data can attach to stable scene nodes and query services.

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

### Phase 5: Rendering Engine and Interaction Readability
Deliverables:
1. Shader/VBO-based 3D map rendering path for walls and floors.
2. Tunable isometric-perspective camera rig.
3. Mouse-driven tile/entity selection and picking.
4. Occlusion-aware wall fade/hide strategy for interaction readability.
5. Frustum-culling and render submission metrics.

Exit criteria:
1. Player can reliably select intended targets in 3D scenes without foreground wall occlusion blocking interaction readability.

### Phase 6: Dungeon Generation v1
Deliverables:
1. Rule-based room and corridor generator.
2. Key-lock and switch-door constraint support.
3. Prefab insertion support for complex set pieces.
4. Reachability/solvability validation pass.

Exit criteria:
1. Generated dungeons are mechanically coherent and completable.

### Phase 7: AI and Content Expansion
Deliverables:
1. Scripted NPC behavior framework.
2. Investigate/patrol/chase/disengage tactical behavior.
3. Expanded encounter and item content.

Exit criteria:
1. Stable session with multiple encounter patterns and no progression blockers.

### Phase 8: Tooling and Hardening
Deliverables:
1. Authoring helpers and content validators.
2. Save/load reliability pass.
3. Performance and memory stabilization.
4. Regression suite and release checklist.

Exit criteria:
1. Engine is stable as a reusable base for additional projects.

## Sequencing Principles
1. Prioritize deterministic simulation before large feature breadth.
2. Lock world attachment and broadphase query structure before actor schema work.
3. Add sophisticated AI only after perception is robust.
4. Delay rich editor development until schemas are stable.
5. Lock interaction primitives before procedural complexity.
