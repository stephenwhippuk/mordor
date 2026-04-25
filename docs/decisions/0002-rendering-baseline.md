# 0002: Rendering Baseline

Status: Accepted  
Date: 2026-04-25

## Context
Phase 1 requires renderer bootstrap and world draw path. We need an explicit baseline for OpenGL capabilities so shader/material decisions are consistent across Linux, Windows, and macOS.

## Options Considered
1. OpenGL 3.3 Core Profile
2. OpenGL 4.1 Core Profile
3. OpenGL 4.5 Core Profile

## Decision
Adopt **OpenGL 4.1 Core Profile** as the engine baseline.

## Rationale
1. Portable across modern Linux and Windows GPU drivers.
2. Aligns with macOS maximum supported core profile, preserving future portability.
3. Provides sufficient feature set for isometric RPG renderer (UBOs, instancing, framebuffer workflows) without 4.5-only requirements.

## Consequences
1. Positive: one cross-platform baseline for shader and renderer implementation.
2. Positive: avoids dependency on newer extensions not guaranteed on all targets.
3. Tradeoff: no hard dependency on OpenGL 4.5 features (direct state access, etc.) in core path.

## Follow-Up Actions
1. Add context creation guard that fails with a clear error if runtime OpenGL < 4.1.
2. Keep any optional 4.5+ optimizations behind capability checks.
3. Reference this decision in Phase 1 rendering tasks.
