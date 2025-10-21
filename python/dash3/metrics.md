Great idea! You want to define **metrics with alarm levels** in `dash.json`, and have the system automatically create the associated variables (min, max, avg, count) for each alarm level.

Let me design a clean structure for this:

---

## ✅ Proposed `dash.json` structure for metrics

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
        {"level": 1, "min": 3.0, "max": 3.3, "severity": "info"},
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
        {"level": 1, "min": 20, "max": 40, "severity": "info"},
        {"level": 2, "min": 40, "max": 60, "severity": "warning"},
        {"level": 3, "min": 60, "max": 80, "severity": "critical"}
      ]
    }
  ]
}
```

---

## ✅ What variables get created?

For each metric, the system creates these variables in the database:

### For scalar metrics (no array):
```
rack_rtt_min_l1, rack_rtt_max_l1, rack_rtt_avg_l1, rack_rtt_count_l1
rack_rtt_min_l2, rack_rtt_max_l2, rack_rtt_avg_l2, rack_rtt_count_l2
rack_rtt_min_l3, rack_rtt_max_l3, rack_rtt_avg_l3, rack_rtt_count_l3
```

### For array metrics (e.g., 480 cells):
```
cell_volts[0]_min_l1, cell_volts[0]_max_l1, cell_volts[0]_avg_l1, cell_volts[0]_count_l1
cell_volts[0]_min_l2, ...
...
cell_volts[479]_min_l1, cell_volts[479]_max_l1, ...
```

**OR** if you want aggregated stats across all cells:
```
cell_volts_min_l1, cell_volts_max_l1, cell_volts_avg_l1, cell_volts_count_l1  (aggregated)
```

---

## ✅ Add database model for metrics

Add this to `db_server.py`:

```python
class MetricDef(Base):
    __tablename__ = "metric_defs"
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)
    label = Column(String, nullable=False)
    unit = Column(String, nullable=True)
    category = Column(String, nullable=True)
    array_size = Column(Integer, nullable=True)  # null = scalar, >0 = array
    alarm_levels = Column(JSON, nullable=False)  # [{level:1, min:0, max:50, severity:"info"}, ...]
```

---

## ✅ Sync function for metrics

Add this function to `db_server.py`:

```python
def sync_dash_metrics():
    """
    Ensure all metrics defined in dash.json exist in the database
    and create associated alarm variables (min, max, avg, count per level).
    """
    try:
        with open("dash.json", "r", encoding="utf-8") as f:
            dash = json.load(f)
    except Exception as e:
        print(f"Warning: could not load dash.json for metrics sync: {e}")
        return

    metrics = dash.get("metrics", [])
    if not metrics:
        print("No metrics defined in dash.json")
        return

    with SessionLocal() as db:
        prof = get_active_profile(db)
        default_area = "rtos"
        default_type = "hold"
        
        synced_metrics = []
        synced_vars = []
        
        for metric in metrics:
            name = metric.get("name")
            if not name:
                continue
            
            # 1. Store metric definition
            metric_def = db.query(MetricDef).filter_by(name=name).first()
            if not metric_def:
                metric_def = MetricDef(
                    name=name,
                    label=metric.get("label", name),
                    unit=metric.get("unit"),
                    category=metric.get("category"),
                    array_size=metric.get("array_size"),
                    alarm_levels=metric.get("alarm_levels", [])
                )
                db.add(metric_def)
                synced_metrics.append(name)
            
            # 2. Create alarm variables
            array_size = metric.get("array_size")
            alarm_levels = metric.get("alarm_levels", [])
            
            # Decide: per-element or aggregated?
            # Option A: Aggregated (one set of stats for entire array)
            if array_size is None or array_size == 0:
                # Scalar metric
                indices = [None]
            else:
                # Array metric - create aggregated stats only
                indices = [None]  # Change to range(array_size) for per-element
            
            for idx in indices:
                for level_def in alarm_levels:
                    level = level_def.get("level")
                    if level is None:
                        continue
                    
                    # Create variable names
                    if idx is None:
                        prefix = name
                    else:
                        prefix = f"{name}[{idx}]"
                    
                    var_names = [
                        f"{prefix}_min_l{level}",
                        f"{prefix}_max_l{level}",
                        f"{prefix}_avg_l{level}",
                        f"{prefix}_count_l{level}"
                    ]
                    
                    for var_name in var_names:
                        row = db.query(Variable).filter_by(
                            profile_id=prof.id,
                            mem_area=default_area,
                            mem_type=default_type,
                            offset=var_name
                        ).first()
                        
                        if row is None:
                            row = Variable(
                                profile_id=prof.id,
                                mem_area=default_area,
                                mem_type=default_type,
                                offset=var_name,
                                value="0"  # Initialize to 0
                            )
                            db.add(row)
                            synced_vars.append(var_name)
        
        db.commit()
        
        if synced_metrics:
            print(f"✅ Synced {len(synced_metrics)} metric definitions: {', '.join(synced_metrics)}")
        if synced_vars:
            print(f"✅ Created {len(synced_vars)} alarm variables")
```

---

## ✅ Update `init_db()` to call it

```python
def init_db():
    Base.metadata.create_all(engine)
    # ... existing code ...
    
    # ✅ Sync dash.json variables and metrics
    sync_dash_variables()
    sync_dash_metrics()
```

---

## ✅ API endpoint to query metrics

```python
@app.get("/api/metrics")
def list_metrics():
    """List all defined metrics with their alarm levels."""
    with SessionLocal() as db:
        metrics = db.query(MetricDef).all()
        return [
            {
                "name": m.name,
                "label": m.label,
                "unit": m.unit,
                "category": m.category,
                "array_size": m.array_size,
                "alarm_levels": m.alarm_levels
            }
            for m in metrics
        ]

@app.get("/api/metrics/{name}/alarms")
def get_metric_alarms(name: str, profile: Optional[str] = None):
    """Get current alarm variable values for a metric."""
    with SessionLocal() as db:
        if profile:
            prof = db.query(EnvProfile).filter_by(name=profile).first()
            if not prof:
                raise HTTPException(404, f"Profile '{profile}' not found")
        else:
            prof = get_active_profile(db)
        
        metric_def = db.query(MetricDef).filter_by(name=name).first()
        if not metric_def:
            raise HTTPException(404, f"Metric '{name}' not found")
        
        # Collect all alarm variables
        vars_pattern = f"{name}_%_l%"
        rows = db.query(Variable).filter(
            Variable.profile_id == prof.id,
            Variable.offset.like(vars_pattern.replace("%", "%%"))
        ).all()
        
        result = {}
        for row in rows:
            result[row.offset] = row.value
        
        return {
            "metric": name,
            "profile": prof.name,
            "alarms": result
        }
```

---

## 🎯 Design decision: Per-element vs Aggregated

For **480 cells**, do you want:

**Option A: Aggregated (recommended)**
- One set of stats for all cells combined
- `cell_volts_min_l1`, `cell_volts_max_l1`, etc.
- 12 variables total (4 stats × 3 levels)

**Option B: Per-element**
- Stats for each individual cell
- `cell_volts[0]_min_l1`, `cell_volts[1]_min_l1`, ..., `cell_volts[479]_min_l1`
- 5,760 variables (480 cells × 4 stats × 3 levels)

I'd recommend **Option A** unless you need per-cell alarm tracking. Let me know which you prefer! 🚀

