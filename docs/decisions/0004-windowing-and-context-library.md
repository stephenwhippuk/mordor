# 0004: Windowing and OpenGL Context Library

Status: Accepted  
Date: 2026-04-25

## Context
Phase 1 requires a renderer bootstrap with an actual window, input event pump, and OpenGL context creation aligned to decision 0002 (OpenGL 4.1 core baseline).

## Options Considered
1. GLFW
2. SDL2
3. Native platform APIs per OS

## Decision
Use **GLFW** as the initial windowing and OpenGL context library.

## Rationale
1. Minimal API surface and straightforward CMake integration.
2. Cross-platform support with well-understood OpenGL context hints.
3. Good fit for an engine bootstrap phase where rendering/input primitives are needed quickly.

## Consequences
1. Positive: rapid renderer startup and stable event loop integration.
2. Positive: clear path to later platform-layer abstraction.
3. Tradeoff: introduces external dependency in source build.

## Follow-Up Actions
1. Keep GLFW usage confined to renderer/platform boundary.
2. Revisit abstraction once input mapping and camera systems are implemented.
3. If requirements shift toward richer platform integration, re-evaluate SDL2.
