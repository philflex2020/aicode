# db_server.py
# RBMS Data Server with DB persistence for profiles and variables

from fastapi import FastAPI, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Dict, Any, List, Optional
from sqlalchemy import (
    create_engine, Column, Integer, String, Text, JSON, TIMESTAMP,
    ForeignKey, CheckConstraint, func, UniqueConstraint
)
from sqlalchemy.orm import sessionmaker, declarative_base, relationship, Session
import os
import time
import random
import math
import json
import uvicorn
import argparse

app = FastAPI(title="RBMS DB Data Server")

# CORS (allow frontend on another port)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ------------------------------
# Database setup
# ------------------------------
DB_URL = os.environ.get("RBMS_DB_URL", "sqlite:///rbms.db")
engine = create_engine(
    DB_URL,
    connect_args={"check_same_thread": False} if DB_URL.startswith("sqlite") else {},
    pool_pre_ping=True,
)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()

# ------------------------------
# Models
# ------------------------------
class EnvProfile(Base):
    __tablename__ = "env_profiles"
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)   # e.g. "default", "lab-A"
    build = Column(String, nullable=False)               # hatteras|warwick|factory
    version = Column(String, nullable=False)             # e.g. "0.3.34"
    target = Column(String, nullable=True)               # install name (optional)
    __table_args__ = (
        CheckConstraint("build in ('hatteras','warwick','factory')", name="ck_build"),
    )


class ProfileHistory(Base):
    __tablename__ = "profile_history"
    id = Column(Integer, primary_key=True)
    profile_id = Column(Integer, ForeignKey("env_profiles.id"), nullable=False)
    action = Column(String, nullable=False)  # create|update|clone|select
    data = Column(JSON, nullable=True)
    created_at = Column(TIMESTAMP, server_default=func.current_timestamp())
    profile = relationship("EnvProfile")


class MetaKV(Base):
    __tablename__ = "meta_kv"
    id = Column(Integer, primary_key=True)
    k = Column(String, unique=True, nullable=False)
    v = Column(String, nullable=True)


class Variable(Base):
    __tablename__ = "variables"
    id = Column(Integer, primary_key=True)
    profile_id = Column(Integer, ForeignKey("env_profiles.id"), nullable=False)
    mem_area = Column(String, nullable=False)   # rtos|rack|sbmu (extensible)
    mem_type = Column(String, nullable=False)   # sm16|sm8|hold|input|bits|coils|...
    offset = Column(String, nullable=False)     # int/hex/name stored as text
    value = Column(Text, nullable=True)
    updated_at = Column(TIMESTAMP, server_default=func.current_timestamp(), onupdate=func.current_timestamp())
    __table_args__ = (
        UniqueConstraint("profile_id", "mem_area", "mem_type", "offset", name="uniq_var_key"),
    )

# ------------------------------
# Schemas (Pydantic v2: use 'pattern' instead of 'regex')
# ------------------------------
class ProfileIn(BaseModel):
    name: str = Field(..., description="Profile label, e.g. 'default'")
    build: str = Field(..., pattern="^(hatteras|warwick|factory)$")
    version: str = Field(..., description="e.g. 0.3.34")
    target: Optional[str] = None

class ProfileOut(BaseModel):
    id: int
    name: str
    build: str
    version: str
    target: Optional[str]

    model_config = {"from_attributes": True}

class CloneIn(BaseModel):
    source_name: str
    new_name: str
    new_version: Optional[str] = None
    new_target: Optional[str] = None

class SelectIn(BaseModel):
    name: str

class VarUpdateIn(BaseModel):
    data: Dict[str, Any]

# tlelmetry     
class TelemetryMeta(Base):
    __tablename__ = "telemetry_meta"
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)
    label = Column(String, nullable=False)
    unit = Column(String, nullable=True)
    category = Column(String, nullable=True)


# ------------------------------
# State init from dash.json (compatibility)
# ------------------------------
def init_state_from_dash():
    try:
        with open("dash.json", "r", encoding="utf-8") as f:
            dash = json.load(f)
        state = {}
        if "controls" in dash and "cards" in dash["controls"]:
            for card in dash["controls"]["cards"]:
                for el in card.get("elements", []):
                    if "id" in el and "default" in el:
                        state[el["id"]] = el["default"]
        state["buttonPressed"] = ""
        return state
    except Exception as e:
        print(f"Warning: could not load dash.json for state init: {e}")
        return {"buttonPressed": ""}

STATE: Dict[str, Any] = init_state_from_dash()

