"""Read-only LLDB shader state export at a real game rendering failure."""
import json
import struct
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
        "texture_handles": integers(value("s_texture_handles")),
        "pixel_definition": {field.GetName(): integers(field)
                             for field in value("s_pixel_definition")},
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
        "render_state_cache": words(0x10EE18, 146),
        "stream_records": [dict(zip(("stride", "offset", "handle"),
                                    words(0x10F280 + stream * 12, 3)))
                           for stream in range(16)],
        "game_state": {"level": words(0x19C068, 1)[0],
                       "demo": words(0x23B750, 1)[0],
                       "cutmovie": words(1556068, 1)[0],
                       "next_cut_movie": words(9391540, 1)[0],
                       "cutworldix": words(9391612, 1)[0]},
    }
    arrays=target.FindFirstGlobalVariable("s_vertex_arrays")
    if arrays.IsValid():
        out["retained_vertex_arrays"]=[
            {field.GetName():integers(field) for field in arrays.GetChildAtIndex(i)}
            for i in range(min(arrays.GetNumChildren(),16))]
        for name in ("s_vertex_arrays_ready","s_vertex_array_base","s_vertex_generation"):
            item=target.FindFirstGlobalVariable(name)
            if item.IsValid():out[name]=integers(item)
    def floats(address, count):
        return [struct.unpack("<f", word.to_bytes(4, "little"))[0]
                for word in words(address, count)]

    out["hub_state"] = {
        "hub": words(0x19A838, 1)[0],
        "selected_slot": words(0x560F1C, 1)[0],
        "candidate_available": words(0x560ECC, 1)[0],
        "portal_dwell": words(0x23B7B8, 1)[0],
        "pending_level": words(0x19C074, 1)[0],
        "requested_level": words(0x19C06C, 1)[0],
        "warp_elapsed": words(0x561674, 1)[0],
        "warp_duration": words(0x561668, 1)[0],
        "player_position": floats(0x8F518C, 3),
    }
    # First hub's original spline: portal slot0 center is point2. Only read a
    # valid loaded structure; early title/story captures may have no hub yet.
    spline = words(0x19C148, 1)[0]
    if 0x1000 <= spline < 0x4000000 - 12:
        header = words(spline, 3)
        stride, points = header[0] >> 16, header[2]
        center = points + 2 * stride
        if 12 <= stride <= 4096 and 0x1000 <= center < 0x4000000 - 12:
            out["hub_state"]["arctic_portal_center"] = floats(center, 3)

    # Capture the real D3DX mesh registry at a null indirect call. This does
    # not depend on LLDB being able to resolve ARM64 thread-local globals.
    # The original pool has 1000 five-DWORD entries at21F8B8; word4 is mesh*.
    # At3B460 the expected object vtables are169EB0/169F20, method+38=111140.
    if any("sub_0003B2E0" in (f.GetFunctionName() or "")
           for f in process.GetSelectedThread()):
        mesh = {"registers": {}, "pool": [],
                "free_head": words(0x2246E0, 1)[0],
                "used_count": words(0x2246E4, 1)[0],
                "used_head": words(0x2246E8, 1)[0],
                "expected_vtables": {hex(a): words(a, 28)
                                     for a in (0x169EB0, 0x169F20)}}
        for name in ("g_eax", "g_ebx", "g_ecx", "g_edx", "g_esi", "g_edi",
                     "g_esp", "g_ebp", "g_seh_ebp", "g_xbox_kernel_caller"):
            item = target.FindFirstGlobalVariable(name)
            mesh["registers"][name] = (item.GetValueAsUnsigned()
                if item.IsValid() and item.GetError().Success() and item.GetValue() is not None else None)
        esp = mesh["registers"].get("g_esp")
        if esp and 0x10000 <= esp < 0x4000000 - 128:
            mesh["guest_stack"] = words(esp, 32)
        registry = words(0x21F8B8, 1000 * 5)
        for i in range(1000):
            entry = registry[i * 5:(i + 1) * 5]
            address = entry[4]
            if not address:
                continue
            record = {"index": i, "handle": i + 1, "entry": entry}
            if 0x10000 <= address < 0x4000000 - 104:
                record["object"] = words(address, 26)
                vtable = record["object"][0]
                if 0x10000 <= vtable < 0x4000000 - 112:
                    record["vtable"] = words(vtable, 28)
            mesh["pool"].append(record)
        out["mesh"] = mesh
    filename = process.ReadMemory(base + 0x561740, 256, error)
    for stream in out["stream_records"]:
        handle = stream["handle"]
        if 0x1000 <= handle <= 0x4000000 - 24:
            stream["header"] = words(handle, 6)
    if error.Success():
        out["game_state"]["level_filename"] = filename.split(b"\0", 1)[0].decode("utf-8", "replace")
    # Read allocated native metadata pages in bulk; no getter or game code runs.
    pages = target.FindFirstGlobalVariable("s_resource_pages")
    page_count = target.FindFirstGlobalVariable("s_resource_page_count")
    if pages.IsValid() and page_count.IsValid():
        count = page_count.GetValueAsUnsigned()
        summary = {"allocated_pages": count, "capacity": count * 256,
                   "live": 0, "by_type": {}, "guest_bytes": 0, "bound_records": []}
        bound_handles = set(out["texture_handles"]) | {
            stream["handle"] for stream in out["stream_records"]}
        bound_handles.update(record["handle"] for record in out.get("retained_vertex_arrays",[]))
        bound_handles.discard(0)
        if count <= pages.GetNumChildren():
            for i in range(count):
                pointer = pages.GetChildAtIndex(i)
                kind = pointer.GetType().GetPointeeType()
                size = kind.GetByteSize()
                offsets = {kind.GetFieldAtIndex(j).GetName():
                           kind.GetFieldAtIndex(j).GetOffsetInBytes()
                           for j in range(kind.GetNumberOfFields())}
                required = ("handle", "type", "data", "bytes", "width", "height",
                            "levels", "format", "pitch", "references", "bindings")
                if not size or not all(k in offsets for k in required):
                    summary["error"] = "Resource debug layout unavailable"
                    break
                data = process.ReadMemory(pointer.GetValueAsUnsigned(), size * 256, error)
                if error.Fail():
                    summary["error"] = error.GetCString()
                    break
                for slot in range(256):
                    fields = {k: int.from_bytes(data[slot*size+offsets[k]:
                                                      slot*size+offsets[k]+4], "little")
                              for k in required}
                    if "vertex_generation" in offsets:
                        start=slot*size+offsets["vertex_generation"]
                        fields["vertex_generation"]=int.from_bytes(data[start:start+8],"little")
                    if fields["handle"]:
                        if fields["handle"] in bound_handles:
                            summary["bound_records"].append(fields)
                        summary["live"] += 1
                        summary["guest_bytes"] += fields["bytes"]
                        name = str(fields["type"])
                        summary["by_type"][name] = summary["by_type"].get(name, 0) + 1
        else:
            summary["error"] = "Invalid resource page count"
        out["native_resources"] = summary
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
