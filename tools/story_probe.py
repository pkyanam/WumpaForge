"""Read-only LLDB snapshots for the original Xbox story movie (selector 1).

Import this module and call install(lldb.debugger) before the existing bounded
run, or dump(lldb.debugger) at a stop. No guest functions are called and no guest
memory is written. Callback snapshots are BEFORE the named function executes;
use two stopped snapshots to establish animation movement, not these labels.
"""
import json
import struct
import time
import lldb


def integer(value, signed=False):
    if not value.IsValid() or not value.GetError().Success():
        return None
    return value.GetValueAsSigned() if signed else value.GetValueAsUnsigned()


def native_output(target):
    # These are real stopped-process counters, not calls into the audio API.
    names = ('g_sdl_submitted', 'g_sdl_played', 'g_sdl_queued', 'g_sdl_underruns',
             'g_sdl_overflows', 'g_sdl_nonzero_in', 'g_sdl_nonzero_out')
    return {name: integer(target.FindFirstGlobalVariable(name)) for name in names}


def native_stream(target, words):
    streams = target.FindFirstGlobalVariable('s_streams')
    for index in range(min(streams.GetNumChildren(), 64)):
        entry = streams.GetChildAtIndex(index)
        if integer(entry.GetChildMemberWithName('guest')) != words[1]:
            continue
        pointer = entry.GetChildMemberWithName('native')
        if not integer(pointer):
            continue
        stream = pointer.Dereference()
        slot = integer(stream.GetChildMemberWithName('slot'), True)
        out = {'slot': slot, 'paused': integer(stream.GetChildMemberWithName('paused'))}
        if slot is None or not 0 <= slot < 64:
            return out
        voice = target.FindFirstGlobalVariable('g_mixer_voices').GetChildAtIndex(slot)
        for name in ('active', 'num_channels', 'sample_rate', 'play_offset'):
            out[name] = integer(voice.GetChildMemberWithName(name))
        out['volume'] = voice.GetChildMemberWithName('volume').GetValue()
        queue = target.FindFirstGlobalVariable('g_mixer_queues').GetChildAtIndex(slot)
        for name in ('capacity', 'head', 'count', 'bytes'):
            out['queue_' + name] = integer(queue.GetChildMemberWithName(name))
        head, count = out['queue_head'], out['queue_count']
        if head is not None and count is not None and 0 <= head < 64 and 0 <= count <= 64:
            packets = queue.GetChildMemberWithName('packets')
            frames = [integer(packets.GetChildAtIndex((head + i) % 64).GetChildMemberWithName('frames'))
                      for i in range(count)]
            out['packet_frames'] = frames
            # Exact for these non-looped mono ADPCM story streams while all
            # three original packet slots are pending: no refill is in flight.
            if (words[10] in (173, 174) and words[7] <= words[6]
                    and words[7] % 36 == 0 and count == 3
                    and words[3:6] == [0x8000000A] * 3
                    and None not in frames and out['play_offset'] is not None
                    and out['sample_rate'] == 44100 and out['num_channels'] == 1):
                consumed = words[7] // 36 * 64 - sum(frames) + out['play_offset'] / 65536
                out['source_seconds_mixed'] = consumed / 44100
        return out
    return {'error': 'native stream debug metadata unavailable'}


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
        ('channel4_index', 0x427A10, 'i'), ('channel4_kind', 0x427A14, 'i'),
        ('swaps', 0x10EBD4, 'I'), ('vblank', 0x1BABB0, 'I'))}
    out['host_monotonic_ns'] = time.monotonic_ns()
    out['native_output'] = native_output(target)
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
        out['native_stream'] = native_stream(target, out['stream_wrapper_words'])
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
