# Immediate Next Steps (2-3 Weeks)

## Goal
Advance Phase 6 generation baseline from deterministic room/corridor layout to constrained, validated dungeon generation.

## Work Items
1. Extend generator output with key-lock and switch-door placement constraints.
2. Add prefab insertion hooks for authored set-piece rooms.
3. Add reachability and solvability validation pass for generated outputs.

## Success Criteria
1. Generated maps are deterministic for fixed seeds and satisfy baseline room/corridor connectivity.
2. Key progression constraints can be generated without producing dead-end progression states.
3. Validation pass rejects unsolved or disconnected outputs with actionable failure reasons.
4. Existing simulation and rendering baselines remain green while generation features land.

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
