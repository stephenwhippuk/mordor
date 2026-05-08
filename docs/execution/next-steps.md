# Immediate Next Steps (2-3 Weeks)

## Goal
Start Phase 5 implementation by turning current command/ability/inventory scaffolding into generation-ready systems.

## Work Items
1. Implement room/corridor generation algorithm baseline.
2. Add generator constraints for key-lock and switch-door progression paths.
3. Extend integration coverage across party commands, abilities, inventory item use, and HUD surfaces before generation iteration.

## Success Criteria
1. Generated dungeon layouts produce deterministic room/corridor topology for fixed seeds.
2. Generated progression gates preserve solvable key-lock and switch-door paths.
3. Existing command/ability/inventory/HUD baselines remain green under expanded test coverage.

## Follow-On
1. Add prefab insertion and full solvability validation after generator baseline stabilizes.
