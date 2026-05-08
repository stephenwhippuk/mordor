# Immediate Next Steps (2-3 Weeks)

## Goal
Finalize the remaining perception deliverables and expose them through debug rendering workflows.

## Work Items
1. Add debug overlays for LOS rays, audible events, and visible/explored cells.
2. Expand the unit-test suite to cover richer perception invariants across LOS, hearing, and fog-of-war state transitions.

## Success Criteria
1. LOS, hearing, and fog-of-war queries consume simulation-owned blocking and occupancy state rather than ad hoc debug logic.
2. The renderer can visualize visible/explored state and perception debug information from simulation-owned results.
3. New perception and interaction regressions are covered by runnable automated tests.

## Follow-On
1. Start party command and ability pipeline work once perception results can drive targeting and visibility.
