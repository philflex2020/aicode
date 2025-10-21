### Telemetry Dashboard System — Project Context

Last updated: 2025-10-21

#### Overview
This project is a FastAPI-based telemetry and dashboard system. It reads a declarative dashboard configuration (dash.json), synchronizes variables and metrics into a database per active profile, and serves time-series telemetry with category filtering to a frontend dashboard.

Core capabilities:
- Real-time telemetry API with dynamic discovery and category filters
- Auto-sync of variables and metrics (with alarm-level stats) from dash.json
- Per-profile variable management and audit history
- Frontend dashboard that polls telemetry and renders widgets (lines, dials, tables)

#### Components

- Backend (FastAPI)
  - Endpoints: /series (GET/POST), /vars (GET/POST), /api/sync_dash (POST), /api/metrics (GET), /api/metrics/{name}/alarms (GET)
  - Models: EnvProfile, MetaKV, Variable, TelemetryMeta, MetricDef, ProfileHistory
  - Startup: Seeds default profile, base telemetry, and syncs dash variables/metrics
- Frontend
  - dash.json defines tabs, widgets, controls, values, and metrics
  - app.js polls /series and updates widgets
  - Robust polling management with start/stop interval

#### Key Endpoints

- GET /series
  - Discovery mode (no names): returns available telemetry (optionally filter by ?category=network,system)
  - Data mode (names provided): returns time-series for names (optionally constrained by ?category=)
  - Params: names=mbps,cpu, window=300, category=network, profile=ignored
- POST /series
  - Adds or updates telemetry metadata (TelemetryMeta)
  - Body example: {"name":"cpu","label":"CPU Usage (%)","unit":"%","category":"system"}
- POST /api/sync_dash
  - Re-reads dash.json and synchronizes variables and metrics into the database for the active profile
- GET /api/metrics
  - Lists all metrics with alarm levels and definitions (from MetricDef)
- GET /api/metrics/{name}/alarms
  - Returns current alarm-related variable values for a metric and profile

#### Database Models

- EnvProfile: named environment profiles (default, prod, test)
- MetaKV: metadata KV store (e.g., active_profile)
- Variable: profile-scoped variables (mem_area, mem_type, offset, value)
- TelemetryMeta: telemetry series definitions (name, label, unit, category)
- MetricDef: metric families from dash.json (name, label, unit, category, array_size, alarm_levels JSON)
- ProfileHistory: audit log of profile actions

#### Startup Flow (init_db)
- Create tables
- Ensure default profile and active_profile KV
- Seed core telemetry into TelemetryMeta:
  - mbps, rtt_ms, temp_c, errors, drops, link_state, cpu
- Run:
  - sync_dash_variables()
  - sync_dash_metrics()

#### Sync Functions

- sync_dash_variables()
  - Scans dash.json controls/cards, values/fields, and vartable names
  - Ensures each referenced variable exists in Variable for active profile
  - Does not overwrite existing values; initializes new to provided defaults or null

- sync_dash_metrics()
  - Reads "metrics" array from dash.json
  - Upserts MetricDef entries
  - For each metric and each alarm level:
    - Scalar metrics: create min, max, avg, count per level
    - Array metrics: create aggregated sum, avg, max_val, max_idx, min_val, min_idx, count per level
  - Initializes numeric values to "0" and *_idx_* variables to "-1"

#### dash.json — Metrics Schema (Example)

