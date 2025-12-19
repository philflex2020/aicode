Nice — below is a ready-to-run Python script that:

- Loads your alarms JSON and limit_defs JSON.
- Merges limit_defs into each alarm (matching by offset).
- If a matching limit_def is missing, creates a sensible dummy limit_def (and appends it to the limit_defs output).
- Writes two outputs:
  - combined alarms JSON (alarms with merged/filled `limit_defs`)
  - updated limit_defs JSON (original items + any dummy entries added)

Save this as merge_limit_defs.py and run it from the command line.

```python
#!/usr/bin/env python3
"""
merge_limit_defs.py

Usage:
  python merge_limit_defs.py --alarms alarms.json --limits limit_defs.json \
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


def merge_files(alarms_file: str, limit_defs_file: str):
    with open(alarms_file, 'r', encoding='utf-8') as f:
        alarms_data = json.load(f)

    with open(limit_defs_file, 'r', encoding='utf-8') as f:
        limits_data = json.load(f)

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
```

Notes / behavior summary
- Matching is done by `offset`. For `sm_id` style strings like `rtos:mb_hold:at_total_v_over`, the trailing token `at_total_v_over` is used as the offset key.
- If the alarm has no offset, the script derives a safe offset from the alarm name (lowercased, spaces replaced with underscores).
- Dummy limit_defs get:
  - name: alarm name (or alarm_num fallback)
  - sm_name default 'rtos' (when missing)
  - reg_type default 'mb_hold' (when missing)
  - num derived from alarm.limit_defs.num or alarm.levels or default 4
  - write_data/read_data filled/truncated/expanded to `num`
  - an extra key `"__dummy_created": True` to mark dummies (you can remove that if you don't want it in the output)
- The script writes:
  - combined alarms JSON (where each alarm now has a populated `limit_defs`)
  - updated limit_defs JSON with any added dummies appended to `items`

If you'd like:
- a version that uses a different matching key (name instead of offset),
- to remove the `__dummy_created` marker,
- to auto-write specific default write_data values instead of zeros,
I can adjust the script. Want me to tweak any defaults?