# ------------------------------
# DB helpers
# ------------------------------
def init_db():
    Base.metadata.create_all(engine)
    with SessionLocal() as db:
        prof = db.query(EnvProfile).filter_by(name="default").first()
        if not prof:
            prof = EnvProfile(name="default", build="hatteras", version="0.3.34", target=None)
            db.add(prof)
            db.flush()
            db.add(ProfileHistory(profile=prof, action="create", data={"build": prof.build, "version": prof.version, "target": prof.target}))
            db.commit()
        if db.query(MetaKV).filter_by(k="active_profile").first() is None:
            db.add(MetaKV(k="active_profile", v="default"))
            db.commit()
        # Seed telemetry_meta if empty
        if db.query(TelemetryMeta).count() == 0:
            seed = [
                TelemetryMeta(name="mbps",      label="Throughput (Mb/s)",  unit="Mbps", category="network"),
                TelemetryMeta(name="rtt_ms",    label="Round-trip (ms)",    unit="ms",   category="network"),
                TelemetryMeta(name="temp_c",    label="Temperature (°C)",   unit="°C",   category="thermal"),
                TelemetryMeta(name="errors",    label="Error Count",        unit="count",category="network"),
                TelemetryMeta(name="drops",     label="Packet Drops",       unit="count",category="network"),
                TelemetryMeta(name="link_state",label="Link Status",        unit="",     category="network"),
                # New: synthetic cell-level metrics
                TelemetryMeta(name="cell_volts", label="Cell Voltage", unit="V",   category="power"),
                TelemetryMeta(name="cell_temp",  label="Cell Temp",    unit="°C",  category="thermal"),
                TelemetryMeta(name="cell_soc",   label="Cell SOC",     unit="%",   category="state"),
                TelemetryMeta(name="cell_soh",   label="Cell SOH",     unit="%",   category="state"),
            ]
            db.add_all(seed)
    # ✅ Sync dash.json variables
    sync_dash_variables()

# ------------------------------
# Variable init from dash.json (compatibility)
# ------------------------------
def sync_dash_variables():
    """
    Ensure all variables referenced in dash.json exist in the database
    for the active profile with their default values.
    """
    try:
        with open("dash.json", "r", encoding="utf-8") as f:
            dash = json.load(f)
    except Exception as e:
        print(f"Warning: could not load dash.json for variable sync: {e}")
        return

    with SessionLocal() as db:
        prof = get_active_profile(db)
        default_area = "rtos"
        default_type = "hold"
        
        # Collect all variable definitions from dash.json
        var_defs = {}  # {name: default_value}
        
        # 1. From controls cards
        if "controls" in dash and "cards" in dash["controls"]:
            for card in dash["controls"]["cards"]:
                for el in card.get("elements", []):
                    if "id" in el and "default" in el:
                        var_defs[el["id"]] = el["default"]
        
        # 2. From values fields (no defaults, just ensure they exist)
        if "values" in dash and "fields" in dash["values"]:
            for field in dash["values"]["fields"]:
                if field not in var_defs:
                    var_defs[field] = None  # No default specified
        
        # 3. From widgets (vartable names)
        for widget in dash.get("widgets", []):
            if widget.get("type") == "vartable" and "names" in widget:
                for name in widget["names"]:
                    if name not in var_defs:
                        var_defs[name] = None
        
        # Now sync to database
        synced = []
        for var_name, default_val in var_defs.items():
            row = db.query(Variable).filter_by(
                profile_id=prof.id,
                mem_area=default_area,
                mem_type=default_type,
                offset=var_name
            ).first()
            
            if row is None:
                # Create new variable with default value
                row = Variable(
                    profile_id=prof.id,
                    mem_area=default_area,
                    mem_type=default_type,
                    offset=var_name,
                    value=str(default_val) if default_val is not None else None
                )
                db.add(row)
                synced.append(f"{var_name}={default_val}")
        
        if synced:
            db.commit()
            print(f"✅ Synced {len(synced)} variables from dash.json: {', '.join(synced)}")
        else:
            print("✅ All dash.json variables already exist in database")


def set_active_profile(db: Session, name: str):
    kv = db.query(MetaKV).filter_by(k="active_profile").first()
    if not kv:
        kv = MetaKV(k="active_profile", v=name)
        db.add(kv)
    else:
        kv.v = name
    db.commit()

def get_active_profile_name(db: Session) -> str:
    kv = db.query(MetaKV).filter_by(k="active_profile").first()
    if kv and kv.v:
        return kv.v
    return "default"

def get_active_profile(db: Session) -> EnvProfile:
    name = get_active_profile_name(db)
    prof = db.query(EnvProfile).filter_by(name=name).first()
    if not prof:
        raise HTTPException(404, "Active profile not found")
    return prof

init_db()

# ------------------------------
# Profile endpoints
# ------------------------------
@app.get("/api/profiles", response_model=List[ProfileOut])
def list_profiles():
    with SessionLocal() as db:
        rows = db.query(EnvProfile).all()
        return [ProfileOut.model_validate(r, from_attributes=True) for r in rows]

@app.get("/api/active_profile", response_model=ProfileOut)
def active_profile():
    with SessionLocal() as db:
        prof = get_active_profile(db)
        return ProfileOut.model_validate(prof, from_attributes=True)

