# Immediate Next Steps (2-3 Weeks)

## Goal
Start Phase 4 implementation by turning perception outputs into the playable party command loop.

## Work Items
1. Implement party selection and command issuing baseline.
2. Implement ability-driven action resolution pipeline scaffolding.
3. Implement inventory model and map-targeted item use hooks.
4. Add baseline HUD surfaces for command and party status flow.

## Success Criteria
1. A selected party actor can issue movement/interaction commands through deterministic simulation paths.
2. Ability and inventory actions can target map entities without bypassing the simulation pipeline.
3. HUD command/status feedback reflects command outcomes without requiring manual debug logs.

## Follow-On
1. Expand integration coverage for command, perception, and inventory interactions before Phase 5 generation work.
