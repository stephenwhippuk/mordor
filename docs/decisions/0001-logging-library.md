# 0001: Logging Library Selection

Status: Accepted
Date: 2026-04-25

## Context
Phase 0 requires a logging and diagnostics foundation. The simulation/game loop has strict frame-time budget requirements; any logging call on the hot path must not cause frame spikes regardless of log volume. The engine is also required not to read wall-clock time directly in simulation code.

## Options Considered

### Option A: Roll-your-own ring-buffer logger
Pros: zero external dependency, fully controllable.  
Cons: non-trivial SPSC queue and crash-safe flush implementation; maintenance cost before engine features exist.

### Option B: spdlog (MIT, header-only or compiled)
Pros: massively adopted, easy CMake integration, good documentation.  
Cons: formatting occurs on the caller thread. Benchmarked hot-path latency is ~145 ns at p50 single-thread and degrades to ~214–754 ns under 4 threads. Too high for a tight simulation tick.

### Option C: quill (MIT, header-only embed)
Pros: deferred formatting — only a binary copy of arguments is pushed onto a per-thread SPSC queue on the caller thread; all formatting happens on a background thread. Benchmarked hot-path latency is ~8–9 ns at p50/p99 single and multi-thread. Supports a custom clock source, which is required for deterministic simulation replay. Provides compile-time level elimination for zero-cost trace/debug calls in release builds. Backtrace ring-buffer for crash capture.  
Cons: smaller community than spdlog; slightly more involved setup.

## Decision
Use **quill** via CMake FetchContent.

Rationale:
1. Hot-path latency is ~17× lower than spdlog.
2. Custom clock support satisfies the deterministic simulation clock requirement.
3. MIT license is compatible.

## Defensive Strategy
Engine code must **never** include quill headers directly. All logging is accessed exclusively via `mordor/log.hpp` which exposes `MORDOR_LOG_*` macros and `mordor::log::init/shutdown`. If a switch to spdlog (or any other backend) is required, only `source/include/mordor/log.hpp` and `source/src/log.cpp` require changes.

## Consequences
1. Positive: near-zero hot-path overhead; deterministic clock hookable.
2. Positive: single-file swap surface if migrating to spdlog.
3. Tradeoff: quill background thread adds one extra OS thread; negligible on modern hardware.
4. Action: if quill introduces an incompatibility with a future dependency, re-evaluate using this record as the baseline.

## Follow-Up Actions
1. Task P0-02 — implement logging bootstrap using this decision.
2. Add `QUILL_VERSION` pin to CMake to avoid silent upgrades.
