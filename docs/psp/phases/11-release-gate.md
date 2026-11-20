# Phase 11 — Release gate

| | |
|---|---|
| State | not started |
| Depends on | every previous phase, or a documented reason why one was skipped |
| Exit criterion | four configurations measured on the same console, one report, and a decision about whether anything still needs the direct-GU path |

## What this phase is about

Proving the stack, not assembling it. Four configurations, same console, same workload, same clocks:

1. shipped pspGL + unbatched raylib (the baseline from phase 1)
2. upgraded pspGL + unbatched raylib (phase 3)
3. upgraded pspGL + the PSP batcher (phase 4)
4. native GU backend (phase 5 and later)

That ordering is the whole point: it separates the library's contribution from ours, so a regression can be
attributed instead of argued about.

## Tasks

- [ ] Run all four with the phase 2 counters and the same fixed workloads
- [ ] Report frame-time percentiles, not averages
- [ ] Report logical objects versus actual submissions per configuration
- [ ] Report GE waits, command-list utilisation, texture binds and uploads
- [ ] Report memory: heap, VRAM high-water, largest block
- [ ] Archive screenshots with build numbers for every configuration
- [ ] Check input and audio do not appear in the frame profile
- [ ] Write the report into this site, not into a chat log

## Release gates

- [ ] no correctness regression in the five samples
- [ ] no hidden unconditional `glFinish`-class wait in a steady frame
- [ ] same-texture 2D workloads show submissions collapsing from object count to state runs
- [ ] frame-time variance no worse than the baseline
- [ ] every new configuration switch documented, including how to turn it off

## What this phase decides

Whether the project is finished at configuration 4, or whether specific workloads still need something the
generic path cannot give them. That answer comes from the table, not from preference — and if the table says
pspGL plus a batcher is within a few percent, then the native backend is a maintenance cost that has to justify
itself.

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