```json
{
  "metrics": [
    {
      "name": "rack_rtt",
      "label": "Rack Round-Trip Time",
      "unit": "ms",
      "category": "network",
      "array_size": null,
      "alarm_levels": [
        {"level": 1, "min": 0, "max": 50, "severity": "info"},
        {"level": 2, "min": 50, "max": 100, "severity": "warning"},
        {"level": 3, "min": 100, "max": 999, "severity": "critical"}
      ]
    },
    {
      "name": "cell_volts",
      "label": "Cell Voltage",
      "unit": "V",
      "category": "power",
      "array_size": 480,
      "alarm_levels": [
        {"level": 1, "min": 3.0, "max": 3.3, "severity": "normal"},
        {"level": 2, "min": 3.3, "max": 3.6, "severity": "warning"},
        {"level": 3, "min": 3.6, "max": 4.2, "severity": "critical"}
      ]
    },
    {
      "name": "cell_temp",
      "label": "Cell Temperature",
      "unit": "°C",
      "category": "thermal",
      "array_size": 480,
      "alarm_levels": [
        {"level": 1, "min": 20, "max": 40, "severity": "normal"},
        {"level": 2, "min": 40, "max": 60, "severity": "warning"},
        {"level": 3, "min": 60, "max": 80, "severity": "critical"}
      ]
    }
  ]
}
```

#### Variable Naming Summary

- Scalar metric (per level X):
  - <name>_min_lX, <name>_max_lX, <name>_avg_lX, <name>_count_lX
- Array metric (per level X, aggregated across array):
  - <name>_sum_lX, <name>_avg_lX, <name>_max_val_lX, <name>_max_idx_lX, <name>_min_val_lX, <name>_min_idx_lX, <name>_count_lX

Examples:
- rack_rtt_avg_l2
- cell_volts_max_val_l3, cell_volts_max_idx_l3

#### Example API Usage

- List all telemetry (discovery):
```bash
curl -s "http://127.0.0.1:8085/series" | jq .
```

- Discovery by category (comma-separated supported):
```bash
curl -s "http://127.0.0.1:8085/series?category=network,system" | jq .
```

- Get time-series data for names with optional category filter:
```bash
curl -s "http://127.0.0.1:8085/series?names=mbps,cpu&window=20&category=system" | jq .
```

- Add a telemetry definition:
```bash
curl -X POST http://127.0.0.1:8085/series \
  -H "Content-Type: application/json" \
  -d '{"name":"cpu","label":"CPU Usage (%)","unit":"%","category":"system"}'
```

- Force sync of dash.json:
```bash
curl -X POST http://127.0.0.1:8085/api/sync_dash
```

- Explore metrics and alarms:
```bash
curl -s http://127.0.0.1:8085/api/metrics | jq .
curl -s http://127.0.0.1:8085/api/metrics/cell_volts/alarms | jq .
```

#### Frontend Notes

- Polling
  - POLL_T holds setInterval ID; startPolling() sets it, stopPolling() clears it
  - Ensure WIDGETS contains valid widget instances; guard against undefined entries in collectors
- Series Name Collection
  - collectSeriesNamesForActiveTab() should iterate over WIDGETS and safely handle missing cfg / series
- Debugging
  - Add console logs around pollOnce(), URL construction, and widget tick() to trace issues

#### Known Good Behavior

- curling /series with names returns mock time-series for registered TelemetryMeta
- Adding ?category filters discovery and constrains which names can return data
- CPU telemetry appears after POST /series for cpu and after backend includes cpu generator branch
- Frontend displays mbps line chart after fixing series name collection and widget instantiation guards

#### Roadmap

- Alarm computation engine
  - Background task to evaluate metrics per level and update alarm variables
- UI for alarms
  - Badges, colors, and summaries per metric
- Optional per-element stats for array metrics
  - Caution: large number of variables if enabled
- Real data ingestion (e.g., MQTT)
  - Replace mock data generation
- Dashboard builder
  - Edit dash.json visually and trigger API sync

#### Directory Layout

```
project/
├── db_server.py
├── main.py
├── dash.json
├── app.js
├── templates/
│   └── index.html
└── data/
    └── database.sqlite
```

#### Tips

- Keep dash.json the source of truth; run /api/sync_dash after edits
- Prefer aggregated stats for large arrays to keep variable counts manageable
- Use categories for organization and efficient discovery in the UI

If you want, I can also attach this as a downloadable CONTEXT.md in the chat.