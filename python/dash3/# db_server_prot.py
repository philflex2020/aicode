# db_server.py
# RBMS Data Server with DB persistence for profiles, variables, and protections

from fastapi import FastAPI, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Dict, Any, List, Optional
from sqlalchemy import (
    create_engine, Column, Integer, String, Text, JSON, TIMESTAMP, Float, Boolean,
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
    name = Column(String, unique=True, nullable=False)
    build = Column(String, nullable=False)
    version = Column(String, nullable=False)
    target = Column(String, nullable=True)
    __table_args__ = (
        CheckConstraint("build in ('hatteras','warwick','factory')", name="ck_build"),
    )


class ProfileHistory(Base):
    __tablename__ = "profile_history"
    id = Column(Integer, primary_key=True)
    profile_id = Column(Integer, ForeignKey("env_profiles.id"), nullable=False)
    action = Column(String, nullable=False)
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
    mem_area = Column(String, nullable=False)
    mem_type = Column(String, nullable=False)
    offset = Column(String, nullable=False)
    value = Column(Text, nullable=True)
    updated_at = Column(TIMESTAMP, server_default=func.current_timestamp(), onupdate=func.current_timestamp())
    __table_args__ = (
        UniqueConstraint("profile_id", "mem_area", "mem_type", "offset", name="uniq_var_key"),
    )


class TelemetryMeta(Base):
    __tablename__ = "telemetry_meta"
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)
    label = Column(String, nullable=False)
    unit = Column(String, nullable=True)
    category = Column(String, nullable=True)


# NEW: Protection metadata (variable definitions)
class ProtectionMeta(Base):
    __tablename__ = "protection_meta"
    id = Column(Integer, primary_key=True)
    name = Column(String, unique=True, nullable=False)
    display_name = Column(String, nullable=False)
    format = Column(String, nullable=True)  # e.g., "%.2f V"
    unit = Column(String, nullable=True)
    text = Column(Text, nullable=True)  # descriptive help text
    callback = Column(String, nullable=True)  # backend callback function name
    has_max = Column(Boolean, default=True)
    has_min = Column(Boolean, default=True)


# NEW: Protection limits (profile-specific)
class Protection(Base):
    __tablename__ = "protections"
    id = Column(Integer, primary_key=True)
    profile_id = Column(Integer, ForeignKey("env_profiles.id"), nullable=False)
    variable_name = Column(String, nullable=False)  # e.g., "cell_voltage"
    limit_type = Column(String, nullable=False)  # "max" or "min"
    level = Column(String, nullable=False)  # "warning", "alert", "fault"
    threshold = Column(Float, nullable=False)
    on_duration = Column(Float, nullable=False)  # seconds
    off_duration = Column(Float, nullable=False)  # seconds
    enabled = Column(Boolean, default=True)
    updated_at = Column(TIMESTAMP, server_default=func.current_timestamp(), onupdate=func.current_timestamp())
    __table_args__ = (
        UniqueConstraint("profile_id", "variable_name", "limit_type", "level", name="uniq_protection"),
        CheckConstraint("limit_type in ('max','min')", name="ck_limit_type"),
        CheckConstraint("level in ('warning','alert','fault')", name="ck_level"),
    )


# ------------------------------
# Schemas (Pydantic v2)
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


class TelemetryMetaIn(BaseModel):
    name: str
    label: str
    unit: Optional[str] = None
    category: Optional[str] = None


class ProtectionMetaOut(BaseModel):
    name: str
    display_name: str
    format: Optional[str]
    unit: Optional[str]
    text: Optional[str]
    callback: Optional[str]
    has_max: bool
    has_min: bool
    model_config = {"from_attributes": True}


class ProtectionLimitIn(BaseModel):
    threshold: float
    on_duration: float
    off_duration: float
    enabled: bool = True


