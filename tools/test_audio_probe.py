"""Exercise partial LLDB metadata/memory failures without launching a process."""
import importlib.util
import struct
import sys
import types
from pathlib import Path


class Error:
    def __init__(self, failed=False): self.failed = failed
    def Fail(self): return self.failed
    def Success(self): return not self.failed
    def __str__(self): return 'unavailable' if self.failed else 'success'


class Value:
    def __init__(self, value=None, children=None):
        self.value, self.children = value, children or []
    def IsValid(self): return self.value is not None or bool(self.children)
    def GetError(self): return Error(not self.IsValid())
    def GetValue(self): return str(self.value) if self.value is not None else None
    def GetValueAsUnsigned(self): return self.value
    def GetNumChildren(self): return len(self.children)
    def GetChildAtIndex(self, i): return self.children[i]
    def GetChildMemberWithName(self, name): return Value()


class Options:
    def SetExecutionPolicy(self, policy): assert policy == 7
    def SetTimeoutInMicroSeconds(self, timeout): assert timeout <= 100000


class Frame:
    def EvaluateExpression(self, name, options):
        assert name.startswith('g_') and name.isidentifier()
        if name == 'g_esi': return Value(0x431EDC)
        if name == 'g_ecx': return Value(0x16B70C)
        if name == 'g_edi': raise RuntimeError('optimized out')
        return Value()
    def GetFunctionName(self): return 'sub_0005DA30'


class Thread:
    def GetSelectedFrame(self): return Frame()
    def GetThreadID(self): return 20
    def __iter__(self): return iter([Frame()])


class Process:
    def GetSelectedThread(self): return Thread()
    def ReadMemory(self, address, length, error):
        address -= 0x100000000
        data = {0x431EDC: 0x20000, 0x20000: 0x16B70C,
                0x16B718: 0x1363D6}
        if address == 0x431F60:
            error.failed = True
            return b''
        return b''.join(struct.pack('<I', data.get(a, 0))
                        for a in range(address, address+length, 4))


class Target:
    def GetProcess(self): return Process()
    def FindFirstGlobalVariable(self, name):
        return Value(0x100000000) if name == 'g_xbox_mem_offset' else Value()


sys.modules['lldb'] = types.SimpleNamespace(SBError=Error,
    SBExpressionOptions=Options, eExecutionPolicyNever=7)
spec = importlib.util.spec_from_file_location('audio_probe', Path(__file__).with_name('audio_probe.py'))
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
result = probe.capture(types.SimpleNamespace(GetSelectedTarget=lambda: Target()))
assert result['registers']['g_esi'] == 0x431EDC
assert result['registers']['g_edi'] is None
assert result['wrappers'][0]['stream_object']['vtable'][3] == 0x1363D6
assert result['selected_stream']['address'] == 0x20000
assert result['guest_stack'] is None
assert result['native_streams'] == [] and 'native_streams' in result['errors']
assert len(result['wrappers']) == 6
print('PASS: TLS expression fallback, independent missing metadata, bounded stream/vtable reads')
