# alarm_utils.py
import os
import csv
import json
from datetime import datetime
from extensions import db
from alarm_models import AlarmDefinition, AlarmLevelAction, LimitsValues

# Default CSV filenames
CSV_ALARM_DEF_FILE = 'alarm_definitions.csv'
CSV_ALARM_LEVEL_ACTIONS_FILE = 'alarm_level_actions.csv'
CSV_LIMITS_VALUES_FILE = 'limits_values.csv'
JSON_ALARM_DEF_FILE = 'alarm_definition.json'


def load_alarm_defs_from_json(json_path: str, verbose: bool = False):
    """
    Load alarm definitions from JSON file in the format:
    {
      "alarms": [
        {
          "num": 0,
          "name": "...",
          "levels": 3,
          "measured": "...",
          "limits": [0, 0, 0, 0],
          "compare": "...",
          "alarm_var": "...",
          "latch_var": "...",
          "notes": "...",
          "limits_def":"",
          "actions": [
            {
              "level": 1,
              "level_enabled": true,
              "duration": "0:0",
              "actions": [...],
              "notes": ""
            },
            ...
          ]
        },
        ...
      ]
    }
    """
    # import json
    # from datetime import datetime

    if verbose:
        print(f"[load_alarm_defs_from_json] Loading from {json_path}")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    alarms = data.get("alarms", [])

    # Clear existing tables
    AlarmLevelAction.query.delete()
    LimitsValues.query.delete()
    AlarmDefinition.query.delete()

    created_limits = set()
    now_ts = datetime.utcnow().isoformat()

    for alarm in alarms:
        alarm_num = alarm.get("num", 0)
        alarm_name = alarm.get("name", "")
        num_levels = alarm.get("levels", 1)
        measured_variable = alarm.get("measured", "")
        limits_array = alarm.get("limits", [0, 0, 0, 0])
        limits_var = alarm.get("limits_var", "")
        limits_def = alarm.get("limits_def", "")
        comparison_type = alarm.get("compare", "greater_than")
        alarm_variable = alarm.get("alarm_var", "")
        latched_variable = alarm.get("latch_var", "")
        notes = alarm.get("notes", "")

        limits_structure = limits_var
        if (limits_structure == ""):

            # Derive limits_structure name from alarm_name (or use a convention)
            # e.g., "Terminal Over Voltage" -> "terminal_over_voltage_limits"
            limits_structure = alarm_name.lower().replace(" ", "_") + "_limits" if alarm_name else ""

        # Normalize comparison_type
        comparison_type = normalize_comparison_type(comparison_type)

        # Create AlarmDefinition
        ad = AlarmDefinition(
            alarm_num=alarm_num,
            alarm_name=alarm_name,
            num_levels=num_levels,
            measured_variable=measured_variable if measured_variable != "none" else "",
            limits_structure=limits_structure,
            limits_def=limits_def,
            comparison_type=comparison_type,
            alarm_variable=alarm_variable,
            latched_variable=latched_variable,
            notes=notes
        )
        db.session.add(ad)

        # Create LimitsValues if limits_structure is defined and not already created
        if limits_structure and limits_structure not in created_limits:
            lv = LimitsValues(
                limits_structure=limits_structure,
                level1_limit=limits_array[0] if len(limits_array) > 0 else 0,
                level2_limit=limits_array[1] if len(limits_array) > 1 else 0,
                level3_limit=limits_array[2] if len(limits_array) > 2 else 0,
                hysteresis=limits_array[3] if len(limits_array) > 3 else 0,
                last_updated=now_ts,
                notes=""
            )
            db.session.add(lv)
            created_limits.add(limits_structure)

        # Create AlarmLevelAction entries
        actions_list = alarm.get("actions", [])
        # act = actions_list[0]
        # print(" action list 0 ", act)
        for action_def in actions_list:
            level = action_def.get("level", 1)
            level_enabled = action_def.get("level_enabled", True)
            duration_str = action_def.get("duration", "0:0")
            actions_data = action_def.get("actions", [])
            action_notes = action_def.get("notes", "")

            # Convert duration "0:0" to seconds (minutes:seconds)
            # try:
            #     parts = duration_str.split(":")
            #     duration_sec = int(parts[0]) * 60 + int(parts[1])
            # except:
            #     duration_sec = 0

            # Serialize actions as JSON string
            actions_json = json.dumps(actions_data)

            ala = AlarmLevelAction(
                alarm_num=alarm_num,
                level=level,
                enabled=level_enabled,
                duration=duration_str,
                actions=actions_json,
                notes=action_notes
            )
            db.session.add(ala)

    db.session.commit()

    if verbose:
        print(f"[load_alarm_defs_from_json] Loaded {len(alarms)} alarm definitions with levels and limits.")

        
