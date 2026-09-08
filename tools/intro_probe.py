"""Read-only LLDB intro-state probe. Import with `command script import`, then
`script intro_probe.install(lldb.debugger)` before run. Use the existing bounded
watchdog and `thread backtrace all`/`quit` commands; see tools/boot.py.

Samples original intro update boundaries without changing game state. Debugger
stops affect wall time; do not use timings here as performance measurements.
"""
from pathlib import Path
import json, struct, time, lldb
count = 0
draw_count = 0
start = time.monotonic()

def global_value(target, name):
    v = target.FindFirstGlobalVariable(name)
    return v.GetValueAsUnsigned() if v.IsValid() and v.GetError().Success() else None

def snapshot(frame, label):
    process = frame.GetThread().GetProcess()
    target = process.GetTarget()
    base = global_value(target, 'g_xbox_mem_offset')
    if not base:
        return {'error': 'guest memory base unavailable'}

    def read(address, fmt='I'):
        err = lldb.SBError()
        data = process.ReadMemory(base + address, struct.calcsize('<' + fmt), err)
        return struct.unpack('<' + fmt, data)[0] if err.Success() else None
    out = {'label': label, 'seconds': round(time.monotonic() - start, 3), 'swaps': read(1098000 + 10948)}
    for name, address, fmt in [('intro', 1556068, 'i'), ('next', 9391540, 'i'), ('fade', 1706364, 'i'), ('step', 2343948, 'i'), ('latch', 2343944, 'i'), ('gate', 9707620, 'I'), ('scene', 9391612, 'i'), ('pause', 2340696, 'i'), ('audio_gate', 1813036, 'I')]:
        out[name] = read(address, fmt)
    idx = out['scene']
    out['channel_index'] = read(4356624, 'i')
    out['channel_kind'] = read(4356628, 'i')
    out['intro_mode'] = read(5638190, 'B')
    out['animation_override'] = read(1813144, 'f')
    if idx is not None and 0 <= idx < 32:
        out['cut_audio'] = read(9391744 + idx * 4, 'i')
        out['scene_object'] = read(2343648 + idx * 4)
        inst = read(1812776 + idx * 4)
        out['instance'] = inst
        if inst and inst + 120 < 67108864:
            out['anim_flags'] = read(inst + 108)
            out['anim_position'] = read(inst + 112, 'f')
            out['anim_step'] = read(inst + 116, 'f')
    for name in ('s_fvf', 's_vertex_handle', 's_pixel_handle', 's_target_width', 's_target_height'):
        out[name] = global_value(target, name)
    out['states'] = {str(i): read(1109528 + i * 4) for i in [57, 58, 59, 60, 61, 62, 63, 64, 67]}
    out['stages'] = [[read(1109016 + stage * 128 + i * 4) for i in range(32)] for stage in range(4)]
    return out

def fade(frame, bp_loc, internal_dict):
    global count
    if not any(('sub_0002D950' in (f.GetFunctionName() or '') for f in list(frame.GetThread())[:8])):
        return False
    count += 1
    if count in (1, 2, 17, 33, 65, 129):
        print('[INTRO-PROBE] ' + json.dumps(snapshot(frame, 'before_fade_update_' + str(count))), flush=True)
    if count == 33:
        bp = frame.GetThread().GetProcess().GetTarget().BreakpointCreateByName('draw_vertices_data')
        bp.SetScriptCallbackFunction('intro_probe.draw')
    if count >= 129:
        bp_loc.GetBreakpoint().SetEnabled(False)
    return False

def draw(frame, bp_loc, internal_dict):
    global draw_count
    draw_count += 1
    process = frame.GetThread().GetProcess()
    target = process.GetTarget()
    out = snapshot(frame, 'draw_' + str(draw_count))
    for name in ('type', 'count', 'stride', 'vertices'):
        v = frame.FindVariable(name)
        out[name] = v.GetValueAsUnsigned() if v.IsValid() and v.GetError().Success() else None
    if out['vertices'] and out['stride'] and out['count']:
        err = lldb.SBError()
        data = process.ReadMemory(out['vertices'], min(out['stride'] * out['count'], 256), err)
        if err.Success():
            out['vertex_bytes'] = data.hex()
    g = target.FindFirstGlobalVariable('g')
    if g.IsValid() and g.GetTypeName() == 'D3DGL':
        rs = g.GetChildMemberWithName('rs')
        out['native_rs'] = {str(i): rs.GetChildAtIndex(i).GetValueAsUnsigned() for i in (7, 14, 15, 19, 20, 22, 23, 24, 25, 27, 168)}
    base = global_value(target, 'g_xbox_mem_offset')
    if base:
        out['matrices'] = {}
        for state in (0, 1, 6):
            err = lldb.SBError()
            data = process.ReadMemory(base + 1098000 + 1872 + state * 64, 64, err)
            if err.Success():
                out['matrices'][str(state)] = struct.unpack('<16f', data)
    print('[INTRO-DRAW] ' + json.dumps(out), flush=True)
    if draw_count >= 8:
        bp_loc.GetBreakpoint().SetEnabled(False)
    return False

def watchdog(frame, bp_loc, internal_dict):
    print('[INTRO-PROBE] ' + json.dumps(snapshot(frame, 'watchdog')), flush=True)
    return True

def install(debugger):
    target = debugger.GetSelectedTarget()
    bp = target.BreakpointCreateByName('sub_00085D40')
    bp.SetScriptCallbackFunction('intro_probe.fade')
    source = Path(__file__).resolve().parents[1] / 'third_party/xboxrecomp/src/kernel/xbox_memory_layout.c'
    for number, line in enumerate(source.read_text().splitlines(), 1):
        if 'fprintf(stderr, "[WATCHDOG]' in line:
            bp = target.BreakpointCreateByLocation('xbox_memory_layout.c', number)
            bp.SetScriptCallbackFunction('intro_probe.watchdog')
            break
    print('Installed intro probe', flush=True)