class ProtectionConfigIn(BaseModel):
    profile: Optional[str] = None
    protections: Dict[str, Dict[str, Dict[str, ProtectionLimitIn]]]
    # e.g., {"cell_voltage": {"max": {"warning": {...}, "alert": {...}}, "min": {...}}}


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
        # Create default profile
        prof = db.query(EnvProfile).filter_by(name="default").first()
        if not prof:
            prof = EnvProfile(name="default", build="hatteras", version="0.3.34", target=None)
            db.add(prof)
            db.flush()
            db.add(ProfileHistory(profile=prof, action="create", data={"build": prof.build, "version": prof.version, "target": prof.target}))
            db.commit()
        
        # Set active profile
        if db.query(MetaKV).filter_by(k="active_profile").first() is None:
            db.add(MetaKV(k="active_profile", v="default"))
            db.commit()
        
        # Seed telemetry_meta if empty
        if db.query(TelemetryMeta).count() == 0:
            seed = [
                TelemetryMeta(name="mbps", label="Throughput (Mb/s)", unit="Mbps", category="network"),
                TelemetryMeta(name="rtt_ms", label="Round-trip (ms)", unit="ms", category="network"),
                TelemetryMeta(name="temp_c", label="Temperature (°C)", unit="°C", category="thermal"),
                TelemetryMeta(name="errors", label="Error Count", unit="count", category="network"),
                TelemetryMeta(name="drops", label="Packet Drops", unit="count", category="network"),
                TelemetryMeta(name="link_state", label="Link Status", unit="", category="network"),
                TelemetryMeta(name="cpu", label="CPU Usage", unit="%", category="system"),
                TelemetryMeta(name="cell_volts", label="Cell Voltage", unit="V", category="power"),
                TelemetryMeta(name="cell_temp", label="Cell Temp", unit="°C", category="thermal"),
                TelemetryMeta(name="cell_soc", label="Cell SOC", unit="%", category="state"),
                TelemetryMeta(name="cell_soh", label="Cell SOH", unit="%", category="state"),
            ]
            db.add_all(seed)
            db.commit()
        
        # Seed protection_meta if empty
        if db.query(ProtectionMeta).count() == 0:
            seed_protections = [
                ProtectionMeta(
                    name="cell_voltage",
                    display_name="Cell Voltage",
                    format="%.2f V",
                    unit="V",
                    text="Cell voltage protection monitors individual cell voltages",
                    callback="on_cell_voltage_alarm",
                    has_max=True,
                    has_min=True
                ),
                ProtectionMeta(
                    name="temperature",
                    display_name="Temperature",
                    format="%.1f °C",
                    unit="°C",
                    text="Temperature protection monitors system thermal limits",
                    callback="on_temperature_alarm",
                    has_max=True,
                    has_min=True
                ),
                ProtectionMeta(
                    name="current",
                    display_name="Current",
                    format="%.1f A",
                    unit="A",
                    text="Current protection monitors charge/discharge current",
                    callback="on_current_alarm",
                    has_max=True,
                    has_min=False
                ),
                ProtectionMeta(
                    name="soc",
                    display_name="State of Charge",
                    format="%.1f %%",
                    unit="%",
                    text="SOC protection monitors battery state of charge",
                    callback="on_soc_alarm",
                    has_max=True,
                    has_min=True
                ),
            ]
            db.add_all(seed_protections)
            db.commit()
        
        # Seed default protections for default profile
        prof = db.query(EnvProfile).filter_by(name="default").first()
        if prof and db.query(Protection).filter_by(profile_id=prof.id).count() == 0:
            default_protections = [
                # Cell Voltage - Max
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="max", level="warning",
                          threshold=4.15, on_duration=5.0, off_duration=10.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="max", level="alert",
                          threshold=4.20, on_duration=3.0, off_duration=15.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="max", level="fault",
                          threshold=4.25, on_duration=1.0, off_duration=20.0, enabled=True),
                # Cell Voltage - Min
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="min", level="warning",
                          threshold=2.70, on_duration=5.0, off_duration=10.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="min", level="alert",
                          threshold=2.65, on_duration=3.0, off_duration=15.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="cell_voltage", limit_type="min", level="fault",
                          threshold=2.60, on_duration=1.0, off_duration=20.0, enabled=True),
                
                # Temperature - Max
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="max", level="warning",
                          threshold=50.0, on_duration=10.0, off_duration=20.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="max", level="alert",
                          threshold=55.0, on_duration=5.0, off_duration=30.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="max", level="fault",
                          threshold=60.0, on_duration=2.0, off_duration=40.0, enabled=True),
                # Temperature - Min
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="min", level="warning",
                          threshold=0.0, on_duration=10.0, off_duration=20.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="min", level="alert",
                          threshold=-5.0, on_duration=5.0, off_duration=30.0, enabled=False),
                Protection(profile_id=prof.id, variable_name="temperature", limit_type="min", level="fault",
                          threshold=-10.0, on_duration=2.0, off_duration=40.0, enabled=False),
                
                # Current - Max only
                Protection(profile_id=prof.id, variable_name="current", limit_type="max", level="warning",
                          threshold=100.0, on_duration=5.0, off_duration=10.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="current", limit_type="max", level="alert",
                          threshold=120.0, on_duration=2.0, off_duration=15.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="current", limit_type="max", level="fault",
                          threshold=150.0, on_duration=1.0, off_duration=20.0, enabled=True),
                
                # SOC
                Protection(profile_id=prof.id, variable_name="soc", limit_type="max", level="warning",
                          threshold=95.0, on_duration=30.0, off_duration=60.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="soc", limit_type="max", level="alert",
                          threshold=98.0, on_duration=15.0, off_duration=90.0, enabled=False),
                Protection(profile_id=prof.id, variable_name="soc", limit_type="min", level="warning",
                          threshold=10.0, on_duration=30.0, off_duration=60.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="soc", limit_type="min", level="alert",
                          threshold=5.0, on_duration=15.0, off_duration=90.0, enabled=True),
                Protection(profile_id=prof.id, variable_name="soc", limit_type="min", level="fault",
                          threshold=2.0, on_duration=5.0, off_duration=120.0, enabled=True),
            ]
            db.add_all(default_protections)
            db.commit()
    
    # Sync dash.json variables
    sync_dash_variables()


