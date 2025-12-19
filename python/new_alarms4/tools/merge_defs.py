#!/usr/bin/env python3
"""
merge_defs.py

Usage:
  python merge_defs.py --alarms alarms.json --limits limit_defs.json \
      --out_alarms combined_alarms.json --out_limits limit_defs_out.json

If a limit_def is missing for an alarm, a dummy limit_def will be created and
appended to the output limit_defs JSON.
"""

import json
import argparse
import copy
from typing import Dict, Any, List, Optional


def parse_sm_id(sm_id: str):
    """
    Parse sm_id like "rtos:mb_hold:at_total_v_over" into (sm_name, reg_type, offset)
    If parsing fails, return (None, None, None)
    """
    if not sm_id:
        return None, None, None
    parts = sm_id.split(':')
    if len(parts) >= 3:
        sm_name = parts[0]
        reg_type = parts[1]
        offset = ':'.join(parts[2:])  # keep remainder as offset
        return sm_name, reg_type, offset
    return None, None, None

def find_name_from_alarm(alarm: Dict[str, Any]) -> Optional[str]:
    """
    Extract the name key to match limit_defs items by 'name'.
    """
    ld = alarm.get('limit_defs') or {}
    name = ld.get('name')
    if name:
        return name

    # Fallback: use alarm's own name
    name = alarm.get('name') or alarm.get('alarm_name')
    if name:
        return name

    return None


def merge_files(alarms_file: str, limit_defs_file: str):
    with open(alarms_file, 'r', encoding='utf-8') as f:
        alarms_data = json.load(f)

    with open(limit_defs_file, 'r', encoding='utf-8') as f:
        limits_data = json.load(f)

    # Find the target_vars table
    target_vars_table = None
    for table_obj in limits_data:
        if table_obj.get('table') == 'target_vars':
            target_vars_table = table_obj
            break

    if target_vars_table is None:
        raise ValueError("No 'target_vars' table found in limit_defs JSON")

    limits_items = target_vars_table.get('items', [])

    # Build lookup by NAME (not offset)
    limit_lookup = {item.get('name'): item for item in limits_items if item.get('name')}

    # Proceed with merging
    alarms_out = copy.deepcopy(alarms_data)
    limits_out = copy.deepcopy(limits_data)

    added_dummies = []

    for alarm in alarms_out.get('alarms', []):
        name = find_name_from_alarm(alarm)
        if name and name in limit_lookup:
            limit_def_from_file = limit_lookup[name]
            alarm_limit_def = alarm.get('limit_defs', {})

            # Merge with priority to limit_def_from_file
            merged_limit_def = {
                **alarm_limit_def,
                "name": limit_def_from_file.get("name"),
                "sm_name": limit_def_from_file.get("sm_name"),
                "reg_type": limit_def_from_file.get("reg_type"),
                "offset": limit_def_from_file.get("offset"),
                "num": limit_def_from_file.get("num"),
                "write_data": limit_def_from_file.get("write_data"),
                "read_data": limit_def_from_file.get("read_data"),
            }

            alarm['limit_defs'] = merged_limit_def
            alarm['_limit_def_matched'] = name
            alarm['limits'] = merged_limit_def['write_data']  # sync limits

        else:
            # Create dummy using name
            if not name:
                name = alarm.get('name') or alarm.get('alarm_name') or f"alarm_{alarm.get('num', 'unknown')}"
                name = str(name).strip()

            dummy = make_dummy_limit_def(alarm, name)  # reuse name as offset for dummy
            target_vars_table.setdefault('items', []).append(dummy)
            limit_lookup[name] = dummy
            added_dummies.append(name)

            alarm['limit_defs'] = dummy
            alarm['_limit_def_matched'] = name
            alarm['_limit_def_dummy'] = True
            alarm['limits'] = dummy['write_data']

    return alarms_out, limits_out, added_dummies

# not used
def find_offset_from_alarm(alarm: Dict[str, Any]) -> Optional[str]:
    """
    Try multiple places to extract an offset key that will match limit_defs items' 'offset'.
    """
    # If alarm.limit_defs.offset present, use that
    ld = alarm.get('limit_defs') or {}
    offset = ld.get('offset') or ld.get('sm_id')  # sm_id might be full sm_id
    if offset:
        # if sm_id form like rtos:mb_hold:at_total_v_over, extract trailing offset
        if ':' in offset:
            _, _, parsed_offset = parse_sm_id(offset)
            return parsed_offset
        return offset

    # fallback attempt: look for limits_var or limits_structure or limits_var name
    for candidate in ('limits_var', 'limits_structure', 'limit_var', 'limits'):
        v = alarm.get(candidate)
        if isinstance(v, str) and v:
            return v
        # if limits is an array of values, no offset here
    # no offset found
    return None


