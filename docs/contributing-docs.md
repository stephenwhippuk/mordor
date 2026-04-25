# Documentation Contribution Guide

## Purpose
Define how to grow documentation in a consistent, navigable way.

## Core Rules
1. Keep one concern per document.
2. Add every new document to the docs index.
3. Prefer incremental updates over rewriting historical decisions.
4. Use clear section headings and concrete acceptance criteria where applicable.

## Where to Place Content
1. Architecture and boundaries: docs/architecture
2. Delivery phases and release scope: docs/plan
3. Research and option analysis: docs/research
4. Testing, risks, and quality controls: docs/quality
5. Scheduling, backlog, and short-term execution: docs/execution
6. Technical decisions and templates: docs/decisions

## Document Naming
1. Use lowercase and hyphenated file names.
2. Keep names specific and short.
3. Avoid vague names such as notes.md or ideas.md.

## Update Workflow
1. Identify the correct target document.
2. Add or revise content in that file.
3. Update docs/README.md navigation links.
4. If the change is a decision, add a decision record under docs/decisions.
5. Check for duplicate or stale content and remove it.

## Decision Records
1. Use the template in docs/decisions/0000-decision-template.md.
2. Assign the next sequence number.
3. Set status to Proposed, Accepted, or Superseded.
4. Link related implementation tasks in execution docs.

## Style Guide
1. Use concise, direct language.
2. Use numbered lists for procedures and checklists.
3. Include explicit exit criteria for plans and milestones.
4. Keep tables for status tracking and dependencies.

## Pull Request Checklist for Docs
1. Does each changed doc have a single clear purpose?
2. Are all new docs linked from docs/README.md?
3. Are decision-impacting changes recorded in docs/decisions?
4. Are outdated sections removed or marked as superseded?
