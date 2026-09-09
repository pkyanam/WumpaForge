# Bounded texture coherence diagnostic

Set `WRATH_TRACE_TEXTURE_FRAME=N` before process start. Like the existing pixel
probe, N is the guest swap counter plus one. Only that frame is inspected, with
at most96 records across the process and256KiB per resource. Larger resources
are excluded. No resource handles are hardcoded. Root coordinates marker wiring,
Android builds and device use.

`[shield texture]` correlates pre-upload attempts with completed render-target
resolves using handle, owner, data address, parent offset/mip, format, dimensions,
guest return address and upload serial. Resolve records compare an exact temporary
copy of guest encoded bytes before readback with those after resolve: `changed`
and `first` are the changed-byte count and first offset. `before=0` means no copy
was available or requested; zero changes then does not prove equality.

`shadow_changed` compares current bytes against the parent's valid upload snapshot
at the matching offset. `shadow=0` means comparison was unavailable. `first` or
`shadow_first` equal4294967295 means no difference was found. Hashes help correlate
records but are not substitutes for exact comparison. An upload row precedes the
upload and does not report its eventual success. Resolve `status` is its actual
result. GPU readback, guest writes and normal upload decisions are unchanged.

Repeated resolves with zero exact changes and zero shadow changes indicate no
encoded-byte change for those samples. Nonzero resolve changes identify actual
readback-to-guest changes; compare surrounding pixel-probe evidence before blaming
layout or game writes. A later upload difference alone cannot identify the writer.
The probe adds CPU scanning/copying and log I/O, so its frame is not a performance
benchmark. It does not suppress any resolve or relax texture coherence.

Validation: `python3 android/shield2019/tests/test_texture_trace.py` passes a host
UBSan fixture for selected-frame admission, exact byte differences, owner/mip
mapping, missing/invalid settings, bounds, and unchanged guest/snapshot state.
The patch applies after the existing Android title patch sequence. Device behavior
and its usefulness on the reported loading transition remain to be checked.
