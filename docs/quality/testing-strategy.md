# Testing Strategy

## Goal
Protect deterministic behavior, prevent regressions, and enforce generation correctness.

## Test Pyramid

### Unit Tests
Scope:
1. Interaction state machines for doors, chests, and traps.
2. Inventory operations and rule checks.
3. Math and geometry helpers used by simulation and perception.

Expected value:
1. Fast confidence during local development.

### Integration and Simulation Tests
Scope:
1. Movement, collision, and occupancy interactions.
2. Perception events influencing AI behavior.
3. Party commands and action resolution flow.

Expected value:
1. Catch contract breaks between systems.

### Property-Based and Fuzz Tests
Scope:
1. Dungeon generation invariants (reachability, solvability).
2. Save/load idempotence and migration assumptions.
3. Randomized interaction sequence safety.

Expected value:
1. Surface edge cases not represented in curated tests.

### Golden Scenario Tests
Scope:
1. Scripted critical gameplay scenarios.
2. Baseline examples: key-door progression and trap-aware traversal.

Expected value:
1. Long-term protection for player-facing mechanics.

### Performance and Stability Tests
Scope:
1. Frame time under increased entity counts.
2. Memory growth over long sessions.
3. Profiling of visibility and generation hotspots.

Expected value:
1. Prevent quality collapse as feature count grows.

## Test Operations
1. Run unit and integration suites on each pull request.
2. Run property/fuzz and performance suites on scheduled jobs.
3. Block merges on failed deterministic simulation tests.
4. Keep test fixtures versioned with schema changes.

## Current Baseline
1. Unit test target: `mordor_unit_tests`.
2. Run command: `ctest --test-dir build --output-on-failure`.
3. Current coverage: interaction state transitions, key/switch link behavior, occupancy/blocking invariants, baseline line-of-sight/occlusion checks, directional hearing invariants, fog-of-war visible/explored state transitions, and perception debug overlay payload generation.
