"""Read-only LLDB shader state export at a real game rendering failure."""
import json
from pathlib import Path
import lldb


def dump(debugger, destination):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    error = lldb.SBError()

    def value(name):
        item = target.FindFirstGlobalVariable(name)
        if not item.IsValid():
            raise RuntimeError("Missing debugger global: " + name)
        return item

    def integers(item):
        if item.GetNumChildren():
            return [integers(item.GetChildAtIndex(i))
                    for i in range(item.GetNumChildren())]
        return item.GetValueAsUnsigned()

    base = value("g_xbox_mem_offset").GetValueAsUnsigned()

    def words(address, count):
        data = process.ReadMemory(base + address, count * 4, error)
        if error.Fail():
            raise RuntimeError(error.GetCString())
        return [int.from_bytes(data[i:i+4], "little")
                for i in range(0, len(data), 4)]

    obj = value("s_vertex_object")
    count = obj.GetChildMemberWithName("instruction_count").GetValueAsUnsigned()
    out = {
        "vertex_handle": integers(value("s_vertex_handle")),
        "pixel_handle": integers(value("s_pixel_handle")),
        "swap_count": words(0x10C110 + 0x2AC4, 1)[0],
        "instruction_count": count,
        "words": integers(obj.GetChildMemberWithName("words"))[:count],
        "slots": [
            {field.GetName(): integers(field)
             for field in obj.GetChildMemberWithName("slots").GetChildAtIndex(i)}
            for i in range(16)
        ],
        "vertex_constants_bits": words(
            value("s_vertex_constants").GetLoadAddress() - base, 192 * 4),
        "texture_states": [words(0x10EC18 + stage * 128, 32)
                           for stage in range(4)],
    }
    # At shader_error, walk back to the real native draw, without evaluating game
    # functions. Optimized-away parameters are explicitly unavailable.
    for frame in process.GetSelectedThread():
        if "shader_draw" not in (frame.GetFunctionName() or ""):
            continue
        values = [frame.FindVariable(name) for name in ("vertices", "count", "stride")]
        if not all(v.IsValid() and v.GetValue() is not None for v in values):
            out["draw"] = {"available": False}
            break
        address, vertices, stride = [v.GetValueAsUnsigned() for v in values]
        size = vertices * stride
        out["draw"] = {"count": vertices, "stride": stride, "available": False}
        if address and 0 < size <= 16 * 1024 * 1024:
            data = process.ReadMemory(address, size, error)
            if error.Success():
                out["draw"].update(available=True, bytes_hex=data.hex())
        break
    Path(destination).write_text(json.dumps(out, indent=2) + "\n")
    print("[SHADER-PROBE] wrote " + destination)
