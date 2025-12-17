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
    Compact duration. If only one csv 'duration' field exists, 
    we map duration -> "duration:duration".
    If on/off are separate, map -> "on:off".
    """
    on_i = parse_int(on, 0)
    if off is None:
        return f"{on_i}:{on_i}"
    off_i = parse_int(off, on_i)
    return f"{on_i}:{off_i}"


def generate_default_actions(alarm_name: str, level: int) -> List[Dict[str, List[str]]]:
    """
    Generate default when_set / when_clear actions for a level.
    Format: sanitized alarm name + level number.
    """
    # Sanitize alarm name: lowercase, replace spaces with underscores
    safe_name = alarm_name.lower().replace(" ", "_").replace("-", "_")
    
    return [
        {
            "when_set": [
                f"set_{safe_name}_level{level}",
                f"log:{safe_name.upper()}_L{level}_ACTIVE",
                f"notify:ui_warning:{alarm_name} Level {level} active"
            ]
        },
        {
            "when_clear": [
                f"reset_{safe_name}_level{level}",
                f"log:{safe_name.upper()}_L{level}_CLEAR"
            ]
        }
    ]


def convert_csv_to_alarm_json(
    alarm_defs_csv: str,
    level_actions_csv: Optional[str],
    limits_csv: Optional[str],
    json_out: str,
    vars_dict: Optional[Dict[str, Any]] = None,
) -> None:
    """
    Convert the 3 alarm CSVs into a single alarm_definitions.json in the compact format.
    
    New features:
    - Auto-generates default when_set/when_clear actions if none defined
    - Adds level_enabled field to each level
    - Always includes limits array (zeros if not defined)
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
                "limits_array": [0, 0, 0, 0],  # default zeros, overwritten in step 3
            }

    # 2. Load level actions (if provided)
    level_actions_loaded: Dict[int, Dict[int, Dict[str, Any]]] = {}  # alarm_num -> level -> data
    
    if level_actions_csv:
        level_actions_csv = Path(level_actions_csv)
        if level_actions_csv.exists():
            with level_actions_csv.open(newline="", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    alarm_num = parse_int(row.get("alarm_num"))
                    if alarm_num not in alarms_by_num:
                        continue

                    level = parse_int(row.get("level"), 0)
                    enabled = parse_bool(row.get("enabled"))
                    duration_val = row.get("duration", "0")
                    duration = parse_duration(duration_val)

                    # Parse actions from CSV (comma or semicolon separated)
                    raw_actions = (row.get("actions") or "").strip()
                    notes = (row.get("notes") or "").strip()

                    action_entries: List[Dict[str, Any]] = []
                    if raw_actions:
                        tokens = [t.strip() for t in raw_actions.replace(";", ",").split(",") if t.strip()]
                        if tokens:
                            action_entries.append({"when_set": tokens})

                    if alarm_num not in level_actions_loaded:
                        level_actions_loaded[alarm_num] = {}
                    
                    level_actions_loaded[alarm_num][level] = {
                        "level": level,
                        "level_enabled": enabled,
                        "duration": duration,
                        "actions": action_entries,
                        "notes": notes,
                    }

    # 2b. Generate actions for all levels (use loaded or generate defaults)
    for alarm_num, alarm in alarms_by_num.items():
        num_levels = alarm["levels"]
        alarm_name = alarm["name"]
        
        for level in range(1, num_levels + 1):
            if alarm_num in level_actions_loaded and level in level_actions_loaded[alarm_num]:
                # Use loaded action
                level_obj = level_actions_loaded[alarm_num][level]
                
                # If no actions defined, generate defaults
                if not level_obj["actions"]:
                    level_obj["actions"] = generate_default_actions(alarm_name, level)
            else:
                # Generate default level action
                level_obj = {
                    "level": level,
                    "level_enabled": True,  # Default enabled
                    "duration": "0:0",
                    "actions": generate_default_actions(alarm_name, level),
                    "notes": "",
                }
            
            alarm["actions"].append(level_obj)

    # 3. Load limits values (if provided)
    if limits_csv:
        limits_csv = Path(limits_csv)
        if limits_csv.exists():
            with limits_csv.open(newline="", encoding="utf-8") as f:
                reader = csv.DictReader(f)
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
        
        # Rename 'limits_array' -> 'limits' (always present now)
        a["limits"] = a.pop("limits_array")

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
    
    print(f"✅ Wrote {json_out}")
    print(f"   - {len(alarms_json)} alarms")
    total_actions = sum(len(a["actions"]) for a in alarms_json)
    print(f"   - {total_actions} level actions (with defaults)")


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

    # Example vars table
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
