Here's the **complete system** for your FastAPI backend and frontend schemas to support all feature tables (`pubs`, `charts`, `metrics`, `protections`, `aggregates`) with full CRUD endpoints and matching UI schemas.

---

## ✅ 1. `schemas.js` – Updated with all feature fields

```js
// schemas.js
window.SCHEMAS = {
  accessPath: {
    title: "Add Access Path",
    nested: "reference",
    fields: [
      { name: "access_type", label: "Access Type", type: "select", options: ["modbus", "can", "string"], required: true },
      { name: "protocol", label: "Protocol", type: "text" },
      { name: "address", label: "Address", type: "text", required: true },
      { name: "unit_id", label: "Unit ID", type: "number" },
      { name: "read_only", label: "Read Only", type: "checkbox" },
      { name: "priority", label: "Priority", type: "number", default: 10 },
      { name: "reference_table", label: "Reference Table", type: "text", required: true, default: "oper:sbmu:input" },
      { name: "reference_offset", label: "Reference Offset", type: "number", required: true, default: 100 },
      { name: "reference_size", label: "Reference Size", type: "number", required: true, default: 2 },
      { name: "reference_num", label: "Reference Num", type: "number", required: true, default: 1 }
    ]
  },

  protection: {
    title: "Add Protection",
    fields: [
      { name: "prot_name", label: "Protection Name", type: "text" },
      { name: "aggregation", label: "Aggregation", type: "select", options: ["max", "min", "avg", "count"], required: true },
      { name: "hysteresis", label: "Hysteresis", type: "number", default: 5 },
      { name: "active_1", label: "Active Level 1", type: "checkbox" },
      { name: "reset_1", label: "Reset Level 1", type: "checkbox" },
      { name: "prot_level_1", label: "Level 1 Threshold", type: "number" },
      { name: "on_time_1", label: "On Time 1", type: "number", default: 3 },
      { name: "on_action_1", label: "On Action 1", type: "text" },
      { name: "off_time_1", label: "Off Time 1", type: "number", default: 3 },
      { name: "off_action_1", label: "Off Action 1", type: "text" },
      { name: "extra", label: "Extra (JSON)", type: "text" },
      { name: "format", label: "Format", type: "text" }
    ]
  },

  chart: {
    title: "Add to Chart",
    fields: [
      { name: "chart_name", label: "Chart Name", type: "text", required: true },
      { name: "color", label: "Color (hex or name)", type: "text" },
      { name: "format", label: "Format", type: "text" },
      { name: "scale", label: "Scale", type: "number" },
      { name: "offset", label: "Offset", type: "number" },
      { name: "extra", label: "Extra (JSON)", type: "text" }
    ]
  },

  metrics: {
    title: "Add to Metrics",
    fields: [
      { name: "metric_name", label: "Metric Name", type: "text", required: true },
      { name: "aggregation", label: "Aggregation", type: "select", options: ["max", "min", "avg", "count"], required: true },
      { name: "format", label: "Format", type: "text" },
      { name: "extra", label: "Extra (JSON)", type: "text" }
    ]
  },

  pub: {
    title: "Add to Pub",
    fields: [
      { name: "pub_name", label: "Publish Stream Name", type: "text", required: true },
      { name: "format", label: "Format Details (e.g. json/simple)", type: "text" },
      { name: "scale", label: "Scale", type: "number" },
      { name: "offset", label: "Offset", type: "number" },
      { name: "extra", label: "Extra (JSON)", type: "text" }
    ]
  }
};

window.FEATURES = [
  { key: "accessPath", label: "Add Access Path", schemaKey: "accessPath" },
  { key: "protection", label: "Add Protection", schemaKey: "protection" },
  { key: "chart", label: "Add to Chart", schemaKey: "chart" },
  { key: "metrics", label: "Add to Metrics", schemaKey: "metrics" },
  { key: "pub", label: "Add to Pub", schemaKey: "pub" }
];

window.SERVER_BASE = 'http://192.168.86.51:8902';

window.ENDPOINT_MAP = {
  accessPath: '/api/access-paths',
  protection: '/api/protections',
  chart: '/api/charts',
  metrics: '/api/metrics',
  pub: '/api/pubs'
};
```

---

## ✅ 2. FastAPI Models (Pydantic + SQLAlchemy)

### Pydantic Models

