# Immediate Next Steps (2-3 Weeks)

## Goal
Turn the completed Phase 2 simulation baseline into the first usable perception stack for Phase 3.

## Work Items
1. Implement line-of-sight and occlusion checks over the current occupancy and scene-query structures.
2. Add directional hearing event primitives tied to simulation-side events.
3. Build explored-versus-visible fog-of-war state.
4. Add debug overlays for LOS rays, audible events, and visible/explored cells.
5. Expand the unit-test suite to cover occupancy, interaction sequences, and first perception invariants.

## Success Criteria
1. Perception queries consume current simulation blocking and occupancy state rather than ad hoc debug logic.
2. The renderer can visualize visible/explored state and LOS debug information from simulation-owned results.
3. New perception and interaction regressions are covered by runnable automated tests.

## Follow-On
1. Start party command and ability pipeline work once perception results can drive targeting and visibility.
