# Hub visual stability: build63

This bounded review used existing files only; it did not inspect or interrupt the
live game. The user's intermittent Cortex distortion and missing story geometry
remain unverified visually. The retained-array and component dependency paths
did not reveal a new concrete translation error during this review.

The strongest actionable evidence is a different rendering boundary:
`local/reports/play-24.log` records five rejected `SetRenderTarget` calls at lines
10819, 10849, 10889, 10924 and 11027. Their color handles are
`0x118EA80/AA0/AC0/AE0/B00`, with depth `0xB600A0`. The surrounding profile reports
level37, demo0, frames9180 onward. An earlier rejection also exists in
`story-23.log` line13680. Thus absence of fatal shader or stream failures does
not establish that every original rendering call succeeded.

`src/graphics.c:sub_000FEF20` accepts a depth surface only with the native
backbuffer. A texture surface paired with nonzero depth is explicitly rejected;
the previously bound target remains active. The logs do not include resource
types, dimensions or the original caller, so they do not prove which validation
condition failed. Nor do they establish that these calls draw the Cortex effect.
If the caller continues drawing after a failed target switch, those draws use
the previous target. Supporting a valid offscreen color/depth pair requires real
compatible depth storage and correct attachment lifetime, not removing the guard.

## Next targeted capture

On a future coordinated run, record only the first rejected target switch:
guest return address, requested handles, current device color/depth handles,
both resource types/formats/dimensions/pitches/owners/generations, native FBO and
attachment identity, and current level/demo/cutscene/animation/frame. Capture
the next presented image at the same frame and inspect the original caller's
HRESULT handling. This can be a bounded diagnostic in the existing rejection
branch; it does not require another full story run or a per-draw breakpoint.

If this is unrelated to the observed character, the necessary separate evidence
is a distorted frame paired with the specific draw's vertex program/constants,
indices, primary and retained attribute bytes and committed physical addresses.
Prior bounds failures alone cannot establish the cause of a later visual defect.