@app.post("/api/profiles", response_model=ProfileOut)
def upsert_profile(p: ProfileIn):
    with SessionLocal() as db:
        prof = db.query(EnvProfile).filter_by(name=p.name).first()
        if prof:
            prof.build = p.build
            prof.version = p.version
            prof.target = p.target
            db.add(ProfileHistory(profile=prof, action="update", data=p.model_dump()))
        else:
            prof = EnvProfile(name=p.name, build=p.build, version=p.version, target=p.target)
            db.add(prof)
            db.flush()
            db.add(ProfileHistory(profile=prof, action="create", data=p.model_dump()))
        db.commit()
        return ProfileOut.model_validate(prof, from_attributes=True)

@app.post("/api/profiles/select")
def select_profile(sel: SelectIn):
    with SessionLocal() as db:
        prof = db.query(EnvProfile).filter_by(name=sel.name).first()
        if not prof:
            raise HTTPException(404, "Profile not found")
        set_active_profile(db, sel.name)
        db.add(ProfileHistory(profile=prof, action="select", data={"name": sel.name}))
        db.commit()
        return {"ok": True, "active": sel.name}

@app.post("/api/profiles/clone", response_model=ProfileOut)
def clone_profile(c: CloneIn):
    with SessionLocal() as db:
        src = db.query(EnvProfile).filter_by(name=c.source_name).first()
        if not src:
            raise HTTPException(404, "Source profile not found")
        if db.query(EnvProfile).filter_by(name=c.new_name).first():
            raise HTTPException(409, "New profile name already exists")

        new_prof = EnvProfile(
            name=c.new_name,
            build=src.build,
            version=c.new_version if c.new_version is not None else src.version,
            target=c.new_target if c.new_target is not None else src.target
        )
        db.add(new_prof)
        db.flush()

        vars_src = db.query(Variable).filter_by(profile_id=src.id).all()
        for v in vars_src:
            db.add(Variable(
                profile_id=new_prof.id,
                mem_area=v.mem_area,
                mem_type=v.mem_type,
                offset=v.offset,
                value=v.value
            ))

        db.add(ProfileHistory(profile=new_prof, action="clone",
                              data={"from": src.name, "version": new_prof.version, "target": new_prof.target}))
        db.commit()
        return ProfileOut.model_validate(new_prof, from_attributes=True)


@app.post("/api/sync_dash")
def sync_dash():
    """
    Manually trigger sync of dash.json variables to database.
    Useful after editing dash.json.
    """
    sync_dash_variables()
    return {"ok": True, "message": "dash.json variables synced to database"}


# ------------------------------
# State and button (compatibility)
# ------------------------------
@app.get("/api/state")
def get_state():
    return {"ok": True, "state": STATE}

@app.post("/api/state")
async def set_state(request: Request):
    body = await request.json()
    data = body.get("data", {})
    for k, v in data.items():
        STATE[k] = v
    return {"ok": True, "state": STATE}

@app.post("/api/button/{name}")
def button_action(name: str):
    STATE["buttonPressed"] = name
    return {"ok": True, "pressed": name, "state": STATE}
#################################################
@app.post("/vars")
async def update_vars(request: Request, profile: Optional[str] = None):
    """
    Update one or more configuration variables.
    If ?profile=<name> is given, apply changes to that profile.
    Otherwise use the current active profile.
    """
    payload = await request.json()
    if not isinstance(payload, dict):
        raise HTTPException(400, "Expected JSON object {name: value, ...}")

    with SessionLocal() as db:
        if profile:
            prof = db.query(EnvProfile).filter_by(name=profile).first()
            if not prof:
                raise HTTPException(404, f"Profile '{profile}' not found")
        else:
            prof = get_active_profile(db)

        default_area, default_type = "rtos", "hold"
        updated = {}

        for k, v in payload.items():
            key = str(k)
            row = db.query(Variable).filter_by(
                profile_id=prof.id,
                mem_area=default_area,
                mem_type=default_type,
                offset=key
            ).first()
            if row is None:
                row = Variable(profile_id=prof.id,
                               mem_area=default_area,
                               mem_type=default_type,
                               offset=key,
                               value=str(v))
                db.add(row)
            else:
                row.value = str(v)
            updated[key] = str(v)

        db.commit()
        STATE.update(payload)
        return {"ok": True, "updated": updated, "profile": prof.name}
################################################





# ------------------------------
# Vars with DB persistence tied to active profile
# ------------------------------
@app.post("/xvars")
async def update_vars(request: Request):
    payload = await request.json()  # e.g., {"gain":"42","phase":"180"}
    if not isinstance(payload, dict):
        raise HTTPException(400, "Expected JSON object {name: value, ...}")

    with SessionLocal() as db:
        prof = get_active_profile(db)
        default_area = "rtos"
        default_type = "hold"
        updated = {}
        for k, v in payload.items():
            key = str(k)
            row = db.query(Variable).filter_by(
                profile_id=prof.id, mem_area=default_area, mem_type=default_type, offset=key
            ).first()
            if not row:
                row = Variable(profile_id=prof.id, mem_area=default_area, mem_type=default_type, offset=key, value=str(v))
                db.add(row)
            else:
                row.value = str(v)
            updated[key] = str(v)
        db.commit()
        STATE.update(payload)
        return {"ok": True, "updated": updated, "profile": prof.name}
