# 0003: Initial Runtime Data Model Strategy

Status: Accepted  
Date: 2026-04-25

## Context
Phase 2 depends on a stable representation for actors and interactables (doors, traps, chests, keys, switches). We need a minimal model that supports deterministic simulation and can be evolved into tooling later.

## Options Considered
1. Pure object-oriented hierarchy (`Entity` base class + subclasses)
2. Full ECS framework dependency immediately
3. Lightweight component-data model with engine-owned storage

## Decision
Use a **lightweight component-data model** implemented in engine code.

## Rationale
1. Matches the architecture goal of data-driven systems without early framework lock-in.
2. Keeps deterministic simulation easier to reason about (explicit update order and data ownership).
3. Avoids up-front integration complexity before gameplay invariants are known.

## Data Rules (Initial)
1. Entities are opaque integer handles (`entity_id`).
2. Components are POD-style data records stored in contiguous arrays or vectors.
3. Systems operate over component sets in deterministic order.
4. Runtime data must not depend on wall-clock time.
5. Content definitions are text-based files validated at load time.

## Consequences
1. Positive: low complexity startup with clear migration path to richer ECS patterns.
2. Positive: predictable memory and update behavior for profiling and testing.
3. Tradeoff: some boilerplate for component storage and query utilities.

## Follow-Up Actions
1. Define initial component schemas for actor, transform, collider, inventory, and interactable state.
2. Add serialization format draft for content definitions.
3. Add validation rules for state-machine backed interactables.
