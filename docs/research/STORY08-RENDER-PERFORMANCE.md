# Story08 chamber performance

Read-only monitoring of the parent's build50 run (PID51552), with no additional
game launch, debugger attachment, profiling sampler, screenshot or production
edit. Source: ignored `local/reports/story-08.log`. The parent visually confirmed
the Uka Uka chamber; scene2 now passes the previous shader-constant-mode stop.

Through scene2 frame7883, animation2431, vblank8255, the chamber remains at60FPS.
The first56 chamber aggregate windows have median60.00FPS. Typical work is11–13ms
per frame, with almost identical CPU and wall times; Present takes the remaining
roughly4–5ms. Native draw-driver time in an observed window is0.319ms/frame.
Texture uploads are zero in steady windows, so the prior palette-triggered decode
bottleneck has not returned.

The three early windows below59FPS are frame3083 (56.76FPS,max72.615ms),3323
(56.27,max82.518ms),3803 (55.27,max61.040ms). These are isolated stalls; subsequent
windows return to60. Their cause cannot be uniquely attributed from aggregates
(the live run also includes sparse debugger boundary probes and user-interface
inspection). It would be misleading to call them a sustained renderer slowdown.

Between scene2 frame3023/animation1/vblank3383 and frame7883/animation2431/vblank8255,
original animation advances2430/30=81.0seconds over81.2seconds of vblank time.
The extra12 vblanks arise mostly in those early isolated stalls. This is not an
ongoing accumulating multi-second video delay. Audio continuity is separately
owned/measured by the native-audio agent; these counters alone do not prove exact
end-to-end speaker synchronization.

Rendering still omits indexed draws rejected for secondary attribute6/stream1
range validation, including counts144 and60. That is a concrete correctness bug
under investigation by the controller agent. Current frame-cost headroom must be
rechecked once these draws actually render. No speculative performance optimization
or timing change is justified by the present60FPS measurements.