##############################################################################
@app.get("/vars")
def get_vars(names: str, rack: int = 0, profile: Optional[str] = None):
    """
    Read current config or telemetry-related variable values.
    Only manipulates the profile-specific DB when applicable.
    """
    name_list = [n.strip() for n in names.split(",") if n.strip()]
    out: Dict[str, Any] = {}

    with SessionLocal() as db:
        if profile:
            prof = db.query(EnvProfile).filter_by(name=profile).first()
            if not prof:
                raise HTTPException(404, f"Profile '{profile}' not found")
        else:
            prof = get_active_profile(db)

        default_area, default_type = "rtos", "hold"

        for nm in name_list:
            # --- profile‑specific config vars ---
            row = db.query(Variable).filter_by(
                profile_id=prof.id,
                mem_area=default_area,
                mem_type=default_type,
                offset=nm
            ).first()
            if row is not None:
                out[nm] = row.value
                STATE[nm] = row.value
                continue

            # --- non‑profile system telemetry vars ---
            if nm in ("link_state", "errors", "drops", "temp_c", "mbps", "rtt_ms"):
                val = (
                    "UP" if nm == "link_state" and random.random() > 0.2 else
                    str(random.randint(0, 5)) if nm == "errors" else
                    str(random.randint(0, 3)) if nm == "drops" else
                    str(round(45 + 2 * random.random(), 2))
                )
                STATE[nm] = val
                out[nm] = val
                continue

            # --- default fallback / first creation ---
            val = STATE.get(nm)
            out[nm] = val
            db.add(Variable(profile_id=prof.id,
                            mem_area=default_area,
                            mem_type=default_type,
                            offset=nm,
                            value=str(val) if val is not None else None))
            db.commit()

    return out
#################################################################################
# @app.get("/xvars")
# def get_vars(names: str, rack: int = 0):
#     name_list = [n.strip() for n in names.split(",") if n.strip()]
#     out = {}
#     with SessionLocal() as db:
#         prof = get_active_profile(db)
#         default_area = "rtos"
#         default_type = "hold"
#         for nm in name_list:
#             row = db.query(Variable).filter_by(
#                 profile_id=prof.id, mem_area=default_area, mem_type=default_type, offset=nm
#             ).first()
#             if row is not None:
#                 out[nm] = row.value
#                 STATE[nm] = row.value
#                 continue
#             if nm in STATE:
#                 out[nm] = STATE[nm]
#                 db.add(Variable(profile_id=prof.id, mem_area=default_area, mem_type=default_type, offset=nm, value=str(STATE[nm])))
#                 db.commit()
#             else:
#                 if nm == "link_state":
#                     val = "UP" if random.random() > 0.2 else "DOWN"
#                 elif nm == "errors":
#                     val = str(random.randint(0, 5))
#                 elif nm == "drops":
#                     val = str(random.randint(0, 3))
#                 else:
#                     val = None
#                 STATE[nm] = val
#                 out[nm] = val
#                 db.add(Variable(profile_id=prof.id, mem_area=default_area, mem_type=default_type, offset=nm, value=str(val) if val is not None else None))
#                 db.commit()
#     return out

# ------------------------------
# Metrics and series (unchanged mocks)
# ------------------------------

@app.get("/metrics")
def metrics(profile: Optional[str] = None):
    # profile ignored intentionally
    rows = []
    for i in range(6):
        rows.append({
            "name": f"rack_{i}",
            "host": f"192.168.100.{i+10}",
            "port": 6500 + i,
            "tx_msgs": random.randint(1000, 50000),
            "rx_msgs": random.randint(1000, 50000),
            "tx_bytes": random.randint(10**6, 10**8),
            "rx_bytes": random.randint(10**6, 10**8),
            "ema_mbps": random.random()*100,
            "ewma_rtt_s": random.random()*0.2,
        })
    return rows
