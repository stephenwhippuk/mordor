# Basic Engine Release Definition

## Purpose
Define the minimum complete engine product that is useful and extensible.

## Scope (Must Have)
1. Portable runtime architecture (implemented Linux-first).
2. Isometric 3D rendering and camera controls.
3. Playable sandbox level with one party and scripted enemies.
4. Working world interactions: doors, chests, traps, keys, switches, and containers.
5. Visibility stack: line-of-sight, hearing cues, and fog of war.
6. Inventory and party command UI sufficient for core loop.
7. Procedural generation prototype with solvability validation.
8. Automated tests for core simulation and generation invariants.

## Out of Scope (For This Release)
1. Full-featured content editor GUI.
2. Large narrative content and deep quest systems.
3. Advanced visual effects polish.
4. Complex multiplayer/networking features.

## Acceptance Checklist
1. A new build can run on a clean machine with documented steps.
2. Core loop is playable without manual debug commands.
3. Generated dungeon runs pass validation checks.
4. No critical crash or progression blocker in a 30-minute playtest.
5. Test suite passes in CI for release candidate branch.
