# Mordor — Copilot Workspace Instructions

## Project Context
Mordor is a C/C++ isometric RPG and engine project. The codebase does not exist yet; we are in planning and foundation phase.

See [docs/overview.md](../docs/overview.md) for project direction.
See [docs/README.md](../docs/README.md) for the full documentation index.

## Documentation Rules

These rules apply to every code, planning, or documentation task.

### Always keep docs in sync with changes
1. If a change affects architecture, update [docs/architecture/engine-architecture.md](../docs/architecture/engine-architecture.md).
2. If a change completes or modifies a deliverable, update the status in [docs/execution/phase-backlog.md](../docs/execution/phase-backlog.md).
3. If a change crosses a milestone boundary, update [docs/execution/milestone-tracker.md](../docs/execution/milestone-tracker.md).
4. If a new technical choice is made, create a decision record under [docs/decisions/](../docs/decisions/) using the template in `0000-decision-template.md`.
5. If a new doc is created, add it to [docs/README.md](../docs/README.md) under the appropriate section.

### Follow the documentation structure
Documentation is organized into six concern areas. Place content in the correct folder:
1. Architecture and system boundaries → `docs/architecture/`
2. Phases and release scope → `docs/plan/`
3. Research and open questions → `docs/research/`
4. Testing, risks, and quality → `docs/quality/`
5. Milestones, backlog, immediate tasks → `docs/execution/`
6. Technical decisions → `docs/decisions/`

See [docs/contributing-docs.md](../docs/contributing-docs.md) for full conventions.

### Never leave documentation stale
1. When implementing a planned task, mark it In Progress in the phase backlog before starting.
2. When completing a task, mark it Complete in the phase backlog and update the milestone status if the milestone exit criteria are now met.
3. Do not add new research candidates without recording them in [docs/research/research-tracks.md](../docs/research/research-tracks.md).
4. Do not hard-code architectural decisions without a corresponding entry in `docs/decisions/`.

## Git Workflow Rules
These rules apply to every task.

1. Before starting a task, switch to `master` and pull the latest changes.
2. Every task must be done on a new branch created from the latest `master`.
3. Use one task branch per task. Do not reuse old branches.
4. Commit task changes to that task branch.
5. Request a local Copilot agent review and resolve high-severity findings before push.
6. Push branch changes and open a pull request targeting `master` for review.
7. Do not begin the next task until the current task PR is reviewed and merged to `master`.
8. After merge, pull latest `master` again before creating the next task branch.

Recommended command sequence:
1. `git checkout master`
2. `git pull --ff-only origin master`
3. `git checkout -b task/<short-task-name>`
4. Implement changes, then `git add -A && git commit -m "<message>"`
5. Request local Copilot agent review and fix critical findings
6. Push and open PR to `master`
7. After merge: `git checkout master && git pull --ff-only origin master`

## Code Rules

### Language and portability
1. Implementation language is C/C++.
2. Rendering is OpenGL.
3. OS-specific includes belong only in the platform layer.
4. Avoid toolchain-specific pragmas outside of clearly guarded platform files.

### Architecture discipline
1. Implement new functionality in the correct engine layer.
2. Cross-layer dependencies must go downward in the layer stack only.
3. Every system with external side effects should emit events via the event bus.
4. Simulation logic must be deterministic and must not read wall-clock time directly.

### Testing
1. New interaction state machines require unit tests before being considered complete.
2. New procedural generation paths require validation checks.
3. See [docs/quality/testing-strategy.md](../docs/quality/testing-strategy.md) for test categories and expectations.

## Decision Workflow
When a meaningful technical choice is encountered:
1. Create `docs/decisions/NNNN-short-title.md` using the template.
2. List options considered, the chosen option, and consequences.
3. Link the decision from the relevant task in [docs/execution/phase-backlog.md](../docs/execution/phase-backlog.md).