#####################################################################################
@app.get("/series")
def get_series(
    names: Optional[str] = None,
    category: Optional[str] = None,
    window: int = 300,
    profile: Optional[str] = None,
):
    """
    Telemetry time-series endpoint.
    - If no params: return all metadata from telemetry_meta table.
    - If ?category=network: return metadata for that category only.
    - If ?names=mbps,cpu: generate mock data for requested telemetry names.
    - If ?category=network&names=...: filter by category, then generate data.
    The 'profile' parameter is accepted but ignored (telemetry is global).
    """
    with SessionLocal() as db:
        # --- Discovery mode ---
        if not names:
            query = db.query(TelemetryMeta)
            
            # Filter by category if provided
            if category:
                categories = [c.strip() for c in category.split(",") if c.strip()]
                query = query.filter(TelemetryMeta.category.in_(categories))
            
            rows = query.order_by(TelemetryMeta.name).all()
            return {
                "available": [
                    {
                        "name": r.name,
                        "label": r.label,
                        "unit": r.unit,
                        "category": r.category,
                    }
                    for r in rows
                ]
            }

        # --- Data generation mode ---
        now = int(time.time())
        name_list = [n.strip() for n in names.split(",") if n.strip()]
        print("name_list:",name_list)

        # Preload valid telemetry names
        query = db.query(TelemetryMeta.name)
        
        # Filter by category if provided
        if category:
            categories = [c.strip() for c in category.split(",") if c.strip()]
            query = query.filter(TelemetryMeta.category.in_(categories))
        
        known = {r.name for r in query.all()}
        series: Dict[str, List[List[float]]] = {}

        for nm in name_list:
            if nm not in known:

                print("unknown name:",nm)
                #continue  # skip unknown telemetry name

            pts: List[List[float]] = []

            # --- Special: synthetic array-type telemetry for cells ---
            if nm in ("cell_volts", "cell_temp", "cell_soc", "cell_soh"):
                TOTAL = 480   # number of cells; match dash.json opts.totalItems

                if nm == "cell_volts":
                    arr = [
                        round(3.20 + 0.05 * math.sin((i / 10.0) + (now % 60) / 10.0), 3)
                        for i in range(TOTAL)
                    ]
                elif nm == "cell_temp":
                    arr = [
                        round(25.0 + 5.0 * math.sin((i / 30.0) + (now % 120) / 20.0), 1)
                        for i in range(TOTAL)
                    ]
                elif nm == "cell_soc":
                    arr = [
                        round(90.0 + 5.0 * math.sin((i / 50.0) + (now % 300) / 40.0), 1)
                        for i in range(TOTAL)
                    ]
                elif nm == "cell_soh":
                    arr = [
                        round(98.0 - 0.01 * i, 2)
                        for i in range(TOTAL)
                    ]
                else:
                    arr = [0.0] * TOTAL

                # single time sample: [timestamp, array-of-values]
                pts.append([now, arr])
                series[nm] = pts
                continue

            # --- Normal scalar time-series as before ---
            for i in range(window // 2):
                t = now - (window - i * 2)
                if nm == "mbps":
                    val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
                elif nm == "rtt_ms":
                    val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
                elif nm == "temp_c":
                    val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
                elif nm == "cpu":
                    val = 60 + 20 * math.sin(i / 8.0) + random.random() * 10
                elif nm in ("errors", "drops"):
                    val = random.randint(0, 5)
                elif nm == "link_state":
                    val = 1 if random.random() > 0.2 else 0
                else:
                    val = random.random() * 100
                pts.append([t, float(val)])
            series[nm] = pts

        return {"series": series}
#####################################################################################
@app.get("/xxseries")
def get_xxseries(
    names: Optional[str] = None,
    window: int = 300,
    profile: Optional[str] = None,
):
    """
    Telemetry time-series endpoint.
    - If 'names' is omitted: return metadata list from telemetry_meta table.
    - Otherwise: generate mock data for requested telemetry names.
    The 'profile' parameter is accepted but ignored (telemetry is global).
    """
    with SessionLocal() as db:
        # --- Discovery mode ---
        if not names:
            rows = db.query(TelemetryMeta).order_by(TelemetryMeta.name).all()
            return {
                "available": [
                    {
                        "name": r.name,
                        "label": r.label,
                        "unit": r.unit,
                        "category": r.category,
                    }
                    for r in rows
                ]
            }

        # --- Data generation mode ---
        now = int(time.time())
        name_list = [n.strip() for n in names.split(",") if n.strip()]

        # Preload valid telemetry names
        known = {r.name for r in db.query(TelemetryMeta.name).all()}
        series: Dict[str, List[List[float]]] = {}

        for nm in name_list:
            if nm not in known:
                continue  # skip unknown telemetry name

            pts: List[List[float]] = []
            for i in range(window // 2):
                t = now - (window - i * 2)
                if nm == "mbps":
                    val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
                elif nm == "rtt_ms":
                    val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
                elif nm == "temp_c":
                    val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
                elif nm in ("errors", "drops"):
                    val = random.randint(0, 5)
                elif nm == "link_state":
                    val = 1 if random.random() > 0.2 else 0
                else:
                    val = random.random() * 100
                pts.append([t, float(val)])
            series[nm] = pts

        return {"series": series}

@app.get("/series/category/{category}")
def get_series_by_category(category: str):
    """
    Get all telemetry series names for a specific category.
    Example: /series/category/network
    """
    with SessionLocal() as db:
        rows = db.query(TelemetryMeta).filter_by(category=category).order_by(TelemetryMeta.name).all()
        return {
            "category": category,
            "series": [
                {
                    "name": r.name,
                    "label": r.label,
                    "unit": r.unit,
                }
                for r in rows
            ]
        }



###########################################################
from pydantic import BaseModel

# Schema for creating telemetry series entries
class TelemetryMetaIn(BaseModel):
    name: str
    label: str
    unit: Optional[str] = None
    category: Optional[str] = None


@app.post("/series")
def create_series(meta: TelemetryMetaIn):
    """
    Create or update a telemetry series definition in the database.
    Example payload:
    {
        "name": "voltage_v",
        "label": "Input Voltage (V)",
        "unit": "V",
        "category": "power"
    }
    """
    with SessionLocal() as db:
        # Check for existing entry by name
        row = db.query(TelemetryMeta).filter_by(name=meta.name).first()
        if row:
            # Update existing record
            row.label = meta.label
            row.unit = meta.unit
            row.category = meta.category
            action = "updated"
        else:
            # Create new record
            row = TelemetryMeta(
                name=meta.name,
                label=meta.label,
                unit=meta.unit,
                category=meta.category,
            )
            db.add(row)
            action = "created"

        db.commit()
        return {
            "ok": True,
            "action": action,
            "name": row.name,
            "label": row.label,
            "unit": row.unit,
            "category": row.category,
        }
########################################################
# @app.get("/series")
# def series(
#     names: Optional[str] = None,
#     window: int = 300,
#     profile: Optional[str] = None,
# ):
#     """
#     Telemetry time-series endpoint backed by DB metadata.
#     - If 'names' is omitted, return list of telemetry types from telemetry_meta table.
#     - Otherwise, generate mock data for the requested names.
#     The 'profile' parameter is accepted but ignored (telemetry is global).
#     """
#     with SessionLocal() as db:
#         # --- Discovery mode: no names provided ---
#         if not names:
#             rows = db.query(TelemetryMeta).order_by(TelemetryMeta.name).all()
#             catalog = [
#                 {
#                     "name": r.name,
#                     "label": r.label,
#                     "unit": r.unit,
#                     "category": r.category,
#                 }
#                 for r in rows
#             ]
#             return {"available": catalog}

#         # --- Data generation mode ---
#         now = int(time.time())
#         name_list = [n.strip() for n in names.split(",") if n.strip()]
#         series: Dict[str, List[List[float]]] = {}

#         # Preload available telemetry names to guard against typos
#         known = {r.name for r in db.query(TelemetryMeta.name).all()}

#         for nm in name_list:
#             pts: List[List[float]] = []
#             if nm not in known:
#                 # Gracefully skip unknown telemetry name
#                 continue

#             for i in range(window // 2):
#                 t = now - (window - i * 2)
#                 # Existing synthetic patterns
#                 if nm == "mbps":
#                     val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
#                 elif nm == "rtt_ms":
#                     val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
#                 elif nm == "temp_c":
#                     val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
#                 elif nm in ("errors", "drops"):
#                     val = random.randint(0, 5)
#                 elif nm == "link_state":
#                     val = 1 if random.random() > 0.2 else 0
#                 else:
#                     val = random.random() * 100
#                 pts.append([t, float(val)])
#             series[nm] = pts

#         return {"series": series}
# ##########################################################
# @app.get("/xseries")
# def series(
#     names: Optional[str] = None,
#     window: int = 300,
#     profile: Optional[str] = None,
# ):
#     """
#     Telemetry time-series endpoint.
#     - If 'names' is omitted, return metadata list of available telemetry types.
#     - Otherwise, generate mock sample data for the requested names.
#     The 'profile' parameter is accepted but ignored.
#     """
#     # Discovery mode
#     if not names:
#         telemetry_catalog = [
#             {"name": "mbps", "label": "Throughput (Mb/s)", "unit": "Mbps"},
#             {"name": "rtt_ms", "label": "Round-trip Time (ms)", "unit": "ms"},
#             {"name": "temp_c", "label": "Temperature (°C)", "unit": "°C"},
#             {"name": "errors", "label": "Error Count", "unit": "count"},
#             {"name": "drops", "label": "Packet Drops", "unit": "count"},
#             {"name": "link_state", "label": "Link Up/Down", "unit": ""},
#         ]
#         return {"available": telemetry_catalog}

#     # Data mode
#     now = int(time.time())
#     name_list = [n.strip() for n in names.split(",") if n.strip()]
#     series: Dict[str, List[List[float]]] = {}

#     for nm in name_list:
#         pts = []
#         for i in range(window // 2):
#             t = now - (window - i * 2)
#             if nm == "mbps":
#                 val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
#             elif nm == "rtt_ms":
#                 val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
#             elif nm == "temp_c":
#                 val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
#             elif nm in ("errors", "drops"):
#                 val = random.randint(0, 5)
#             elif nm == "link_state":
#                 val = 1 if random.random() > 0.2 else 0
#             else:
#                 val = random.random() * 100
#             pts.append([t, float(val)])
#         series[nm] = pts

#     return {"series": series}
# #####################################################################################

# @app.get("/xseries")
# def series(names: str, window: int = 300, profile: Optional[str] = None):
#     now = int(time.time())
#     name_list = [n.strip() for n in names.split(",") if n.strip()]
#     series: Dict[str, List[List[float]]] = {}
#     for nm in name_list:
#         pts = []
#         for i in range(window // 2):
#             t = now - (window - i*2)
#             if nm == "mbps":
#                 val = 50 + 30*math.sin(i/6.0) + random.random()*5
#             elif nm == "rtt_ms":
#                 val = 20 + 5*math.sin(i/10.0) + random.random()*3
#             elif nm == "temp_c":
#                 val = 45 + 2*math.sin(i/20.0) + random.random()*0.5
#             else:
#                 val = random.random()*100
#             pts.append([t, float(val)])
#         series[nm] = pts
#     return {"series": series}

# ------------------------------
# Entry point with CLI for host/port
# ------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RBMS DB Data Server")
    parser.add_argument("--host", default=os.environ.get("HOST", "0.0.0.0"), help="Host to bind (default 0.0.0.0)")
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "8084")), help="Port to bind (default 8084)")
    parser.add_argument("--reload", action="store_true", help="Enable uvicorn reload")
    args = parser.parse_args()

    uvicorn.run("db_server:app", host=args.host, port=args.port, reload=args.reload)




    
