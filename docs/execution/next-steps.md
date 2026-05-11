# Immediate Next Steps (2-3 Weeks)

## Goal
Integrate generated dungeon outputs into runtime systems and begin Phase 7 AI/content expansion groundwork.

## Work Items
1. Extend generated key/switch/door runtime handling from visualization into interactable gameplay placement/state transitions. (Complete)
2. Bind generated prefab placement metadata into runtime scene/entity spawning. (Complete)
3. Define and implement initial scripted NPC behavior interface (P7-01).

## Success Criteria
1. Generated map metadata (constraints + prefab placements) is consumed by runtime scene/interactable spawn paths. (Complete)
2. Runtime can boot from generated map path with deterministic outputs for fixed seed. (Complete)
3. Scripted NPC behavior interface is in place with stable contracts for follow-on behavior tasks.
4. Existing simulation and rendering baselines remain green while integration and AI scaffolding land.

## Follow-On
1. Add seed/config surface (CLI or config file) so generated map bootstrap can be varied without code edits.

## Carry-Over Rendering Issues (From Quick Playtest)
1. Prior wall over-generation and occlusion-anchor ambiguity findings were addressed in the P5 rendering completion sequence and subsequent layering/collision follow-ups.
2. This section is retained as historical context; active rendering follow-up tasks are listed below under Follow-Up Actions.

## Occlusion Behavior Contract (Required)
1. Use active character (or selected party member) world position as the occlusion reference anchor.
2. Fade only occluders between camera and anchor that materially block character readability.
3. Do not fade walls behind the character relative to camera view direction.
4. Preserve full map detail outside the occlusion corridor unless another gameplay system (for example fog-of-war or line-of-sight) intentionally limits visibility.
5. Apply a radius gate around the actor-centric occlusion corridor: geometry outside the configured occlusion radius is ignored for fade decisions.
6. Expected normal case: only one to two wall segments fade at once; higher counts are acceptable primarily at low camera pitch where southern foreground stacks can overlap the actor.

## Follow-Up Actions
1. Integrate wall-octree collision and scene spatial queries into broader visibility helper paths beyond the current player movement path.
2. Generalize camera-to-target occlusion ray helper for non-wall visibility checks (keys, switches, items, NPCs) using the same query model now that runtime visuals are separate scene nodes.
3. Add debug telemetry that logs camera position, active party tile/world position, and count of occlusion-faded wall surfaces each sample interval.
4. Add acceptance checks: rotate camera around stationary actor and verify that only foreground blockers fade while background walls remain opaque.
5. Add click-to-move probe placement so testing can reposition the active player anchor from mouse picks.
