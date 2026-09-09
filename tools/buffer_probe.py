"""Read-only LLDB capture of original game VB registry and tangent pools.

Import in a stopped game, then buffer_probe.dump(lldb.debugger, absolute_path).
No target expression is executed. Fixed original tables are bounded by 39670
(0xDAC0 DWORD registry initialization) and 3A550 (1000 pool records).
"""
import json
from pathlib import Path
import lldb


def dump(debugger, destination):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    base = target.FindFirstGlobalVariable('g_xbox_mem_offset').GetValueAsUnsigned()
    errors = {}

    def words(address, count):
        if not 0x1000 <= address <= 0x4000000 - count * 4:
            return None
        error = lldb.SBError()
        data = process.ReadMemory(base + address, count * 4, error)
        if error.Fail() or len(data) != count * 4:
            errors[hex(address)] = str(error)
            return None
        return [int.from_bytes(data[i:i+4], 'little')
                for i in range(0, len(data), 4)]

    streams = [words(0x10F280 + i * 12, 3) for i in range(16)]
    handles = {record[2] for record in streams if record and record[2]}
    registry = words(0x1E8D78, 0xDAC0)
    matches = []
    if registry:
        for i in range(0, len(registry), 8):
            row = registry[i:i+8]
            if row[3] in handles or row[4] in handles:
                matches.append({'game_handle': i // 8 + 1,
                                'address': 0x1E8D78 + i * 4,
                                'words': row})
    match_ids = {row['game_handle'] for row in matches}
    pools = words(0x1E3E90, 1000 * 5)
    active_pools = []
    if pools:
        for i in range(0, len(pools), 5):
            row = pools[i:i+5]
            if row[0]:
                active_pools.append({'address': 0x1E3E90 + i * 4,
                                     'matches_bound': row[0] in match_ids,
                                     'words': row})
    # B8690 walks scene instances (+18 count,+1C 80-byte records), uses each
    # instance's +40 object index in the scene +14 pointer table, then follows
    # the object's +0C geometry list. Capture only geometries using a bound VB.
    scene_pointer = words(0x23C2E0, 1)
    scene = words(scene_pointer[0], 8) if scene_pointer else None
    geometries = []
    visited = set()
    if scene and scene[6] <= 4096:
        for index in range(scene[6]):
            instance = words(scene[7] + index * 80, 20)
            if not instance or instance[16] > 4096:
                continue
            object_pointer = words(scene[5] + instance[16] * 4, 1)
            obj = words(object_pointer[0], 4) if object_pointer else None
            geometry = obj[3] if obj else 0
            for _ in range(4096):
                if not geometry or geometry in visited:
                    break
                visited.add(geometry)
                data = words(geometry, 24)
                if not data:
                    break
                if data[7] in match_ids:
                    geometries.append({'address': geometry, 'words': data,
                                       'material': words(data[1], 32),
                                       'instance_index': index,
                                       'instance': instance})
                geometry = data[0]
                if len(visited) >= 8192:
                    break
            if len(visited) >= 8192:
                break
    out = {'stream_records': streams, 'registry_matches': matches,
           'active_pools': active_pools, 'errors': errors,
           'scene_address': scene_pointer, 'scene_header': scene,
           'matching_geometries': geometries,
           'reflection_manager_record': words(0x426CB8 + 27 * 20 + 16, 5)}
    path = Path(destination)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(out, indent=2) + '\n')
    print('[BUFFER-PROBE]', path, 'registry_matches=', len(matches),
          'active_pools=', len(active_pools))
