# Programmable texture-mode adjustment

Story11/12 reached hub rendering after the backstory and stopped because native
program51 declared a sampler for unbound stage1. The captured stage0 handle is
19271776 and stages1..3 are null. Vertex program has13 instructions; pixel shader
has2 combiners and raw texture modes0x21 (PROJECT2D for stages0 and1).

## Original shader identity

The controller agent recovered the exact static shader pairing: original B6F19
passes pixel definition1ADF40 and vertex source1ADE68 to B6780. The13 vertex
instructions uniquely match supplied XBE file offset19EA0C. The60-word pixel
definition starts at file19EAE0; its first57 words match the captured SDK cache
except word11 (one live constant changed from0 toFF00000D). Its final_constants
is1FF, so the texture-mode adjustment flag100 is demonstrably enabled.

Its two RGB stages calculate r0=clamp(t0*d0.a), then
r0=clamp(r0*d0+C0_stage1). Alpha becomes1; default final combiner is used. No
combiner reads t1. An unused declaration alone was nevertheless enough to trigger
the old native resource check.

## Original boundary and implementation

Supplied original `local/reports/disasm/asm/D3D.asm`:

-102C3E..102C59 caches pixel-definition wordEC bit100 at device+378 and raw texture
 modes at device+37C.
-107D2C..107D3E leaves raw modes unchanged if adjustment is disabled.
-107D50..107D73 turns unbound sampled stages into modeNONE. Non-sampling modes4,
 5,A,11 are preserved by the original code.
-107D75..107DBA adapts modes1..3 using each bound resource's format header: cube
 bit selects3; dimensionality30hex or format2A..31 selects2; otherwise1.

The native shader draw now applies this conversion to a local pixel definition
before creating/looking up its shader cache key. The original stored definition
and cached raw modes remain intact, so binding/unbinding/rebinding recomputes the
correct variant. This bounded implementation covers the generator's existing
modes0..5. Unsupported dependent/bump modes remain explicit errors; adjustment
does not turn an unsupported declaration into a silently accepted empty shader.
Flag-clear shaders retain their declared modes and live-unbound resource error.

The pinned primary [xemu pixel shader reference](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/psh.c)
defines PS_TEXTUREMODES_NONE's register as(0,0,0,1). The native generator previously
used alpha0 and now matches alpha1. This is a hardware register value supported
by the reference, not a fabricated fallback texture. Actual texture allocation,
sampling and game CPU logic remain unchanged.

Sampler uniform locations are also cached once at native link time. If the
native compiler reports a sampler inactive, no binding is needed. This is only
an additional safe optimization: the Apple driver retained locations for the
unused sampler in the first synthetic test (tex0=9,tex1=12), so liveness filtering
alone was not a sufficient compatibility fix. The original flag-controlled mode
conversion above is what resolves the evidenced boundary.

## Validation

`tools/test_inactive_sampler.inc` uses synthetic original vertex/pixel objects and
actual guest SetTexture/draw bridges. Native GPU readback verifies flagged null
PROJECT2D becomes NONE, bound texture pixels return on rebind, raw modes remain
unchanged when the flag is clear, explicit reads of NONE alpha produce1, and
PASSTHRU/CLIPPLANE modes remain intact. Live samplers remain active when required.
All prior shader, fog, secondary-stream, render-target, palette and resource
lifetime fixtures pass in the same component run.

Logs: ignored `local/reports/inactive-sampler-build.log` and
`local/reports/inactive-sampler-smoke.log`. One compiler job per sequential build;
no independent game launch or full AOT build. Parent owns the next actual hub
validation. The earlier linker-only fixture failed as described above and was
replaced with the original SDK adjustment once its actual flag was recovered.