def make_dummy_limit_def(alarm: Dict[str, Any], offset: str, default_num: int = 4) -> Dict[str, Any]:
    """
    Create a dummy limit_def entry using alarm metadata.
    """
    name = alarm.get('name') or alarm.get('alarm_name') or f"alarm_{alarm.get('num', 'unknown')}"
    ld = alarm.get('limit_defs') or {}
    # Try parse sm_id from alarm.limit_defs or fallback to defaults
    sm_name = None
    reg_type = None
    if ld.get('sm_id'):
        sm_name, reg_type, parsed_offset = parse_sm_id(ld.get('sm_id'))
    # fallback defaults
    sm_name = sm_name or ld.get('sm_name') or 'rtos'
    reg_type = reg_type or ld.get('reg_type') or 'mb_hold'

    # Determine num
    num = int(ld.get('num') or ld.get('count') or alarm.get('levels') or default_num)

    # write_data preference order: limit_defs.write_data -> alarm.limit_defs.write_data -> alarm.limits -> zeros
    write_data = ld.get('write_data') or alarm.get('write_data') or alarm.get('limits') or None
    if write_data is None:
        # default zeros sized to num
        write_data = [0] * num
    else:
        # coerce/trim/expand to length num
        write_data = list(write_data)
        if len(write_data) < num:
            write_data = write_data + [0] * (num - len(write_data))
        elif len(write_data) > num:
            write_data = write_data[:num]

    read_data = ld.get('read_data') or [0] * num
    read_data = list(read_data)
    if len(read_data) < num:
        read_data = read_data + [0] * (num - len(read_data))
    elif len(read_data) > num:
        read_data = read_data[:num]

    dummy = {
        "name": name,
        "sm_name": sm_name,
        "reg_type": reg_type,
        "offset": offset,
        "num": num,
        "write_data": write_data,
        "read_data": read_data,
        "__dummy_created": True
    }
    return dummy

def xerge_files(alarms_file: str, limit_defs_file: str):
    with open(alarms_file, 'r', encoding='utf-8') as f:
        alarms_data = json.load(f)

    with open(limit_defs_file, 'r', encoding='utf-8') as f:
        limits_data = json.load(f)

    # limits_data is a list of tables, find the one with table == 'target_vars'
    target_vars_table = None
    for table_obj in limits_data:
        if table_obj.get('table') == 'target_vars':
            target_vars_table = table_obj
            break

    if target_vars_table is None:
        raise ValueError("No 'target_vars' table found in limit_defs JSON")

    limits_items = target_vars_table.get('items', [])

    # Build lookup by offset for limits_items
    limit_lookup = {item.get('offset'): item for item in limits_items if item.get('offset')}

    # Proceed with merging as before
    alarms_out = copy.deepcopy(alarms_data)
    limits_out = copy.deepcopy(limits_data)  # keep full structure for output

    added_dummies = []

    for alarm in alarms_out.get('alarms', []):
        offset = find_offset_from_alarm(alarm)
        if offset and offset in limit_lookup:
            limit_def_from_file = limit_lookup[offset]
            alarm_limit_def = alarm.get('limit_defs', {})
            # Build merged limit_defs with priority to limit_def_from_file for key fields
            merged_limit_def = {
                **alarm_limit_def,  # start with alarm's existing limit_defs
                "name": limit_def_from_file.get("name", alarm_limit_def.get("name")),
                "sm_name": limit_def_from_file.get("sm_name", alarm_limit_def.get("sm_name")),
                "reg_type": limit_def_from_file.get("reg_type", alarm_limit_def.get("reg_type")),
                "offset": limit_def_from_file.get("offset", alarm_limit_def.get("offset")),
                "num": limit_def_from_file.get("num", alarm_limit_def.get("num")),
                "write_data": limit_def_from_file.get("write_data", alarm_limit_def.get("write_data", [])),
                "read_data": limit_def_from_file.get("read_data", alarm_limit_def.get("read_data", [])),
            }

            alarm['limit_defs'] = merged_limit_def
            alarm['_limit_def_matched'] = offset

            # Also update alarm['limits'] to match write_data fully
            alarm['limits'] = merged_limit_def['write_data']
        else:
            # create dummy as before
            if not offset:
                smid = (alarm.get('limit_defs') or {}).get('sm_id')
                if smid:
                    _, _, parsed_offset = parse_sm_id(smid)
                    offset = parsed_offset

            if not offset:
                safe_off = alarm.get('name') or alarm.get('alarm_name') or f"alarm_{alarm.get('num', 'unknown')}"
                offset = str(safe_off).strip().replace(' ', '_').lower()

            dummy = make_dummy_limit_def(alarm, offset)
            # Append dummy to the target_vars table items
            target_vars_table.setdefault('items', []).append(dummy)
            limit_lookup[offset] = dummy
            added_dummies.append(offset)

            merged = {**(alarm.get('limit_defs') or {}), **dummy}
            alarm['limit_defs'] = merged
            alarm['_limit_def_matched'] = offset
            alarm['_limit_def_dummy'] = True

    return alarms_out, limits_out, added_dummies

