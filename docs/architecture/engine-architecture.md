# Engine Architecture

## Purpose
Define the initial engine layering and ownership boundaries to keep gameplay iteration safe and modular.

## Layered System Model
1. Platform Layer
2. Core Runtime Layer
3. Rendering Layer
4. Audio Layer
5. Simulation Layer
6. Gameplay Rules Layer
7. UI/HUD Layer
8. Content/Tools Layer

## Layer Responsibilities

### 1) Platform Layer
Responsibilities:
1. Window creation and input abstraction.
2. Filesystem and path abstraction.
3. Timing services.
4. Threading primitives.

Current status:
1. Initial input action-binding layer is in place, mapping physical keys to engine actions for camera controls.

Constraints:
1. Linux-first implementation with portable interfaces.
2. No OS-specific includes outside this layer.

### 2) Core Runtime Layer
Responsibilities:
1. Logging and assertion framework.
2. Profiling hooks and runtime metrics.
3. Resource handles and lifetime ownership.
4. Event system for low-coupling communication.

Current baseline:
1. Lightweight entity/component storage is in place for actors, interactables, transforms, colliders, inventories, keyrings, switch links, and occupancy data.
2. A CTest-backed unit-test target covers deterministic interaction, key/switch, and occupancy invariants.

Implementation direction:
1. Use a simple entity-component model first.
2. Favor data-driven configuration over hard-coded behavior.

### 3) Rendering Layer
Responsibilities:
1. OpenGL backend and draw pipeline.
2. Isometric camera control.
3. Mesh/material loading and rendering.
4. Debug rendering overlays for development.
5. View clipping against scene bounds and render submission from scene-query results.

Current baseline:
1. OpenGL 4.1 core profile context (decision 0002).
2. GLFW-based window/context bootstrap (decision 0004).
3. Temporary debug map draw path implemented as scissor-constrained clear regions until shader/VBO pipeline lands.
4. Camera movement consumes input actions from the platform input binding layer rather than hard-coded key checks.
5. A handcrafted dungeon test map is loaded from an ASCII asset and rendered with collision-state debug overlays.

Ordering note:
1. Ship stable world rendering before advanced visual effects.

### 4) Audio Layer
Responsibilities:
1. Music and effects playback.
2. Positional cues when needed.
3. Sound events tied to simulation events.

Ordering note:
1. Start minimal and expand after core gameplay loop is operational.

### 5) Simulation Layer
Responsibilities:
1. Deterministic simulation tick.
2. Spatial and collision queries.
3. Visibility and perception systems.
4. Fog-of-war state management.
5. Authoritative blocking and occupancy state separate from renderer-oriented scene hierarchy.

Current baseline:
1. Simulation-owned occupancy grid derives static blocking from the handcrafted map.
2. Dynamic blocking is applied from interactable state.
3. Actor occupancy is tracked separately from blocking and can be queried per tile.
4. Baseline line-of-sight and occlusion queries are implemented over simulation-owned occupancy state, including blocked-target and blocked-corner handling.
5. Deterministic visibility unit tests now cover clear, blocked, blocked-target, and blocked-corner LOS cases.
6. Directional hearing primitives evaluate audibility from source/listener tiles with deterministic distance falloff, facing bias, and occupancy-based occlusion attenuation.

Ordering note:
1. Deterministic behavior is a hard requirement for reliable testing.

### 6) Gameplay Rules Layer
Responsibilities:
1. Rules for doors, chests, traps, keys, and switches.
2. Character ability driven action resolution.
3. Inventory operations and item usage semantics.
4. Party control model for group and individual commands.

Current baseline:
1. Deterministic state machines exist for door, chest/container, trap, and switch interactions.
2. Key ownership, unlock checks, and switch-to-target linkage rules are implemented.

Ordering note:
1. Stabilize rule contracts early to reduce UI and AI churn.

### 7) UI/HUD Layer
Responsibilities:
1. Party state and command interfaces.
2. Inventory and context interactions.
3. Feedback for hidden-information systems (sound cues, detection state).

Ordering note:
1. Use practical debug-first UI, then evolve presentation quality.

### 8) Content/Tools Layer
Responsibilities:
1. Data schemas for world and gameplay content.
2. Validation tooling for schema and rule consistency.
3. Editor workflows after schema maturity.

Ordering note:
1. Keep content authoring text-based until data formats stabilize.

## World Scene Structure
The world runtime is split between authored map content, scene-node state, and simulation-owned spatial state.

Baseline rules:
1. Runtime scene data uses stable `scene_node_id` handles.
2. Scene hierarchy organizes world geometry, attachment anchors, and dynamic objects.
3. A loose octree over node bounds is the broadphase structure for clipping and picking.
4. Blocking and occupancy remain authoritative in simulation-managed spatial records rather than the scene graph.
5. Actor and interactable schemas in Phase 2 must attach to this structure instead of inventing their own world references.

See [World Scene Structure](world-scene-structure.md) for the detailed contract.

## Cross-Cutting Standards
1. Any system with side effects should emit traceable events.
2. Systems should expose deterministic, test-friendly interfaces.
3. Debug visualization is required for visibility, navigation, and generation systems.
