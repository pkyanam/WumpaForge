"""Read-only LLDB capture for a stopped original audio worker.

Usage: command script import tools/audio_probe.py
       script audio_probe.dump(lldb.debugger, 'local/reports/audio-stop.json')

Reads simple debugger variables and memory only; no guest/API method is invoked.
Unavailable optimized/TLS values are recorded individually, not fatal to the dump.
"""
import json
from pathlib import Path
import lldb


def _integer(value):
    if (not value.IsValid() or not value.GetError().Success()
            or value.GetValue() is None):
        return None
    return value.GetValueAsUnsigned()


def capture(debugger):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    out = {'thread_id': thread.GetThreadID(), 'registers': {}, 'errors': {},
           'frames': [f.GetFunctionName() for f in thread]}

    def variable(name, expression_first=False):
        # LLDB's global lookup often cannot materialize Mach-O TLS; a simple
        # variable expression in the selected stopped thread works in that case.
        # Never permit an expression to execute target code to obtain a value.
        options = lldb.SBExpressionOptions()
        options.SetExecutionPolicy(lldb.eExecutionPolicyNever)
        options.SetTimeoutInMicroSeconds(100000)
        attempts = (('expression', 'global') if expression_first
                    else ('global', 'expression'))
        failures = []
        for method in attempts:
            try:
                value = (frame.EvaluateExpression(name, options) if method == 'expression'
                         else target.FindFirstGlobalVariable(name))
                result = _integer(value)
                if result is not None:
                    return result
                failures.append(method + ': ' + str(value.GetError()))
            except Exception as exc:
                failures.append(method + ': ' + str(exc))
        out['errors'][name] = failures
        return None

    for name in ('g_eax', 'g_ebx', 'g_ecx', 'g_edx', 'g_esi', 'g_edi',
                 'g_esp', 'g_ebp', 'g_seh_ebp', 'g_xbox_kernel_caller'):
        out['registers'][name] = variable(name, expression_first=True)
    base = variable('g_xbox_mem_offset')
    out['memory_offset'] = base

    def words(address, count):
        if base is None or address is None or not 0 <= address <= 0x4000000-count*4:
            return None
        error = lldb.SBError()
        try:
            data = process.ReadMemory(base + address, count * 4, error)
            if error.Fail() or len(data) != count * 4:
                out['errors']['memory_' + hex(address)] = str(error)
                return None
            return [int.from_bytes(data[i:i+4], 'little')
                    for i in range(0, len(data), 4)]
        except Exception as exc:
            out['errors']['memory_' + hex(address)] = str(exc)
            return None

    def object_record(address):
        record = {'address': address, 'words': words(address, 4)}
        data = record['words']
        if data is not None:
            record['vtable_address'] = data[0]
            record['vtable'] = words(data[0], 7)
        return record

    out['original_stream_vtable'] = words(0x16B70C, 7)
    out['wrappers'] = []
    for i in range(6):
        address = 0x431ED8 + i * 0x44
        data = words(address, 17)
        record = {'index': i, 'address': address, 'words': data}
        if data:
            record['file_object'] = object_record(data[0]) if data[0] else None
            record['stream_object'] = object_record(data[1]) if data[1] else None
        out['wrappers'].append(record)
    esi = out['registers']['g_esi']
    out['selected_esi_words'] = words(esi, 17)
    selected = words(esi, 1)
    if selected:
        out['selected_stream'] = object_record(selected[0])
    out['guest_stack'] = words(out['registers']['g_esp'], 24)
    out['native_streams'] = []
    try:
        streams = target.FindFirstGlobalVariable('s_streams')
        for i in range(min(streams.GetNumChildren(), 64)):
            entry = streams.GetChildAtIndex(i)
            guest = _integer(entry.GetChildMemberWithName('guest'))
            native = _integer(entry.GetChildMemberWithName('native'))
            if guest or native:
                out['native_streams'].append({'index': i, 'guest': guest,
                                             'native': native})
        if not streams.IsValid():
            out['errors']['native_streams'] = 's_streams metadata unavailable'
    except Exception as exc:
        out['errors']['native_streams'] = str(exc)
    return out


def dump(debugger, destination):
    result = capture(debugger)
    Path(destination).write_text(json.dumps(result, indent=2) + '\n')
    print('Audio worker snapshot saved: ' + str(destination))
    return result