def sync_dash_variables():
    """Ensure all variables referenced in dash.json exist in the database"""
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
        
        var_defs = {}
        
        if "controls" in dash and "cards" in dash["controls"]:
            for card in dash["controls"]["cards"]:
                for el in card.get("elements", []):
                    if "id" in el and "default" in el:
                        var_defs[el["id"]] = el["default"]
        
        if "values" in dash and "fields" in dash["values"]:
            for field in dash["values"]["fields"]:
                if field not in var_defs:
                    var_defs[field] = None
        
        for widget in dash.get("widgets", []):
            if widget.get("type") == "vartable" and "names" in widget:
                for name in widget["names"]:
                    if name not in var_defs:
                        var_defs[name] = None
        
        synced = []
        for var_name, default_val in var_defs.items():
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
                    value=str(default_val) if default_val is not None else None
                )
                db.add(row)
                synced.append(f"{var_name}={default_val}")
        
        if synced:
            db.commit()
            print(f"✅ Synced {len(synced)} variables from dash.json")


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


def compute_alarm_state(current_value: float, protections: Dict) -> str:
    """Compute alarm state based on current value and protection limits"""
    if not protections:
        return "normal"
    
    # Check max limits
    if "max" in protections:
        for level in ["fault", "alert", "warning"]:
            if level in protections["max"] and protections["max"][level].get("enabled", True):
                if current_value >= protections["max"][level]["threshold"]:
                    return level
    
    # Check min limits
    if "min" in protections:
        for level in ["fault", "alert", "warning"]:
            if level in protections["min"] and protections["min"][level].get("enabled", True):
                if current_value <= protections["min"][level]["threshold"]:
                    return level
    
    return "normal"


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

        # Clone variables
        vars_src = db.query(Variable).filter_by(profile_id=src.id).all()
        for v in vars_src:
            db.add(Variable(
                profile_id=new_prof.id,
                mem_area=v.mem_area,
                mem_type=v.mem_type,
                offset=v.offset,
                value=v.value
            ))
        
        # Clone protections
        prots_src = db.query(Protection).filter_by(profile_id=src.id).all()
        for p in prots_src:
            db.add(Protection(
                profile_id=new_prof.id,
                variable_name=p.variable_name,
                limit_type=p.limit_type,
                level=p.level,
                threshold=p.threshold,
                on_duration=p.on_duration,
                off_duration=p.off_duration,
                enabled=p.enabled
            ))

        db.add(ProfileHistory(profile=new_prof, action="clone",
                              data={"from": src.name, "version": new_prof.version, "target": new_prof.target}))
        db.commit()
        return ProfileOut.model_validate(new_prof, from_attributes=True)