def load_limits_def_from_json(json_path: str, verbose: bool = False):
    """
    Load limits_def entries from JSON file in the format:
    {
      "limits_def": [
        {
          "name": "...",
          "sm_name": "...",
          "reg_type": "...",
          "offset": ...,
          "num": ...,
          "write_data": ...,
          "read_data": ...
        },
        ...
      ]
    }
    """
    import json
    from alarm_models import LimitsDef
    from app import db

    if verbose:
        print(f"[load_limits_def_from_json] Loading from {json_path}")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    limits_defs = data.get("limits_def", [])

    for ld in limits_defs:
        limits_def = LimitsDef(
            name=ld.get("name", ""),
            sm_name=ld.get("sm_name"),
            reg_type=ld.get("reg_type"),
            offset=ld.get("offset"),
            num=ld.get("num"),
            write_data=ld.get("write_data"),
            read_data=ld.get("read_data")
        )
        db.session.add(limits_def)

    db.session.commit()

    if verbose:
        print(f"[load_limits_def_from_json] Loaded {len(limits_defs)} limits_def entries.")


def load_alarm_defs_from_csv(csv_file, verbose=False):
    """
    Load alarm definitions from CSV and auto-generate alarm_level_actions and limits_values.
    """
    if verbose:
        print(f"📂 Loading alarm definitions from: {csv_file}")
    
    if not os.path.exists(csv_file):
        print(f"❌ CSV file not found: {csv_file}")
        return
    
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        
        created_limits = set()
        alarm_count = 0
        level_count = 0
        limits_count = 0
        
        for row in reader:
            try:
                # Parse alarm definition
                alarm_num = int(row.get('alarm_num', 0))
                alarm_name = row.get('alarm_name', '').strip()
                num_levels = int(row.get('num_levels', 1))
                measured_variable = row.get('measured_variable', '').strip()
                limits_structure = row.get('limits_structure', '').strip()
                comparison_type = row.get('comparison_type', '').strip()
                alarm_variable = row.get('alarm_variable', '').strip()
                latched_variable = row.get('latched_variable', '').strip()
                notes = row.get('notes', '').strip()
                
                if not alarm_name:
                    if verbose:
                        print(f"⚠️  Skipping row with missing alarm_name: {row}")
                    continue
                
                # Check if alarm already exists
                existing = AlarmDefinition.query.filter_by(alarm_num=alarm_num).first()
                if existing:
                    if verbose:
                        print(f"⚠️  Alarm {alarm_num} ({alarm_name}) already exists, skipping.")
                    continue
                
                # Create alarm definition
                alarm_def = AlarmDefinition(
                    alarm_num=alarm_num,
                    alarm_name=alarm_name,
                    num_levels=num_levels,
                    measured_variable=measured_variable or None,
                    limits_structure=limits_structure or None,
                    comparison_type=comparison_type or None,
                    alarm_variable=alarm_variable or None,
                    latched_variable=latched_variable or None,
                    notes=notes or None
                )
                db.session.add(alarm_def)
                alarm_count += 1
                
                # Auto-generate alarm_level_actions
                for level in range(1, num_levels + 1):
                    level_action = AlarmLevelAction(
                        alarm_num=alarm_num,
                        level=level,
                        enabled=True,
                        duration="0:2",
                        actions='',
                        notes=''
                    )
                    db.session.add(level_action)
                    level_count += 1
                
                # Auto-generate limits_values if limits_structure is specified
                if limits_structure and limits_structure not in created_limits:
                    limits_value = LimitsValues(
                        limits_structure=limits_structure,
                        level1_limit=0,
                        level2_limit=0,
                        level3_limit=0,
                        hysteresis=0,
                        last_updated=datetime.utcnow().isoformat(),
                        notes=''
                    )
                    db.session.add(limits_value)
                    created_limits.add(limits_structure)
                    limits_count += 1
                
                if verbose:
                    print(f"✅ Created alarm {alarm_num}: {alarm_name} with {num_levels} level(s)")
            
            except Exception as e:
                if verbose:
                    print(f"❌ Error processing row {row}: {e}")
                continue
        
        db.session.commit()
        
        if verbose:
            print(f"\n📊 Import Summary:")
            print(f"   Alarms created: {alarm_count}")
            print(f"   Level actions created: {level_count}")
            print(f"   Limits structures created: {limits_count}")


