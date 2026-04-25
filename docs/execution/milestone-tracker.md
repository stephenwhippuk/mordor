# Milestone Tracker

## Purpose
Track delivery progress at milestone level with clear entry and completion criteria.

## Status Legend
1. Not Started
2. In Progress
3. Blocked
4. Complete

## Milestones

| Milestone | Scope | Target Window | Status | Entry Criteria | Exit Criteria |
|---|---|---|---|---|---|
| M0: Foundation Baseline | Build, runtime diagnostics, standards, first decisions | Weeks 1-2 | In Progress | Repository initialized and team conventions agreed | Engine boots to empty scene with timing/debug output |
| M1: Rendered Sandbox | Main loop, camera, input, map rendering | Weeks 3-4 | Not Started | M0 complete | Hand-authored test dungeon renders with collision overlays |
| M2: Core Interactions | Actor movement, doors, chests, traps, keys/switches | Weeks 5-7 | Not Started | M1 complete | Single actor can complete interaction sandbox loop |
| M3: Visibility and Perception | LOS, hearing, fog of war, debug overlays | Weeks 8-10 | Not Started | M2 complete | Perception drives targeting and visible world state |
| M4: Party RPG Loop | Party commands, abilities, inventory, HUD baseline | Weeks 11-14 | Not Started | M3 complete | Exploration to loot loop is fully playable |
| M5: Generation v1 | Rule generation, prefabs, solvability validation | Weeks 15-18 | Not Started | M4 complete | Generated dungeons pass validation and are completable |
| M6: AI and Content Expansion | Scripted behavior and content breadth | Weeks 19-22 | Not Started | M5 complete | Stable multi-encounter sessions without blockers |
| M7: Hardening and Release Prep | Save/load reliability, performance, regressions | Weeks 23-26 | Not Started | M6 complete | Basic engine release criteria fully met |

## Review Rhythm
1. Update status weekly.
2. Reconfirm target windows at each milestone boundary.
3. Record blockers with owner and mitigation in the risk register.

## Reporting Format
Use this format for milestone updates:
1. Status
2. What changed this week
3. Blockers
4. Next checkpoint
