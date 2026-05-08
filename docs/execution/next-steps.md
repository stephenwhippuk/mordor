# Immediate Next Steps (2-3 Weeks)

## Goal
Start Phase 4 implementation by turning perception outputs into the playable party command loop.

## Work Items
1. Implement ability-driven action resolution pipeline scaffolding.
2. Implement inventory model and map-targeted item use hooks.
3. Add baseline HUD surfaces for command and party status flow.
4. Keep command/ability semantics abstract enough to support iterative game-system design.

## Success Criteria
1. A selected party actor can issue deterministic command intents through the simulation command pipeline.
2. Ability and inventory actions can target map entities without bypassing the simulation pipeline.
3. HUD command/status feedback reflects command outcomes without requiring manual debug logs.

## Follow-On
1. Expand integration coverage for command, perception, and inventory interactions before Phase 5 generation work.