import json
import os

from extensions import db
from alarm_models import DataDefinition  # or wherever your DataDefinition model lives

# import json
# import os

# from extensions import db
# from data_models import DataDefinition  # adjust import path if needed


def load_data_defs_from_json(json_path, verbose=True):
    """
    Load DataDefinition rows from a JSON file structured as:

    [
      { metadata... },
      { "table": "rack:sm16", "items": [ { "name": "...", "offset": ..., "size": ..., "type": "..." }, ... ] },
      ...
    ]

    - Skips any top-level object without a 'table' key.
    - 'items' must be a list of objects with at least 'name', 'offset', 'size'.
    - Maps JSON fields to DataDefinition columns:
        JSON "table" -> table_name
        JSON "name"  -> item_name
        JSON "type"  -> type
    """
    if verbose:
        print(f"📂 Loading data definitions from: {json_path}")

    if not os.path.exists(json_path):
        raise FileNotFoundError(f"Data definition JSON not found: {json_path}")

    with open(json_path, 'r') as f:
        try:
            root = json.load(f)
        except Exception as e:
            print(f"❌ Failed to parse JSON: {e}")
            raise

    if not isinstance(root, list):
        raise ValueError("Expected top-level JSON to be a list of objects")

    # Optionally clear old data
    DataDefinition.query.delete()
    db.session.commit()

    total_tables = 0
    total_items = 0

    for entry in root:
        # Skip metadata / non-table entries
        if not isinstance(entry, dict) or 'table' not in entry:
            continue

        table_name = entry.get('table')
        items = entry.get('items')

        if verbose:
            print(f"  Processing table: {table_name}")

        if not isinstance(items, list):
            print(f"    ❌ Error: 'items' is not a list for table {table_name}")
            continue

        total_tables += 1

        for item in items:
            if not isinstance(item, dict):
                print(f"    ❌ Skipping non-dict item in {table_name}: {item!r}")
                continue

            item_name = item.get('name')
            if not item_name:
                print(f"    ❌ Skipping item with no 'name' in {table_name}: {item!r}")
                continue

            offset = item.get('offset')
            size = item.get('size')
            item_type = item.get('type')  # optional
            notes = item.get('notes')     # optional

            dd = DataDefinition(
                table_name=table_name,
                item_name=item_name,
                offset=offset if offset is not None else 0,
                size=size if size is not None else 1,
                type=item_type,
                notes=notes
            )
            db.session.add(dd)
            total_items += 1

    db.session.commit()

    if verbose:
        print(f"📊 Data definition import complete.")
        print(f"   Tables processed: {total_tables}")
        print(f"   Items created: {total_items}")