@app.post("/api/sync_dash")
def sync_dash():
    sync_dash_variables()
    return {"ok": True, "message": "dash.json variables synced to database"}


# ------------------------------
# Protection endpoints
# ------------------------------
@app.get("/api/protection_meta", response_model=List[ProtectionMetaOut])
def get_protection_meta():
    """Get list of available protection variables with metadata"""
    with SessionLocal() as db:
        rows = db.query(ProtectionMeta).order_by(ProtectionMeta.name).all()
        return [ProtectionMetaOut.model_validate(r, from_attributes=True) for r in rows]


@app.get("/api/protections")
def get_protections(source: str = "current", profile: Optional[str] = None):
    """
    Get protection configuration with current values and alarm states.
    source: "current" (from DB) or "default" (factory defaults)
    """
    with SessionLocal() as db:
        if profile:
            prof = db.query(EnvProfile).filter_by(name=profile).first()
            if not prof:
                raise HTTPException(404, f"Profile '{profile}' not found")
        else:
            prof = get_active_profile(db)
        
        # Get metadata
        meta_rows = db.query(ProtectionMeta).all()
        meta_dict = {m.name: m for m in meta_rows}
        
        # Get protections
        if source == "default":
            # Return factory defaults (from default profile)
            default_prof = db.query(EnvProfile).filter_by(name="default").first()
            if not default_prof:
                raise HTTPException(404, "Default profile not found")
            prot_rows = db.query(Protection).filter_by(profile_id=default_prof.id).all()
        else:
            prot_rows = db.query(Protection).filter_by(profile_id=prof.id).all()
        
        # Organize by variable -> limit_type -> level
        result = {}
        for var_name, meta in meta_dict.items():
            result[var_name] = {
                "meta": {
                    "display_name": meta.display_name,
                    "format": meta.format,
                    "unit": meta.unit,
                    "text": meta.text,
                    "callback": meta.callback,
                    "has_max": meta.has_max,
                    "has_min": meta.has_min,
                },
                "limits": {},
                "current_value": None,
                "current_state": "normal"
            }
        
        for p in prot_rows:
            if p.variable_name not in result:
                continue
            
            if p.limit_type not in result[p.variable_name]["limits"]:
                result[p.variable_name]["limits"][p.limit_type] = {}
            
            result[p.variable_name]["limits"][p.limit_type][p.level] = {
                "threshold": p.threshold,
                "on_duration": p.on_duration,
                "off_duration": p.off_duration,
                "enabled": p.enabled
            }
        
        # Generate mock current values and compute states
        for var_name in result:
            if var_name == "cell_voltage":
                current_val = round(3.20 + 0.95 * random.random(), 2)
            elif var_name == "temperature":
                current_val = round(25.0 + 20.0 * random.random(), 1)
            elif var_name == "current":
                current_val = round(50.0 + 50.0 * random.random(), 1)
            elif var_name == "soc":
                current_val = round(50.0 + 40.0 * random.random(), 1)
            else:
                current_val = 0.0
            
            result[var_name]["current_value"] = current_val
            result[var_name]["current_state"] = compute_alarm_state(
                current_val,
                result[var_name]["limits"]
            )
        
        return {
            "ok": True,
            "profile": prof.name,
            "source": source,
            "protections": result
        }