#     You don’t need to change the `/series` endpoint shape at all for the IndexGridWidget I gave you; you just need to *add* some synthetic “array” series for things like `cell_volts`, `cell_temp`, etc., and then flatten them on the frontend.

# Right now `/series` returns time-series like:

# ```json
# {
#   "series": {
#     "mbps": [[t1, v1], [t2, v2], ...],
#     "temp_c": [[t1, v1], ...]
#   }
# }
# ```

# We’ll treat **cell-style metrics** as:

# ```json
# "cell_volts": [[t_last, [v0, v1, v2, ...]]]
# ```

# Then the `IndexGridWidget.tick()` will detect the array in the last sample and turn it into `[v0, v1, ...]`.

# Below are the minimal changes you need.

# ---

# ### 1. Extend `/series` to generate cell/module arrays

# In `get_series` (your active `/series` handler), inside the `for nm in name_list:` loop, add special handling for `cell_volts`, `cell_temp`, `cell_soc`, `cell_soh`, etc.

# Find this part (near the bottom of `get_series`):

# ```python
#         for nm in name_list:
#             if nm not in known:
#                 continue  # skip unknown telemetry name

#             pts: List[List[float]] = []
#             for i in range(window // 2):
#                 t = now - (window - i * 2)
#                 if nm == "mbps":
#                     val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
#                 elif nm == "rtt_ms":
#                     val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
#                 elif nm == "temp_c":
#                     val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
#                 elif nm == "cpu":
#                     val = 60 + 20 * math.sin(i / 8.0) + random.random() * 10
#                 elif nm in ("errors", "drops"):
#                     val = random.randint(0, 5)
#                 elif nm == "link_state":
#                     val = 1 if random.random() > 0.2 else 0
#                 else:
#                     val = random.random() * 100
#                 pts.append([t, float(val)])
#             series[nm] = pts
# ```

