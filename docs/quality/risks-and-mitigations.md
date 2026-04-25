# Risks and Mitigations

## Purpose
Track current delivery risks and explicit mitigations.

## Active Risks

### 1) Early Over-Engineering
Impact:
1. Delayed playable progress and high rewrite cost.

Mitigation:
1. Enforce phase exit criteria.
2. Defer low-value abstractions until needed by two or more systems.

### 2) Unsolvable Procedural Dungeons
Impact:
1. Broken progression and unstable player experience.

Mitigation:
1. Add generation validation rules before content scale-up.
2. Block generation outputs that fail reachability/solvability tests.

### 3) Weak Perception Model Limits AI Quality
Impact:
1. AI behavior appears inconsistent or unrealistic.

Mitigation:
1. Build and instrument perception primitives before tactical AI expansion.
2. Add debug overlays for line-of-sight and hearing events.

### 4) Tooling Scope Creep
Impact:
1. Time diverted from core engine functionality.

Mitigation:
1. Keep authoring text-based during MVP.
2. Build editor features only after schema and workflows stabilize.

### 5) Performance Regression During Feature Growth
Impact:
1. Frame drops and unstable simulation cadence.

Mitigation:
1. Add routine profiling and performance budgets per phase.
2. Fail pre-release checks when budgets are exceeded.

## Review Cadence
1. Review this file at each phase boundary.
2. Add owner and status tags once team roles are formalized.