VALID_COMPARISON_TYPES = {
    'greater_than',
    'less_than',
    'equal',
    'not_equal',
    'greater_or_equal',
    'less_or_equal',
}

def normalize_comparison_type(value, default='greater_than'):
    """
    Normalize comparison_type to one of the allowed values.
    Accepts some common variants and falls back to default if invalid/empty.
    """
    if not value:
        return default

    v = str(value).strip().lower()

    # direct matches
    if v in VALID_COMPARISON_TYPES:
        return v

    # aliases / shorthand
    aliases = {
        'gt': 'greater_than',
        '>': 'greater_than',
        'lt': 'less_than',
        '<': 'less_than',
        'eq': 'equal',
        '==': 'equal',
        '!=': 'not_equal',
        '<>': 'not_equal',
        'ge': 'greater_or_equal',
        '>=': 'greater_or_equal',
        'le': 'less_or_equal',
        '<=': 'less_or_equal',
        'aggregate': 'aggregate',
    }
    if v in aliases:
        return aliases[v]

    # last resort: default
    return default

# def load_data_defs_from_json(json_path, verbose=True):
#     """
#     Load DataDefinition rows from a JSON file structured as:

#     [
#       { metadata... },
#       { "table": "rack:sm16", "items": [ { "name": "...", "offset": ..., "size": ... }, ... ] },
#       ...
#     ]

#     - Skips any top-level object without a 'table' key.
#     - 'items' must be a list of objects with at least 'name', 'offset', 'size'.
#     """
#     if verbose:
#         print(f"[DATADEF] Loading data definitions from: {json_path}")

#     if not os.path.exists(json_path):
#         raise FileNotFoundError(f"Data definition JSON not found: {json_path}")

#     with open(json_path, 'r') as f:
#         try:
#             root = json.load(f)
#         except Exception as e:
#             print(f"[DATADEF] Failed to parse JSON: {e}")
#             raise

#     if not isinstance(root, list):
#         raise ValueError("Expected top-level JSON to be a list of objects")

#     # Optionally clear old data
#     DataDefinition.query.delete()
#     db.session.commit()

#     total_tables = 0
#     total_items = 0

#     for entry in root:
#         # Skip metadata / non-table entries
#         if not isinstance(entry, dict) or 'table' not in entry:
#             continue

#         table_name = entry.get('table')
#         items = entry.get('items')

#         if verbose:
#             print(f"[DATADEF] Processing table: {table_name}")

#         if not isinstance(items, list):
#             print(f"❌ Error processing table {table_name}: 'items' is not a list")
#             continue

#         total_tables += 1

#         for item in items:
#             if not isinstance(item, dict):
#                 print(f"  ❌ Skipping non-dict item in {table_name}: {item!r}")
#                 continue

#             name = item.get('name')
#             if not name:
#                 print(f"  ❌ Skipping item with no 'name' in {table_name}: {item!r}")
#                 continue

#             offset = item.get('offset')
#             size = item.get('size')

#             # You might have extra fields like 'signed', 'type', etc; adapt as needed
#             dd = DataDefinition(
#                 table=table_name,
#                 name=name,
#                 offset=offset,
#                 size=size,
#                 datatype=item.get('datatype'),      # if present in JSON
#                 units=item.get('units'),            # if present
#                 description=item.get('description') # if present
#             )
#             db.session.add(dd)
#             total_items += 1

#     db.session.commit()

#     if verbose:
#         print(f"[DATADEF] Import complete. Tables: {total_tables}, items: {total_items}")