# Replace it with a version that also handles the cell/module arrays:

# ```python
#         for nm in name_list:
#             if nm not in known:
#                 continue  # skip unknown telemetry name

#             pts: List[List[float]] = []

#             # --- Special: synthetic array-type telemetry (cells/modules) ---
#             if nm in ("cell_volts", "cell_temp", "cell_soc", "cell_soh"):
#                 # Total items – match your widget config (e.g. 480 cells)
#                 TOTAL = 480

#                 # Build a fake array of values at "now"
#                 if nm == "cell_volts":
#                     arr = [
#                         round(3.20 + 0.05 * math.sin((i / 10.0) + (now % 60) / 10.0), 3)
#                         for i in range(TOTAL)
#                     ]
#                 elif nm == "cell_temp":
#                     arr = [
#                         round(25.0 + 5.0 * math.sin((i / 30.0) + (now % 120) / 20.0), 1)
#                         for i in range(TOTAL)
#                     ]
#                 elif nm == "cell_soc":
#                     arr = [
#                         round(90.0 + 5.0 * math.sin((i / 50.0) + (now % 300) / 40.0), 1)
#                         for i in range(TOTAL)
#                     ]
#                 elif nm == "cell_soh":
#                     arr = [
#                         round(98.0 - 0.01 * i, 2)   # gently decreasing with index
#                         for i in range(TOTAL)
#                     ]
#                 else:
#                     arr = [0.0] * TOTAL

#                 # Store as a single time-sample: [timestamp, array-of-values]
#                 pts.append([now, arr])
#                 series[nm] = pts
#                 continue

#             # --- Normal scalar time-series as before ---
#             for i in range(window // 2):
#                 t = now - (window - i * 2)
#                 if nm == "mbps":
#                     val = 50 + 30 * math.sin(i / 6.0) + random.random() * 5
#                 elif nm == "rtt_ms":
#                     val = 20 + 5 * math.sin(i / 10.0) + random.random() * 3
#                 elif nm == "temp_c":
#                     val = 45 + 2 * math.sin(i / 20.0) + random.random() * 0.5
#                 elif nm == "cpu":
#                     val = 60 + 20 * math.sin(i / 8.0) + random.random() * 10
#                 elif nm in ("errors", "drops"):
#                     val = random.randint(0, 5)
#                 elif nm == "link_state":
#                     val = 1 if random.random() > 0.2 else 0
#                 else:
#                     val = random.random() * 100
#                 pts.append([t, float(val)])

