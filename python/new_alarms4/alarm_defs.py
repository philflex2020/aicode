import csv
import json
from pathlib import Path
from typing import Dict, Any, List, Optional


def parse_bool(val: str) -> bool:
    if val is None:
        return False
    return str(val).strip() not in ("0", "", "false", "False", "FALSE", "no", "No")


def parse_int(val: str, default: int = 0) -> int:
    try:
        return int(val)
    except Exception:
        return default


def parse_duration(on: str, off: Optional[str] = None) -> str:
    """
    Compact duration . If only one csv 'duration' field exists, 
    we map duration -> "duration:duration".
    If on/off are separate, map -> "on:off".
    """
    on_i = parse_int(on, 0)
    if off is None:
        return f"{on_i}:{on_i}"
    off_i = parse_int(off, on_i)
    return f"{on_i}:{off_i}"


def convert_csv_to_alarm_json(
    alarm_defs_csv: str,
    level_actions_csv: Optional[str],
    limits_csv: Optional[str],
    json_out: str,
    vars_dict: Optional[Dict[str, Any]] = None,
) -> None:
    """
    Convert the 3 alarm CSVs into a single alarm_definitions.json in the compact format:
    
    {
      "vars": { ... optional ... },
      "alarms": [
        {
          "num": 0,
          "name": "...",
          "levels": 3,
          "measured": "...",
          "limits": "rtos:hold:...",
          "compare": "aggregate",
          "alarm_var": "...",
          "latch_var": "...",
          "notes": "some notes",
          "actions": [
            {
              "level": 1,
              "enabled": true,
              "duration": "3:3",
              "actions": [
                {"when_set": [...]},
                {"when_clear": [...]}
              ],
              "notes": ""
            },
            ...
          ],
          "limits": [l1, l2, l3, hyst]
        }
      ]
    }
    """
    alarm_defs_csv = Path(alarm_defs_csv)
    json_out = Path(json_out)

    # 1. Load alarm definitions
    alarms_by_num: Dict[int, Dict[str, Any]] = {}

    with alarm_defs_csv.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            alarm_num = parse_int(row.get("alarm_num"))
            name = row.get("alarm_name", "").strip()
            num_levels = parse_int(row.get("num_levels"), 0)
            measured = row.get("measured_variable", "").strip()
            limits_ref = row.get("limits_structure", "").strip()
            compare = row.get("comparison_type", "").strip()
            alarm_var = row.get("alarm_variable", "").strip()
            latch_var = row.get("latched_variable", "").strip()
            notes = row.get("notes", "").strip()

            alarms_by_num[alarm_num] = {
                "num": alarm_num,
                "name": name,
                "levels": num_levels,
                "measured": measured,
                "limits": limits_ref,
                "compare": compare,
                "alarm_var": alarm_var,
                "latch_var": latch_var,
                "notes": notes,
                "actions": [],   # filled in step 2
                "limits_array": None,  # filled in step 3, then renamed to "limits"
            }

    # 2. Load level actions (if provided)
    if level_actions_csv:
        level_actions_csv = Path(level_actions_csv)
        if level_actions_csv.exists():
            with level_actions_csv.open(newline="", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                # expected: alarm_num,alarm_name,level,enabled,duration,actions,notes
                for row in reader:
                    alarm_num = parse_int(row.get("alarm_num"))
                    if alarm_num not in alarms_by_num:
                        continue

                    level = parse_int(row.get("level"), 0)
                    enabled = parse_bool(row.get("enabled"))
                    duration_val = row.get("duration", "0")
                    duration = parse_duration(duration_val)  # "on:off" compact

                    # 'actions' here is (probably) a flat string from old CSV.
                    # For now, we map it into a single when_set list if non-empty.
                    raw_actions = (row.get("actions") or "").strip()
                    notes = (row.get("notes") or "").strip()

                    action_entries: List[Dict[str, Any]] = []
                    if raw_actions:
                        # naïve split by ';' or ','; adjust to your old format
                        tokens = [t.strip() for t in raw_actions.replace(";", ",").split(",") if t.strip()]
                        if tokens:
                            action_entries.append({"when_set": tokens})

                    level_obj = {
                        "level": level,
                        "enabled": enabled,
                        "duration": duration,
                        "actions": action_entries,  # compact trigger structure
                        "notes": notes,
                    }

                    alarms_by_num[alarm_num]["actions"].append(level_obj)

    # 3. Load limits values (if provided)
    if limits_csv:
        limits_csv = Path(limits_csv)
        if limits_csv.exists():
            with limits_csv.open(newline="", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                # expected: alarm_num,alarm_name,limits_structure,
                #           level_1,level_2,level_3,hysteresis,last_updated,notes
                for row in reader:
                    alarm_num = parse_int(row.get("alarm_num"))
                    if alarm_num not in alarms_by_num:
                        continue

                    l1 = parse_int(row.get("level_1"), 0)
                    l2 = parse_int(row.get("level_2"), 0)
                    l3 = parse_int(row.get("level_3"), 0)
                    hyst = parse_int(row.get("hysteresis"), 0)
                    alarms_by_num[alarm_num]["limits_array"] = [l1, l2, l3, hyst]

    # 4. Normalize alarms list for JSON
    alarms_json: List[Dict[str, Any]] = []
    for alarm_num in sorted(alarms_by_num.keys()):
        a = alarms_by_num[alarm_num]
        # Rename 'limits_array' -> 'limits' and drop None if not present
        limits_array = a.pop("limits_array", None)
        if limits_array is not None:
            a["limits"] = limits_array
        else:
            # If you prefer always having "limits", uncomment the next line:
            # a["limits"] = [0, 0, 0, 0]
            pass

        # Sort actions by level for consistency
        a["actions"] = sorted(a["actions"], key=lambda lv: lv.get("level", 0))

        alarms_json.append(a)

    # 5. Build final output structure
    out: Dict[str, Any] = {"alarms": alarms_json}
    if vars_dict:
        out["vars"] = vars_dict

    json_out.parent.mkdir(parents=True, exist_ok=True)
    with json_out.open("w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    print(f"Wrote {json_out}")


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print(
            "Usage: python convert_alarms.py alarm_definitions.csv alarm_definitions.json "
            "[alarm_level_actions.csv] [limits_values.csv]"
        )
        sys.exit(1)

    alarm_defs_csv = sys.argv[1]
    json_out = sys.argv[2]
    level_actions_csv = sys.argv[3] if len(sys.argv) > 3 else None
    limits_csv = sys.argv[4] if len(sys.argv) > 4 else None

    # Example vars table: you can hard-code, load from a file, or pass via env/args
    vars_table = {
        "num_racks": 14,
        "max_string_voltage": 1000,
        "site_name": "Demo Site A"
    }

    convert_csv_to_alarm_json(
        alarm_defs_csv=alarm_defs_csv,
        level_actions_csv=level_actions_csv,
        limits_csv=limits_csv,
        json_out=json_out,
        vars_dict=vars_table,
    )



