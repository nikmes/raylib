# Recipe template

> Copy to `docs/recipes/<slug>.md`. A recipe is a technique that **already worked on hardware** — measured,
> not proposed. If it has not run on the console yet, it belongs in a sprint or a feature doc, not here.

## Problem

One sentence: the situation that needs this.

## When to use it

The conditions that make it correct. Include the conditions that make it **wrong** — most of these recipes
have a neighbouring case where they break.

## Solution

The smallest complete code or configuration that works. Real code, no `...`. If it needs setup (alignment,
thread attribute, cache operation), that setup is part of the recipe.

```c
/* example */
```

## Why it works

The mechanism, in two or three sentences. If it depends on an erratum or a firmware behaviour, name it.

## Measured

| what | value | conditions |
|---|---|---|
| | | model, firmware, build, workload |

Tag every number: `[measured: PSP-1000, fw 6.60]`, `[derived]`, or `[public: source]`.

## Traps

What went wrong while finding this, and the symptom that identifies it next time. One line each, written so
someone can grep for the symptom.

## Sources

Public sources, or the measurement that produced it. Same discipline as the memory map.

## Used by

Which sample, sprint or phase uses this today. A recipe nothing references is either new or dead — say which.
