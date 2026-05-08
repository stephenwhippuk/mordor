# Immediate Next Steps (2-3 Weeks)

## Goal
Extend the newly established Phase 3 LOS baseline into a usable multi-sense perception stack.

## Work Items
1. Add directional hearing event primitives tied to simulation-side events.
2. Build explored-versus-visible fog-of-war state.
3. Add debug overlays for LOS rays, audible events, and visible/explored cells.
4. Expand the unit-test suite to cover perception invariants across LOS, hearing, and fog-of-war state transitions.

## Success Criteria
1. LOS, hearing, and fog-of-war queries consume simulation-owned blocking and occupancy state rather than ad hoc debug logic.
2. The renderer can visualize visible/explored state and perception debug information from simulation-owned results.
3. New perception and interaction regressions are covered by runnable automated tests.

## Follow-On
1. Start party command and ability pipeline work once perception results can drive targeting and visibility.
