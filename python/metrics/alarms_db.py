# alarms_db.py
import os
import glob
import shutil
from typing import List, Dict, Any
from datetime import datetime, timedelta
import random

from fastapi import FastAPI, HTTPException, Depends, Body, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi.responses import HTMLResponse
from pydantic import BaseModel
from sqlalchemy import create_engine, Column, Integer, String, Text, DateTime
from sqlalchemy.orm import sessionmaker, Session
from sqlalchemy.ext.declarative import declarative_base

# Database Models
Base = declarative_base()

class AlarmEntry(Base):
    __tablename__ = "alarm_entries"
    id = Column(Integer, primary_key=True, index=True)
    timestamp = Column(DateTime, index=True)
    alarm_data = Column(Text)  # JSON string of alarm states

class LimitEntry(Base):
    __tablename__ = "limit_entries"
    id = Column(Integer, primary_key=True, index=True)
    alarm_name = Column(String, unique=True, index=True)
    limit_data = Column(Text)  # JSON string of limit configuration

# Pydantic Models
class AlarmData(BaseModel):
    time: str
    alarms: Dict[str, int]

class AlarmsResponse(BaseModel):
    id: str
    type: str
    caption: str
    version: str
    data: List[AlarmData]

# Globals for DB management
current_db_name = None
engine = None
SessionLocal = None

def get_database_files():
    """Get list of .sqlite files in current directory"""
    return glob.glob("*.sqlite")

def select_database():
    """Prompt user to select or create a database"""
    db_files = get_database_files()
    print("\n=== Alarm Data Database Selector ===")
    if db_files:
        print("Existing databases found:")
        for i, db_file in enumerate(db_files, 1):
            print(f"  {i}. {db_file}")
        print(f"  {len(db_files) + 1}. Create new database")
    else:
        print("No existing databases found.")
        print("Creating new database...")
        return "alarms.sqlite"

    while True:
        try:
            choice = input(f"\nSelect database (1-{len(db_files) + 1}): ").strip()
            choice_num = int(choice)
            if 1 <= choice_num <= len(db_files):
                return db_files[choice_num - 1]
            elif choice_num == len(db_files) + 1:
                new_name = input("Enter new database filename: ").strip()
                if not new_name:
                    print("Filename cannot be empty!")
                    continue
                if not new_name.endswith(('.sqlite', '.db')):
                    new_name += '.sqlite'
                return new_name
            else:
                print(f"Please enter a number between 1 and {len(db_files) + 1}")
        except ValueError:
            print("Please enter a valid number!")

def switch_database(dbname: str):
    global current_db_name, engine, SessionLocal
    if not dbname.endswith(('.sqlite', '.db')):
        dbname += '.sqlite'
    current_db_name = dbname
    db_url = f"sqlite:///{current_db_name}"
    if engine:
        engine.dispose()
    engine = create_engine(db_url, connect_args={"check_same_thread": False})
    SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
    Base.metadata.create_all(bind=engine)
    print(f"Switched to database: {current_db_name}")

# Initialize DB on startup
current_db_name = select_database()
switch_database(current_db_name)

# Dependency
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# FastAPI App
app = FastAPI(title="Alarm Data API with Database Persistence and DB Management")

# CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Static files and templates
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

# HTML Routes
@app.get("/", response_class=HTMLResponse)
async def root():
    return "<h1>Welcome to the Alarm/Metrics API</h1><p>Go to <a href='/alarms'>/alarms</a> or <a href='/metrics'>/metrics</a></p>"

@app.get("/alarms", response_class=HTMLResponse)
async def alarms_page(request: Request):
    return templates.TemplateResponse("alarms.html", {"request": request})

@app.get("/metrics", response_class=HTMLResponse)
async def metrics_page(request: Request):
    return templates.TemplateResponse("metrics.html", {"request": request})