def xload_data_defs_from_json(json_file, verbose=False):
    """
    Load data definitions from JSON file into DataDefinition table.
    """
    import json
    from alarm_models import DataDefinition
    
    if verbose:
        print(f"📂 Loading data definitions from: {json_file}")
    
    if not os.path.exists(json_file):
        print(f"❌ JSON file not found: {json_file}")
        return
    
    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    # Handle both single object and array of objects
    if isinstance(data, dict):
        data = [data]
    
    total_items = 0
    total_size = 0
    skipped = 0
    
    for obj in data:
        # Skip objects without 'table' key or empty 'table'
        if 'table' not in obj or not obj.get('table', '').strip():
            skipped += 1
            continue
        
        try:
            table_name = obj.get('table', '').strip()
            items = obj.get('items', {})
            
            for item_name, item_data in items.items():
                data_type = item_data.get('type', 'unknown')
                offset = item_data.get('offset', 0)
                size = item_data.get('size', 0)
                
                # Upsert logic
                existing = DataDefinition.query.filter_by(
                    table=table_name,
                    item=item_name
                ).first()
                
                if existing:
                    existing.data_type = data_type
                    existing.offset = offset
                    existing.size = size
                else:
                    data_def = DataDefinition(
                        table=table_name,
                        item=item_name,
                        data_type=data_type,
                        offset=offset,
                        size=size
                    )
                    db.session.add(data_def)
                
                total_items += 1
                total_size += size
        
        except Exception as e:
            if verbose:
                print(f"❌ Error processing object {obj.get('table', 'unknown')}: {e}")
            skipped += 1
            continue
    
    db.session.commit()
    
    if verbose:
        print(f"\n📊 Import Summary:")
        print(f"   Total items: {total_items}")
        print(f"   Total size: {total_size} bytes")
        print(f"   Skipped: {skipped}")