#             series[nm] = pts
# ```

# Everything else in `get_series` stays the same.

# You *also* need to register these names in `TelemetryMeta` or `known` won’t include them.

# Add them to the seed in `init_db()`:

# ```python
#         # Seed telemetry_meta if empty
#         if db.query(TelemetryMeta).count() == 0:
#             seed = [
#                 TelemetryMeta(name="mbps",      label="Throughput (Mb/s)",  unit="Mbps", category="network"),
#                 TelemetryMeta(name="rtt_ms",    label="Round-trip (ms)",    unit="ms",   category="network"),
#                 TelemetryMeta(name="temp_c",    label="Temperature (°C)",   unit="°C",   category="thermal"),
#                 TelemetryMeta(name="errors",    label="Error Count",        unit="count",category="network"),
#                 TelemetryMeta(name="drops",     label="Packet Drops",       unit="count",category="network"),
#                 TelemetryMeta(name="link_state",label="Link Status",        unit="",     category="network"),

#                 # NEW: cell/module-like arrays
#                 TelemetryMeta(name="cell_volts", label="Cell Voltage", unit="V",   category="power"),
#                 TelemetryMeta(name="cell_temp",  label="Cell Temp",    unit="°C",  category="thermal"),
#                 TelemetryMeta(name="cell_soc",   label="Cell SOC",     unit="%",   category="state"),
#                 TelemetryMeta(name="cell_soh",   label="Cell SOH",     unit="%",   category="state"),
#             ]
#             db.add_all(seed)
# ```

# Or add them later via the POST `/series` meta API; either is fine, but they must exist in `telemetry_meta` so the “known” set sees them.

# ---

# ### 2. Make sure the `IndexGridWidget` flattens correctly

# The `tick()` I gave you already supports the `[t, array]` shape; double-check you have this code in `IndexGridWidget.tick`:

# ```js
# // seriesCache: e.g., { cell_volts: [[t, [..vals..]]], ... }
# async tick(seriesCache) {
#   if (!seriesCache) return;

#   const data = {};
#   for (const m of this.metrics) {
#     const pts = seriesCache[m.name] || [];
#     if (!pts.length) {
#       data[m.name] = new Array(this.totalItems).fill(null);
#       continue;
#     }
#     const last = pts[pts.length - 1][1];
#     let arr;
#     if (Array.isArray(last)) {
#       // Our expected case: [t, [v0..]]
#       arr = last.slice();
#     } else {
#       // Fallback: if you ever send scalar series instead
#       arr = pts.map(p => p[1]);
#     }

#     const vals = new Array(this.totalItems);
#     for (let i = 0; i < this.totalItems; i++) {
#       vals[i] = i < arr.length ? arr[i] : null;
#     }
#     data[m.name] = vals;
#   }

#   this.lastData = data;
#   this.renderBlocks(data);
# }
# ```

# That matches:

# - Backend: `cell_volts: [[now, [3.21, 3.22, ...]]]`
# - Frontend: `IndexGridWidget` sees the `last[1]` as an array and turns it into the `[v0..vN-1]` shape the grid renderer expects.

# ---

# ### 3. `dash.json` widget entry

# Your new widget in `dash.json` can look like:

# ```json
# {
#   "id": "cells1",
#   "type": "indexgrid",
#   "title": "Cell Data",
#   "tab": "values",
#   "pos": {"row": 1, "col": 1, "w": 12, "h": 5},
#   "opts": {
#     "totalItems": 480,
#     "itemsPerBlock": 100,
#     "indexLabel": "Cell #"
#   },
#   "series": [
#     {"name": "cell_volts", "label": "Volts", "unit": "V"},
#     {"name": "cell_temp",  "label": "Temp",  "unit": "°C"},
#     {"name": "cell_soc",   "label": "SOC",   "unit": "%"},
#     {"name": "cell_soh",   "label": "SOH",   "unit": "%"}
#   ]
# }
# ```

# And keep the registry entry:

# ```js
# indexgrid: (node,cfg)=>new IndexGridWidget(node,cfg)
# ```

# ---

# ### 4. Quick sanity checks

# 1. Hit `/series?names=cell_volts,cell_temp&window=10` directly:

#    You should see:

#    ```json
#    {
#      "series": {
#        "cell_volts": [[ 1731875000, [3.21, 3.22, ...] ]],
#        "cell_temp":  [[ 1731875000, [25.1, 25.3, ...] ]]
#      }
#    }
#    ```

# 2. Confirm that `names` for the indexgrid widget are included in the poller’s `names` query (your existing `collectSeriesNamesForActiveTab` should already do it).

# 3. Watch console logs; if the widget’s `tick` is called but no values appear, log `seriesCache` and ensure keys match exactly (`cell_volts` vs `cell_volts `).

# ---

# If you paste how your polling function constructs `seriesCache` (the object passed into `tick(seriesCache)`), I can confirm it’s aligned with this shape so you don’t lose time stitching that up.