# API Routes
@app.get("/api/alarms", response_model=AlarmsResponse)
def get_alarms(db: Session = Depends(get_db)):
    entries = db.query(AlarmEntry).order_by(AlarmEntry.timestamp).all()
    if not entries:
        # Generate sample data and save
        alarms = ['alarm1', 'alarm2', 'alarm3', 'alarm4', 'alarm5']
        now = datetime.utcnow()
        data = []
        for i in range(10):
            time = (now + timedelta(minutes=i)).isoformat() + "Z"
            alarm_states = {alarm: random.randint(0, 3) for alarm in alarms}
            data.append({"time": time, "alarms": alarm_states})
            db_entry = AlarmEntry(
                timestamp=datetime.fromisoformat(time.rstrip('Z')),
                alarm_data=str(alarm_states)
            )
            db.add(db_entry)
        db.commit()
        response_data = [AlarmData(**item) for item in data]
    else:
        response_data = []
        for entry in entries:
            alarm_dict = eval(entry.alarm_data)
            response_data.append(AlarmData(
                time=entry.timestamp.isoformat() + "Z",
                alarms=alarm_dict
            ))
    return AlarmsResponse(
        id="tab1",
        type="alarms",
        caption=f"Alarm Data from {current_db_name}",
        version="1",
        data=response_data
    )

@app.post("/api/alarms")
def post_alarms(alarm_data: List[AlarmData], db: Session = Depends(get_db)):
    try:
        for item in alarm_data:
            timestamp = datetime.fromisoformat(item.time.rstrip('Z'))
            db_entry = AlarmEntry(
                timestamp=timestamp,
                alarm_data=str(item.alarms)
            )
            db.add(db_entry)
        db.commit()
        return {"status": "success", "message": f"Added {len(alarm_data)} alarm entries"}
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/limits")
def get_limits(db: Session = Depends(get_db)):
    entries = db.query(LimitEntry).all()
    limits_dict = {}
    for entry in entries:
        limits_dict[entry.alarm_name] = eval(entry.limit_data)
    if not limits_dict:
        default_alarms = ['alarm1', 'alarm2', 'alarm3', 'alarm4', 'alarm5']
        for alarm in default_alarms:
            default_limit = {
                "level1": {"time": 10, "offtime": 5, "action": "Notify", "value": 100, "hysteresis": 5},
                "level2": {"time": 20, "offtime": 10, "action": "Log", "value": 200, "hysteresis": 10},
                "level3": {"time": 30, "offtime": 15, "action": "Shutdown", "value": 300, "hysteresis": 15}
            }
            limits_dict[alarm] = default_limit
            db_entry = LimitEntry(
                alarm_name=alarm,
                limit_data=str(default_limit)
            )
            db.add(db_entry)
        db.commit()
    return limits_dict

@app.post("/api/limits")
def post_limits(limits: Dict[str, Any], db: Session = Depends(get_db)):
    try:
        for alarm_name, limit_data in limits.items():
            existing_entry = db.query(LimitEntry).filter(LimitEntry.alarm_name == alarm_name).first()
            if existing_entry:
                existing_entry.limit_data = str(limit_data)
            else:
                new_entry = LimitEntry(
                    alarm_name=alarm_name,
                    limit_data=str(limit_data)
                )
                db.add(new_entry)
        db.commit()
        return {"status": "success", "updated": list(limits.keys())}
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=str(e))

# Database management endpoints
@app.post("/api/db/switch")
def api_switch_db(dbname: str = Body(..., embed=True)):
    try:
        switch_database(dbname)
        return {"status": "success", "message": f"Switched to database {dbname}"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/db/create")
def api_create_db(dbname: str = Body(..., embed=True)):
    try:
        if not dbname.endswith(('.sqlite', '.db')):
            dbname += '.sqlite'
        if os.path.exists(dbname):
            raise HTTPException(status_code=400, detail="Database already exists")
        open(dbname, 'a').close()
        switch_database(dbname)
        return {"status": "success", "message": f"Created and switched to new database {dbname}"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/db/copy")
def api_copy_db(source: str = Body(...), target: str = Body(...)):
    try:
        if not os.path.exists(source):
            raise HTTPException(status_code=400, detail="Source database does not exist")
        if os.path.exists(target):
            raise HTTPException(status_code=400, detail="Target database already exists")
        shutil.copyfile(source, target)
        switch_database(target)
        return {"status": "success", "message": f"Copied {source} to {target} and switched to it"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/health")
def health_check():
    return {"status": "healthy", "database": current_db_name}

if __name__ == "__main__":
    import uvicorn
    print(f"Starting Alarm Data API with database: {current_db_name}")
    uvicorn.run(app, host="0.0.0.0", port=8011)