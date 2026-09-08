"""Read-only LLDB snapshots for the original Xbox story movie (selector 1).

Import this module and call install(lldb.debugger) before the existing bounded
run, or dump(lldb.debugger) at a stop. No guest functions are called and no guest
memory is written. Callback snapshots are BEFORE the named function executes;
use two stopped snapshots to establish animation movement, not these labels.
"""
import json
import struct
import lldb


def snapshot(frame):
    process = frame.GetThread().GetProcess()
    target = process.GetTarget()
    value = target.FindFirstGlobalVariable('g_xbox_mem_offset')
    if not value.IsValid() or not value.GetError().Success():
        return {'error': 'guest memory base unavailable'}
    base = value.GetValueAsUnsigned()

    def read(address, fmt='I'):
        if address is None or address < 0 or address + struct.calcsize('<' + fmt) > 0x4000000:
            return None
        error = lldb.SBError()
        data = process.ReadMemory(base + address, struct.calcsize('<' + fmt), error)
        return struct.unpack('<' + fmt, data)[0] if error.Success() else None

    def string(address):
        if not address or address >= 0x4000000:
            return None
        error = lldb.SBError()
        return process.ReadCStringFromMemory(base + address, 160, error) if address else None

    out = {name: read(address, fmt) for name, address, fmt in (
        ('movie', 0x17BE64, 'i'), ('next_movie', 0x8F4DB4, 'i'),
        ('scene', 0x8F4DFC, 'i'), ('cut_on', 0x942064, 'I'),
        ('fade', 0x1A097C, 'i'), ('fade_rate', 0x23C40C, 'i'),
        ('paused', 0x23B758, 'I'), ('audio_hold', 0x1BAA2C, 'I'),
        ('level', 0x19C068, 'i'), ('demo', 0x23B750, 'i'),
        ('load_thread', 0x1BAA8C, 'I'), ('loading_finished', 0x1BAA90, 'I'),
        ('channel4_index', 0x427A10, 'i'), ('channel4_kind', 0x427A14, 'i'))}
    out['loaded_story_instances'] = [read(0x1BA928 + i * 4) for i in range(11)]
    index = out['scene']
    if index is not None and 0 <= index < 32:
        inst = read(0x1BA928 + index * 4)
        out['instance'] = inst
        out['cutscene'] = read(0x1BA7A8 + index * 4)
        out['world_scene'] = read(0x23C2E0 + index * 4)
        out['cut_audio'] = read(0x8F4E80 + index * 4, 'i')
        if inst:
            out['animation'] = {name: read(inst + offset, fmt) for name, offset, fmt in (
                ('flags', 0x6C, 'I'), ('position', 0x70, 'f'), ('rate', 0x74, 'f'))}
        if out['movie'] == 1 and index < 11:
            desc = 0x17BBF8 + index * 16
            out['original_descriptor'] = {
                'scene_name': string(read(desc)), 'cut_name': string(read(desc + 4)),
                'audio_id': read(desc + 8, 'i'), 'debris_group': read(desc + 12, 'i')}
    channel = out['channel4_index']
    if out['channel4_kind'] == 2 and channel is not None and 0 <= channel < 6:
        wrapper = 0x431ED8 + channel * 0x44
        out['stream_wrapper'] = wrapper
        # Raw words retain evidence without assigning unaudited field meanings.
        out['stream_wrapper_words'] = [read(wrapper + i * 4) for i in range(17)]
        out['stream_feeding'] = read(wrapper + 0x24)
    pad = read(0x23C2D8)
    if pad:
        out['pad_rising_edges'] = read(pad + 0xD0)
    return out


def dump(debugger, path=None):
    process = debugger.GetSelectedTarget().GetProcess()
    frame = process.GetSelectedThread().GetFrameAtIndex(0)
    out = snapshot(frame)
    data = json.dumps(out, indent=2)
    if path:
        from pathlib import Path
        Path(path).write_text(data + '\n')
    print('[STORY-PROBE] ' + data, flush=True)
    return out


def event(frame, bp_loc, internal_dict):
    out = snapshot(frame)
    if out.get('movie') == 1:
        out['before_function'] = frame.GetFunctionName()
        print('[STORY-PROBE] ' + json.dumps(out), flush=True)
    return False


def install(debugger):
    target = debugger.GetSelectedTarget()
    for symbol in ('sub_0002C020', 'sub_0002C480', 'sub_0002BC40', 'sub_0002BC00'):
        bp = target.BreakpointCreateByName(symbol)
        bp.SetScriptCallbackFunction('story_probe.event')
    print('Installed original story boundary probes (no per-frame stops)', flush=True)
