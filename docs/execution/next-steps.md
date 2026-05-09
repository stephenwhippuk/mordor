# Immediate Next Steps (2-3 Weeks)

## Goal
Integrate generated dungeon outputs into runtime systems and begin Phase 7 AI/content expansion groundwork.

## Work Items
1. Bind generated key/switch/door metadata into runtime interactable placement.
2. Bind generated prefab placement metadata into runtime scene/entity spawning.
3. Define and implement initial scripted NPC behavior interface (P7-01).

## Success Criteria
1. Generated map metadata (constraints + prefab placements) is consumed by runtime scene/interactable spawn paths.
2. Runtime can boot from generated map path with deterministic outputs for fixed seed.
3. Scripted NPC behavior interface is in place with stable contracts for follow-on behavior tasks.
4. Existing simulation and rendering baselines remain green while integration and AI scaffolding land.

## Follow-On
1. Start integrating generated maps into runtime bootstrap as an optional loading path.

## Carry-Over Rendering Issues (From Quick Playtest)
1. Wall mesh over-generation: world mesh currently emits full wall side faces per blocked tile, including interior faces between adjacent wall tiles. This creates unnecessary polygons and becomes visible during occlusion fades.
2. Occlusion anchor ambiguity: wall fade currently appears camera-position driven rather than clearly actor/party-position driven. Actor location and whether camera motion is coupled to actor state are not yet explicit in runtime diagnostics.

## Occlusion Behavior Contract (Required)
1. Use active character (or selected party member) world position as the occlusion reference anchor.
2. Fade only occluders between camera and anchor that materially block character readability.
3. Do not fade walls behind the character relative to camera view direction.
4. Preserve full map detail outside the occlusion corridor unless another gameplay system (for example fog-of-war or line-of-sight) intentionally limits visibility.
5. Apply a radius gate around the actor-centric occlusion corridor: geometry outside the configured occlusion radius is ignored for fade decisions.
6. Expected normal case: only one to two wall segments fade at once; higher counts are acceptable primarily at low camera pitch where southern foreground stacks can overlap the actor.

## Follow-Up Actions
1. Add a mesh optimization pass that suppresses interior wall faces shared by neighboring blocked tiles.
2. Define and document occlusion source-of-truth as active party position and update fade calculation to test camera-to-actor occluders only.
3. Add debug telemetry that logs camera position, active party tile/world position, and count of occlusion-faded tiles each sample interval.
4. Add acceptance checks: rotate camera around stationary actor and verify that only foreground blockers fade while background walls remain opaque.
5. Add acceptance checks for radius behavior: move actor and camera so candidate walls are inside/outside radius and verify outside-radius walls never fade.
