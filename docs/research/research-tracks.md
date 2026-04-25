# Research Tracks

## Purpose
Track open technical decisions and ensure each investigation ends with an explicit decision artifact.

## Track List

### 1) Rendering Stack Details
Questions:
1. Which OpenGL version baseline balances portability and features?
2. Which math and utility libraries should be adopted?

Outputs:
1. Prototype scene benchmark.
2. Decision record with selected baseline and fallback plan.

### 2) Audio Middleware
Questions:
1. Which audio library fits the project constraints and licensing model?
2. How should positional and event-driven audio be integrated?

Outputs:
1. Playback + positional proof-of-concept.
2. Decision record with integration complexity assessment.

### 3) Data Serialization and Save Format
Questions:
1. Text-first data format for authoring and review.
2. Save-game format strategy for versioning/migration.

Outputs:
1. Example content schema.
2. Save/load round-trip validation prototype.

### 4) Scripting Strategy for AI and Content
Questions:
1. Native scripts only or embedded scripting language?
2. How are behavior scripts tested and hot-reloaded?

Outputs:
1. Prototype behavior script execution path.
2. Decision record with performance and maintenance tradeoffs.

### 5) Procedural Generation Algorithms
Questions:
1. Which generation approach best supports puzzle constraints?
2. How should prefab insertion and rule generation interoperate?

Outputs:
1. Generator prototype with validation metrics.
2. Failure-case catalog and mitigation strategy.

### 6) Pathfinding and Navigation
Questions:
1. Grid vs navmesh for isometric dungeon gameplay.
2. Support for dynamic blockers and interaction states.

Outputs:
1. Navigation performance test under dynamic updates.
2. Decision record with complexity and runtime implications.

### 7) Asset Pipeline Standards
Questions:
1. Preferred mesh/animation formats and import tooling.
2. Naming/versioning conventions for asset stability.

Outputs:
1. Minimal import/export pipeline test.
2. Validation checklist for incoming content.

## Research Workflow
1. Time-box each track to 1-2 weeks.
2. Define evaluation criteria before prototyping.
3. Publish result in `docs/decisions`.
4. If undecided, record blockers and next probe.