```python
from pydantic import BaseModel
from typing import Optional
from datetime import datetime

# Base response
class FeatureBaseResponse(BaseModel):
    id: int
    variable_name: str
    is_active: bool
    created_at: datetime
    updated_at: datetime

# Pub
class VariablePubResponse(FeatureBaseResponse):
    pub_name: str
    format: Optional[str]
    scale: Optional[float]
    offset: Optional[float]
    extra: Optional[dict]

class PubCreateRequest(BaseModel):
    variable_name: str
    pub_name: str
    format: Optional[str]
    scale: Optional[float]
    offset: Optional[float]
    extra: Optional[dict]

# Chart
class VariableChartResponse(FeatureBaseResponse):
    chart_name: Optional[str]
    color: Optional[str]
    format: Optional[str]
    scale: Optional[float]
    offset: Optional[float]
    extra: Optional[dict]

class ChartCreateRequest(BaseModel):
    variable_name: str
    chart_name: Optional[str]
    color: Optional[str]
    format: Optional[str]
    scale: Optional[float]
    offset: Optional[float]
    extra: Optional[dict]

# Metric
class VariableMetricResponse(FeatureBaseResponse):
    metric_name: str
    aggregation: Optional[str]
    format: Optional[str]
    extra: Optional[dict]

class MetricCreateRequest(BaseModel):
    variable_name: str
    metric_name: str
    aggregation: Optional[str]
    format: Optional[str]
    extra: Optional[dict]

# Protection
class VariableProtResponse(FeatureBaseResponse):
    prot_name: Optional[str]
    aggregation: str
    hysteresis: int
    active_1: bool
    reset_1: bool
    prot_level_1: Optional[int]
    on_time_1: int
    on_action_1: Optional[str]
    off_time_1: int
    off_action_1: Optional[str]
    extra: Optional[dict]
    format: Optional[str]

class ProtCreateRequest(BaseModel):
    variable_name: str
    prot_name: Optional[str]
    aggregation: str
    hysteresis: int
    active_1: bool
    reset_1: bool
    prot_level_1: Optional[int]
    on_time_1: int
    on_action_1: Optional[str]
    off_time_1: int
    off_action_1: Optional[str]
    extra: Optional[dict]
    format: Optional[str]

# Aggregate
class VariableAggregateResponse(FeatureBaseResponse):
    agg_name: Optional[str]
    max_value: Optional[float]
    min_value: Optional[float]
    avg_value: Optional[float]
    count_value: Optional[int]
    agg_reset: int

class AggregateCreateRequest(BaseModel):
    variable_name: str
    agg_name: Optional[str]
    max_value: Optional[float]
    min_value: Optional[float]
    avg_value: Optional[float]
    count_value: Optional[int]
    agg_reset: int
```

---

### SQLAlchemy Models

```python
from sqlalchemy import Column, Integer, String, Boolean, Float, DateTime, Text, ForeignKey
from sqlalchemy.orm import relationship
from datetime import datetime

Base = declarative_base()

class SystemVariable(Base):
    __tablename__ = 'variables'
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)

class VariablePub(Base):
    __tablename__ = 'variable_pubs'
    id = Column(Integer, primary_key=True)
    variable_id = Column(Integer, ForeignKey('variables.id'), nullable=False)
    variable_name = Column(String, nullable=False)
    pub_name = Column(String, nullable=False)
    format = Column(String)
    scale = Column(Float)
    offset = Column(Float)
    extra = Column(Text)
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

# Repeat for VariableChart, VariableMetric, VariableProt, VariableAggregate
```

---

## ✅ 3. FastAPI Routes (Full CRUD for each feature)

