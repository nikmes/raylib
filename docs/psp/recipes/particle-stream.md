# Recipe — One vertex stream per particle system

## Problem

A particle system drawn with one `DrawTexturePro` per particle pays the per-call corner math, texture
selection and four vertex pushes for every particle, even though the batcher would merge them into one run.

## When to use it

- One texture for the whole system, 2D, unrotated, uniformly scaled quads.
- Particles can be emitted in any order; no per-particle state change.
- **Wrong** for rotated particles mixed in the same loop (the batcher must switch primitive), multiple textures
  interleaved, or anything with depth/3D.

## Solution

```c
/* App-side. The library's batcher is inlined into this translation unit, so the
   build needs: -I<repo>/src -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1 */
rlSetTexture(particleTex.id);
rlBegin(RL_QUADS);

for (int i = 0; i < count; i++)
{
    float x = p[i].pos.x, y = p[i].pos.y;
    Color c = p[i].color;

    rlColor4ub(c.r, c.g, c.b, c.a);
    rlTexCoord2f(0.0f, 0.0f); rlVertex2f(x, y);
    rlTexCoord2f(0.0f, 1.0f); rlVertex2f(x, y + 32.0f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex2f(x + 32.0f, y + 32.0f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex2f(x + 32.0f, y);
}

rlEnd();
rlSetTexture(0);
```

## Why it works

The batcher recognises each axis-aligned one-colour quad as a GE sprite and stores **2 vertices** per particle
in one run; `rlSetTexture` is deferred, so the system costs one bind and one `glDrawArrays` per texture run
until the 4,096-vertex cap. The loop removes the per-call `DrawTexturePro` overhead, not the vertex count.

## Measured

`[measured: PSP-1000, fw 6.60, raylib 6.5-dev, pspgu b24]`

| particles | `DrawTexturePro` | direct stream | change | draws |
|---:|---:|---:|---:|---:|
| 500 | 3.064 ms | **2.218 ms** | −27.6 % | 1 |
| 2,000 | 10.928 ms | **7.804 ms** | −28.6 % | 1 |

Captures are byte-identical at both points; binds are one per run; simulation is measured separately.

## Traps

- **Build flags:** without `-I../../src` and `-DPSP_BATCH=1` the app links the non-inline rl* declarations and
  fails; with the installed header it silently uses a different path.
- **Shared harness files:** a fixed `app.prx` or marker name collides with another session's run; use unique
  names per measurement.
- **Arena:** 4,000 particles are 8,000 vertices = 192 KiB, over the 128 KiB per-list arena (5,461 vertices);
  the frame falls back to a full sync. Keep a stream under the arena or accept the split.

## Sources

Sprint 022 (P7-S5), `/home/psp/evidence/sprint022/`.

## Used by

`psp-tests/scene_2d` `MODE=particles PARTICLE_IMPL=direct`.
