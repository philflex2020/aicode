Application Check list

1/ remove db and restart

python3 app.py

question where does the no db start up get its data from

json/alarm_definition.py

change this with no db and make sure change is detected  OK

2/ test export to json

 cp json_exports/alarm_config.json json/alarm_definition.json

 check that db is ok


 3/ test import from json

 make sure that the file json_exports/alarm_config.json  is not present
press import , detect error

 make sure that the file json_exports/alarm_config.json  is present

press import , all should be good
  but its not
  



















Right now, a fresh “no DB” startup only bootstraps from **CSV**, not JSON:

# app.py -> bootstrap_databases()

alarm_count = AlarmDefinition.query.count()
if alarm_count == 0:
    print("🔄 alarms.db is empty, bootstrapping from alarm_definition.csv...")
    csv_paths = [
        os.path.join(base_dir, 'alarm_definition.csv'),
        os.path.join(base_dir, 'csv_exports', 'alarm_definition.csv'),
    ]
    ...

You want it to instead (or additionally) load defaults from:

- `json/alarm_definition.json`

Let’s wire that in.

---

### 1. Decide the JSON format

I’ll assume the JSON file is **exactly** what `/api/alarms/config` returns, i.e.:

```json
{
  "alarm_definitions": [ ... ],
  "alarm_level_actions": [ ... ],
  "limits_values": [ ... ]
}
```

If your `json/alarm_definition.json` is just a flat list of definitions, we can adapt, but this structure is ideal.

---

### 2. Add a JSON bootstrap helper in `alarm_utils.py`

You don’t have this yet, so add something like:

```python
# in alarm_utils.py
from datetime import datetime
from extensions import db
from alarm_models import AlarmDefinition, AlarmLevelAction, LimitsValues
from api_endpoints import normalize_comparison_type  # or reimplement the helper here

def load_alarm_defs_from_json(json_path: str, verbose: bool = False):
    """
    Load full alarm config (defs, levels, limits) from a JSON file in the same
    shape as /api/alarms/config.
    """
    import json

    if verbose:
        print(f"[load_alarm_defs_from_json] Loading from {json_path}")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    alarm_defs = data.get("alarm_definitions", [])
    alarm_levels = data.get("alarm_level_actions", [])
    limits_values = data.get("limits_values", [])

    # Clear existing tables
    AlarmLevelAction.query.delete()
    LimitsValues.query.delete()
    AlarmDefinition.query.delete()

    # Insert alarm definitions
    for ad_data in alarm_defs:
        if "comparison_type" in ad_data:
            ad_data["comparison_type"] = normalize_comparison_type(ad_data["comparison_type"])
        ad = AlarmDefinition(**ad_data)
        db.session.add(ad)

    # Insert levels (strip alarm_name if present)
    for ala_data in alarm_levels:
        ala_data_clean = {k: v for k, v in ala_data.items() if k != "alarm_name"}
        ala = AlarmLevelAction(**ala_data_clean)
        db.session.add(ala)

    # Insert limits (strip alarm_name and set last_updated)
    now_ts = datetime.utcnow().isoformat()
    for lv_data in limits_values:
        lv_data_clean = {k: v for k, v in lv_data.items() if k not in ("alarm_name", "last_updated")}
        lv_data_clean["last_updated"] = now_ts
        lv = LimitsValues(**lv_data_clean)
        db.session.add(lv)

    db.session.commit()

    if verbose:
        print(f"[load_alarm_defs_from_json] Loaded {len(alarm_defs)} defs, {len(alarm_levels)} levels, {len(limits_values)} limits.")
```

If you prefer not to import from `api_endpoints`, just copy the `normalize_comparison_type` logic into `alarm_utils.py`.

---

### 3. Change `bootstrap_databases` in `app.py` to use `json/alarm_definition.json`

Update the bootstrap to prefer JSON from `json/alarm_definition.json`, and fall back to CSV if missing.

Replace the alarms bootstrap block in `bootstrap_databases(app)` with:

```python
from alarm_utils import load_alarm_defs_from_csv, load_alarm_defs_from_json, load_data_defs_from_json
...
# --- alarms.db bootstrap ---
alarm_count = AlarmDefinition.query.count()
if alarm_count == 0:
    print("🔄 alarms.db is empty, bootstrapping from JSON/CSV...")

    json_paths = [
        os.path.join(base_dir, 'json', 'alarm_definition.json'),
        os.path.join(base_dir, 'alarm_definition.json'),
    ]
    json_file = next((p for p in json_paths if os.path.exists(p)), None)

    if json_file:
        print(f"   Found JSON: {json_file}")
        load_alarm_defs_from_json(json_file, verbose=True)
    else:
        print("   No JSON found, falling back to CSV...")
        csv_paths = [
            os.path.join(base_dir, 'alarm_definition.csv'),
            os.path.join(base_dir, 'csv_exports', 'alarm_definition.csv'),
        ]
        csv_file = next((p for p in csv_paths if os.path.exists(p)), None)
        if csv_file:
            print(f"   Found CSV: {csv_file}")
            load_alarm_defs_from_csv(csv_file, verbose=True)
        else:
            print(f"   ⚠️  No alarm_definition JSON or CSV found in: {json_paths + csv_paths}")
else:
    print(f"✅ alarms.db already has {alarm_count} alarm(s), skipping bootstrap.")
```

The `data.db` bootstrap can stay as-is, unless you also want to move it into a `json/` directory.

---

### 4. Practical test sequence

1. Stop `python3 app.py`
2. Delete `instance/alarms.db` and `instance/data.db`
3. Ensure you have:
   - `json/alarm_definition.json` (full config) present
4. Start:
   ```bash
   python3 app.py
   ```
5. Watch the console; you should see:
   - `🔄 alarms.db is empty, bootstrapping from JSON/CSV...`
   - `Found JSON: /.../json/alarm_definition.json`
   - `[load_alarm_defs_from_json] ...`

Then hit `/` in the browser; `Load from DB` should show the JSON defaults.

---

If you paste your current `alarm_utils.py`, I can drop in a **ready-to-paste** `load_alarm_defs_from_json` plus the exact `bootstrap_databases` block so you don’t have to hand-edit.