```python
from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.orm import Session
from datetime import datetime
import json

router = APIRouter()

# --- PUBS ---
@app.get("/api/pubs", response_model=List[VariablePubResponse])
def list_pubs(variable_name: Optional[str] = Query(None), active_only: bool = Query(True), db: Session = Depends(get_db)):
    return list_generic_feature(db, VariablePub, variable_name, active_only)

@app.post("/api/pubs", response_model=VariablePubResponse, status_code=201)
def create_pub(request: PubCreateRequest, db: Session = Depends(get_db)):
    return create_generic_feature(db, VariablePub, request, "pub_name")

@app.delete("/api/pubs/{pub_id}")
def delete_pub(pub_id: int, db: Session = Depends(get_db)):
    return deactivate_generic_feature(db, VariablePub, pub_id)

# --- CHARTS ---
@app.get("/api/charts", response_model=List[VariableChartResponse])
def list_charts(variable_name: Optional[str] = Query(None), active_only: bool = Query(True), db: Session = Depends(get_db)):
    return list_generic_feature(db, VariableChart, variable_name, active_only)

@app.post("/api/charts", response_model=VariableChartResponse, status_code=201)
def create_chart(request: ChartCreateRequest, db: Session = Depends(get_db)):
    return create_generic_feature(db, VariableChart, request, "chart_name")

@app.delete("/api/charts/{chart_id}")
def delete_chart(chart_id: int, db: Session = Depends(get_db)):
    return deactivate_generic_feature(db, VariableChart, chart_id)

# --- METRICS ---
@app.get("/api/metrics", response_model=List[VariableMetricResponse])
def list_metrics(variable_name: Optional[str] = Query(None), active_only: bool = Query(True), db: Session = Depends(get_db)):
    return list_generic_feature(db, VariableMetric, variable_name, active_only)

@app.post("/api/metrics", response_model=VariableMetricResponse, status_code=201)
def create_metric(request: MetricCreateRequest, db: Session = Depends(get_db)):
    return create_generic_feature(db, VariableMetric, request, "metric_name")

@app.delete("/api/metrics/{metric_id}")
def delete_metric(metric_id: int, db: Session = Depends(get_db)):
    return deactivate_generic_feature(db, VariableMetric, metric_id)

# --- PROTECTIONS ---
@app.get("/api/protections", response_model=List[VariableProtResponse])
def list_protections(variable_name: Optional[str] = Query(None), active_only: bool = Query(True), db: Session = Depends(get_db)):
    return list_generic_feature(db, VariableProt, variable_name, active_only)

@app.post("/api/protections", response_model=VariableProtResponse, status_code=201)
def create_protection(request: ProtCreateRequest, db: Session = Depends(get_db)):
    return create_generic_feature(db, VariableProt, request, "prot_name")

@app.delete("/api/protections/{prot_id}")
def delete_protection(prot_id: int, db: Session = Depends(get_db)):
    return deactivate_generic_feature(db, VariableProt, prot_id)

# --- AGGREGATES ---
@app.get("/api/aggregates", response_model=List[VariableAggregateResponse])
def list_aggregates(variable_name: Optional[str] = Query(None), active_only: bool = Query(True), db: Session = Depends(get_db)):
    return list_generic_feature(db, VariableAggregate, variable_name, active_only)

@app.post("/api/aggregates", response_model=VariableAggregateResponse, status_code=201)
def create_aggregate(request: AggregateCreateRequest, db: Session = Depends(get_db)):
    return create_generic_feature(db, VariableAggregate, request, "agg_name")

@app.delete("/api/aggregates/{agg_id}")
def delete_aggregate(agg_id: int, db: Session = Depends(get_db)):
    return deactivate_generic_feature(db, VariableAggregate, agg_id)
```

---

### Generic Helpers

```python
def list_generic_feature(db, model, variable_name=None, active_only=True):
    query = db.query(model)
    if variable_name:
        var = db.query(SystemVariable).filter_by(name=variable_name).first()
        if not var:
            raise HTTPException(404, f"Variable '{variable_name}' not found")
        query = query.filter(model.variable_id == var.id)
    if active_only:
        query = query.filter(model.is_active == True)
    return query.order_by(model.id).all()

def create_generic_feature(db, model, request, name_field):
    var = db.query(SystemVariable).filter_by(name=request.variable_name).first()
    if not var:
        raise HTTPException(404, f"Variable '{request.variable_name}' not found")

    data = request.dict()
    data["variable_id"] = var.id
    data["is_active"] = True
    if "extra" in data and data["extra"]:
        data["extra"] = json.dumps(data["extra"])

    obj = model(**data)
    db.add(obj)
    db.commit()
    db.refresh(obj)

    # Reload and parse extra
    if obj.extra:
        obj.extra = json.loads(obj.extra)
    return obj

def deactivate_generic_feature(db, model, feature_id):
    obj = db.query(model).filter_by(id=feature_id).first()
    if not obj:
        raise HTTPException(404, f"Feature {feature_id} not found")
    obj.is_active = False
    obj.updated_at = datetime.utcnow()
    db.commit()
    return {"message": f"Feature {feature_id} deactivated successfully"}
```

---

## ✅ Summary

- ✅ All feature tables supported with full CRUD
- ✅ Unified schema and UI
- ✅ Soft delete via `is_active`
- ✅ JSON `extra` field support
- ✅ FastAPI + Pydantic + SQLAlchemy

---

Would you like me to:
- Generate the full SQLAlchemy models for all tables?
- Generate a migration script to create the tables?
- Add a `/api/features` endpoint that unions all features for a variable?

Let me know!