@app.post("/api/protections")
def save_protections(config: ProtectionConfigIn):
    """Save protection configuration to profile (does not deploy)"""
    with SessionLocal() as db:
        if config.profile:
            prof = db.query(EnvProfile).filter_by(name=config.profile).first()
            if not prof:
                raise HTTPException(404, f"Profile '{config.profile}' not found")
        else:
            prof = get_active_profile(db)
        
        # Delete existing protections for this profile
        db.query(Protection).filter_by(profile_id=prof.id).delete()
        
        # Insert new protections
        for var_name, limits in config.protections.items():
            for limit_type, levels in limits.items():
                for level, data in levels.items():
                    db.add(Protection(
                        profile_id=prof.id,
                        variable_name=var_name,
                        limit_type=limit_type,
                        level=level,
                        threshold=data.threshold,
                        on_duration=data.on_duration,
                        off_duration=data.off_duration,
                        enabled=data.enabled
                    ))
        
        db.commit()
        return {"ok": True, "profile": prof.name, "message": "Protections saved"}


@app.post("/api/protections/deploy")
def deploy_protections(config: ProtectionConfigIn):
    """Save and deploy protection configuration to target system"""
    # First save to database
    save_result = save_protections(config)
    
    # TODO: Add actual deployment logic here (send to BMS hardware, etc.)
    # For now, just return success
    
    return {
        "ok": True,
        "profile": save_result["profile"],
        "message": "Protections saved and deployed",
        "deployed": True
    }


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


# ------------------------------
# Variable endpoints
# ------------------------------
@app.post("/vars")
async def update_vars(request: Request, profile: Optional[str] = None):
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


@app.get("/vars")
def get_vars(names: str, rack: int = 0, profile: Optional[str] = None):
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

            val = STATE.get(nm)
            out[nm] = val
            db.add(Variable(profile_id=prof.id,
                            mem_area=default_area,
                            mem_type=default_type,
                            offset=nm,
                            value=str(val) if val is not None else None))
            db.commit()

    return out


# ------------------------------
# Metrics and series
# ------------------------------
@app.get("/metrics")
def metrics(profile: Optional[str] = None):
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


@app.get("/series")
def get_series(
    names: Optional[str] = None,
    category: Optional[str] = None,
    window: int = 300,
    profile: Optional[str] = None,
):
    with SessionLocal() as db:
        if not names:
            query = db.query(TelemetryMeta)
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

        now = int(time.time())
        name_list = [n.strip() for n in names.split(",") if n.strip()]

        query = db.query(TelemetryMeta.name)
        if category:
            categories = [c.strip() for c in category.split(",") if c.strip()]
            query = query.filter(TelemetryMeta.category.in_(categories))
        known = {r.name for r in query.all()}
        
        series: Dict[str, List[List[float]]] = {}

        for nm in name_list:
            if nm not in known:
                print(f"Unknown telemetry name: {nm}")
                continue

            pts: List[List[float]] = []

            # Special: synthetic array-type telemetry for cells
            if nm in ("cell_volts", "cell_temp", "cell_soc", "cell_soh"):
                TOTAL = 480

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

                pts.append([now, arr])
                series[nm] = pts
                continue

            # Normal scalar time-series
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


@app.post("/series")
def create_series(meta: TelemetryMetaIn):
    with SessionLocal() as db:
        row = db.query(TelemetryMeta).filter_by(name=meta.name).first()
        if row:
            row.label = meta.label
            row.unit = meta.unit
            row.category = meta.category
            action = "updated"
        else:
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


# ------------------------------
# Entry point
# ------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RBMS DB Data Server")
    parser.add_argument("--host", default=os.environ.get("HOST", "0.0.0.0"), help="Host to bind")
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "8084")), help="Port to bind")
    parser.add_argument("--reload", action="store_true", help="Enable uvicorn reload")
    args = parser.parse_args()

    uvicorn.run("db_server_prot:app", host=args.host, port=args.port, reload=args.reload)