def xerge_files(alarms_file: str, limit_defs_file: str):
    with open(alarms_file, 'r', encoding='utf-8') as f:
        alarms_data = json.load(f)

    with open(limit_defs_file, 'r', encoding='utf-8') as f:
        limits_data = json.load(f)

    # limits_data is a list of tables, find the one with table == 'target_vars'
    target_vars_table = None
    for table_obj in limits_data:
        if table_obj.get('table') == 'target_vars':
            target_vars_table = table_obj
            break

    if target_vars_table is None:
        raise ValueError("No 'target_vars' table found in limit_defs JSON")

    limits_items = target_vars_table.get('items', [])

    # Build lookup by offset for limits_items
    limit_lookup = {item.get('offset'): item for item in limits_items if item.get('offset')}

    # Work on copies
    alarms_out = copy.deepcopy(alarms_data)
    limits_out = copy.deepcopy(limits_data)

    # Build lookup by offset for limits_data['items']
    limits_items: List[Dict[str, Any]] = limits_out.get('items', [])
    limit_lookup: Dict[str, Dict[str, Any]] = {}
    for item in limits_items:
        off = item.get('offset')
        if off:
            limit_lookup[off] = item

    # Track which offsets were added as dummies
    added_dummies = []

    for alarm in alarms_out.get('alarms', []):
        offset = find_offset_from_alarm(alarm)
        if offset and offset in limit_lookup:
            # merge alarm.limit_defs with the matched limit item
            merged = {**(alarm.get('limit_defs') or {}), **limit_lookup[offset]}
            alarm['limit_defs'] = merged
            alarm['_limit_def_matched'] = offset
        else:
            # missing: create dummy
            if not offset:
                # try to derive offset from sm_id if present
                smid = (alarm.get('limit_defs') or {}).get('sm_id')
                if smid:
                    _, _, parsed_offset = parse_sm_id(smid)
                    offset = parsed_offset

            if not offset:
                # fallback: create an offset from the alarm name (safe key)
                safe_off = alarm.get('name') or alarm.get('alarm_name') or f"alarm_{alarm.get('num', 'unknown')}"
                # sanitize to a simple token (replace spaces)
                offset = str(safe_off).strip().replace(' ', '_').lower()

            dummy = make_dummy_limit_def(alarm, offset)
            # append dummy to limits_out items and update lookup
            limits_out.setdefault('items', []).append(dummy)
            limit_lookup[offset] = dummy
            added_dummies.append(offset)

            # merge into alarm
            merged = {**(alarm.get('limit_defs') or {}), **dummy}
            alarm['limit_defs'] = merged
            alarm['_limit_def_matched'] = offset
            alarm['_limit_def_dummy'] = True

    return alarms_out, limits_out, added_dummies


def main():
    parser = argparse.ArgumentParser(description="Merge alarms JSON and limit_defs JSON, add dummies where missing.")
    parser.add_argument('--alarms', '-a', required=True, help='Alarms JSON input file')
    parser.add_argument('--limits', '-l', required=True, help='Limit defs JSON input file')
    parser.add_argument('--out_alarms', '-o', default='combined_alarms.json', help='Output combined alarms file')
    parser.add_argument('--out_limits', '-L', default='limit_defs_out.json', help='Output updated limits file (with dummies appended)')
    args = parser.parse_args()

    alarms_out, limits_out, added_dummies = merge_files(args.alarms, args.limits)

    with open(args.out_alarms, 'w', encoding='utf-8') as f:
        json.dump(alarms_out, f, indent=2)
    with open(args.out_limits, 'w', encoding='utf-8') as f:
        json.dump(limits_out, f, indent=2)

    print(f"Saved combined alarms -> {args.out_alarms}")
    print(f"Saved updated limits -> {args.out_limits}")
    if added_dummies:
        print("Added dummy limit_defs for offsets:", added_dummies)
    else:
        print("No dummy limit_defs were needed.")


if __name__ == '__main__':
    main()