def export_alarm_definitions_to_csv(directory):
    """
    Export alarm_definitions to CSV in the specified directory.
    """
    os.makedirs(directory, exist_ok=True)
    csv_path = os.path.join(directory, CSV_ALARM_DEF_FILE)
    
    alarms = AlarmDefinition.query.order_by(AlarmDefinition.alarm_num).all()
    
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        fieldnames = [
            'alarm_num', 'alarm_name', 'num_levels', 'measured_variable',
            'limits_structure', 'comparison_type', 'alarm_variable',
            'latched_variable', 'notes'
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        
        for alarm in alarms:
            writer.writerow({
                'alarm_num': alarm.alarm_num,
                'alarm_name': alarm.alarm_name,
                'num_levels': alarm.num_levels,
                'measured_variable': alarm.measured_variable or '',
                'limits_structure': alarm.limits_structure or '',
                'comparison_type': alarm.comparison_type or '',
                'alarm_variable': alarm.alarm_variable or '',
                'latched_variable': alarm.latched_variable or '',
                'notes': alarm.notes or ''
            })
    
    print(f"✅ Exported {len(alarms)} alarm definitions to {csv_path}")


def export_alarm_level_actions_to_csv(directory):
    """
    Export alarm_level_actions to CSV in the specified directory.
    """
    os.makedirs(directory, exist_ok=True)
    csv_path = os.path.join(directory, CSV_ALARM_LEVEL_ACTIONS_FILE)
    
    # Build alarm_num -> alarm_name lookup
    alarms = AlarmDefinition.query.all()
    alarm_name_lookup = {a.alarm_num: a.alarm_name for a in alarms}
    
    levels = AlarmLevelAction.query.order_by(
        AlarmLevelAction.alarm_num,
        AlarmLevelAction.level
    ).all()
    
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        fieldnames = [
            'alarm_num', 'alarm_name', 'level', 'enabled',
            'duration', 'actions', 'notes'
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        
        for level in levels:
            writer.writerow({
                'alarm_num': level.alarm_num,
                'alarm_name': alarm_name_lookup.get(level.alarm_num, 'N/A'),
                'level': level.level,
                'enabled': 1 if level.enabled else 0,
                'duration': level.duration or "0:1",
                'actions': level.actions or '',
                'notes': level.notes or ''
            })
    
    print(f"✅ Exported {len(levels)} alarm level actions to {csv_path}")


def export_limits_values_to_csv(directory):
    """
    Export limits_values to CSV in the specified directory.
    """
    os.makedirs(directory, exist_ok=True)
    csv_path = os.path.join(directory, CSV_LIMITS_VALUES_FILE)
    
    # Build limits_structure -> alarm_name lookup
    alarms = AlarmDefinition.query.all()
    limits_to_alarm_name = {}
    for alarm in alarms:
        if alarm.limits_structure and alarm.limits_structure not in limits_to_alarm_name:
            limits_to_alarm_name[alarm.limits_structure] = alarm.alarm_name
    
    limits = LimitsValues.query.order_by(LimitsValues.limits_structure).all()
    
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        fieldnames = [
            'limits_structure', 'alarm_name', 'level1_limit', 'level2_limit',
            'level3_limit', 'hysteresis', 'last_updated', 'notes'
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        
        for limit in limits:
            writer.writerow({
                'limits_structure': limit.limits_structure,
                'alarm_name': limits_to_alarm_name.get(limit.limits_structure, 'N/A'),
                'level1_limit': limit.level1_limit or 0,
                'level2_limit': limit.level2_limit or 0,
                'level3_limit': limit.level3_limit or 0,
                'hysteresis': limit.hysteresis or 0,
                'last_updated': limit.last_updated or '',
                'notes': limit.notes or ''
            })
    
    print(f"✅ Exported {len(limits)} limits values to {csv_path}")


def ensure_alarm_records_exist(alarm_def):
    """
    Ensure that alarm_level_actions and limits_values exist for a given alarm.
    Auto-creates them if missing.
    Returns (created_levels_count, created_limits_bool)
    """
    created_levels = 0
    created_limits = False
    
    # Check and create alarm_level_actions
    for level in range(1, alarm_def.num_levels + 1):
        existing = AlarmLevelAction.query.filter_by(
            alarm_num=alarm_def.alarm_num,
            level=level
        ).first()
        
        if not existing:
            level_action = AlarmLevelAction(
                alarm_num=alarm_def.alarm_num,
                level=level,
                enabled=True,
                duration='3:3',
                actions='',
                notes=''
            )
            db.session.add(level_action)
            created_levels += 1
    
    # Check and create limits_values
    if alarm_def.limits_structure:
        existing_limit = LimitsValues.query.filter_by(
            limits_structure=alarm_def.limits_structure
        ).first()
        
        if not existing_limit:
            limits_value = LimitsValues(
                limits_structure=alarm_def.limits_structure,
                level1_limit=0,
                level2_limit=0,
                level3_limit=0,
                hysteresis=0,
                last_updated=datetime.utcnow().isoformat(),
                notes=''
            )
            db.session.add(limits_value)
            created_limits = True
    
    if created_levels > 0 or created_limits:
        db.session.commit()
    
    return created_levels, created_limits


def ensure_all_alarms_have_records(verbose=False):
    """
    Iterate through all alarm definitions and ensure they have
    the required alarm_level_actions and limits_values records.
    """
    alarms = AlarmDefinition.query.all()
    total_levels_created = 0
    total_limits_created = 0
    
    for alarm in alarms:
        levels_created, limits_created = ensure_alarm_records_exist(alarm)
        total_levels_created += levels_created
        total_limits_created += 1 if limits_created else 0
        
        if verbose and (levels_created > 0 or limits_created):
            print(f"✅ Auto-created records for alarm {alarm.alarm_num} ({alarm.alarm_name}): "
                  f"{levels_created} level(s), {1 if limits_created else 0} limit(s)")
    
    if verbose:
        print(f"\n📊 Auto-creation Summary:")
        print(f"   Level actions created: {total_levels_created}")
        print(f"   Limits structures created: {total_limits_created}")
    
    return total_levels_created, total_limits_created